/*
 * dhewm3 Android entry point.
 *
 * Game data lives inside the APK as assets/base/ (pak files).
 * dhewm3 uses plain POSIX fopen(), so on first launch we extract
 * assets/base/ into the app internal storage, then point
 * fs_basepath / fs_savepath there.
 *
 * Layout after extraction:
 *   <internal>/base/pak000.pk4
 *   <internal>/base/pak001.pk4
 *   ...
 *   <internal>/base/.extracted   (marker file)
 */
#include <SDL.h>
#include <SDL_system.h>

#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "framework/Common.h"
#include "framework/CmdSystem.h"
#include "sys/sys_public.h"
#include "sys/sys_local.h"
#include "sys/posix/posix_public.h"
#include "sys/android/android_touch.h"

/* idlib poisons snprintf -> use_idStr_snPrintf; use idStr::snPrintf instead. */
#undef snprintf
#undef vsnprintf

#define D3A_TAG "Dhewm3Android"
#define D3A_LOGI(...) __android_log_print(ANDROID_LOG_INFO, D3A_TAG, __VA_ARGS__)
#define D3A_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, D3A_TAG, __VA_ARGS__)
#define D3A_LOGW(...) __android_log_print(ANDROID_LOG_WARN, D3A_TAG, __VA_ARGS__)

static const char *kExtractMarker = ".extracted";

static bool PathExists(const char *path) {
	struct stat st;
	return path && path[0] && (stat(path, &st) == 0);
}

static bool MakeDirs(const char *path) {
	char tmp[512];
	idStr::snPrintf(tmp, sizeof(tmp), "%s", path);
	size_t len = strlen(tmp);
	if (len == 0) {
		return false;
	}
	if (tmp[len - 1] == '/') {
		tmp[len - 1] = '\0';
	}
	for (char *p = tmp + 1; *p; ++p) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0770) != 0 && errno != EEXIST) {
				return false;
			}
			*p = '/';
		}
	}
	if (mkdir(tmp, 0770) != 0 && errno != EEXIST) {
		return false;
	}
	return true;
}

/* ---------------------------------------------------------------------
 * Platform functions normally provided by sys/linux/main.cpp
 * --------------------------------------------------------------------- */

static const char *Android_InternalPath() {
	const char *p = SDL_AndroidGetInternalStoragePath();
	return (p && p[0]) ? p : "/data/local/tmp";
}

void Sys_Shutdown() {
}

int Sys_GetSystemRam() {
	long pages = sysconf(_SC_PHYS_PAGES);
	long psize = sysconf(_SC_PAGE_SIZE);
	if (pages <= 0 || psize <= 0) {
		return 512;
	}
	return (int)(((long long)pages * (long long)psize) / (1024 * 1024)); /* MB */
}

void Sys_DoStartProcess(const char *exeName, bool dofork) {
	D3A_LOGW("Sys_DoStartProcess(%s) ignored on Android", exeName ? exeName : "");
}

void idSysLocal::OpenURL(const char *url, bool quit) {
	D3A_LOGI("OpenURL: %s", url ? url : "");
	if (url) {
		SDL_OpenURL(url);
	}
	if (quit) {
		cmdSystem->BufferCommandText(CMD_EXEC_APPEND, "quit\n");
	}
}

const char *Posix_GetSavePath() {
	return Android_InternalPath();
}

bool Sys_GetPath(sysPath_t type, idStr &path) {
	const char *base = Android_InternalPath();

	switch (type) {
		case PATH_BASE:
		case PATH_CONFIG:
		case PATH_SAVE:
		case PATH_EXE:
			path = base;
			return true;
		default:
			return false;
	}
}

/* --------------------------------------------------------------------- */

static AAssetManager *Android_GetAssetManager() {
	JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
	if (!env) {
		D3A_LOGE("SDL_AndroidGetJNIEnv() failed");
		return NULL;
	}

	jobject activity = (jobject)SDL_AndroidGetActivity();
	if (!activity) {
		D3A_LOGE("SDL_AndroidGetActivity() failed");
		return NULL;
	}

	jclass activityClass = env->GetObjectClass(activity);
	jmethodID getAssets = env->GetMethodID(activityClass, "getAssets",
	                                       "()Landroid/content/res/AssetManager;");
	if (!getAssets) {
		D3A_LOGE("getAssets() method not found");
		env->DeleteLocalRef(activityClass);
		env->DeleteLocalRef(activity);
		return NULL;
	}

	jobject assetManager = env->CallObjectMethod(activity, getAssets);
	env->DeleteLocalRef(activityClass);
	env->DeleteLocalRef(activity);

	if (!assetManager) {
		D3A_LOGE("getAssets() returned null");
		return NULL;
	}

	return AAssetManager_fromJava(env, assetManager);
}

static bool Android_ExtractAssetFile(AAssetManager *mgr, const char *assetPath,
                                     const char *destRoot) {
	AAsset *asset = AAssetManager_open(mgr, assetPath, AASSET_MODE_STREAMING);
	if (!asset) {
		D3A_LOGW("Cannot open asset file: %s", assetPath);
		return false;
	}

	char destPath[768];
	idStr::snPrintf(destPath, sizeof(destPath), "%s/%s", destRoot, assetPath);

	char parent[768];
	idStr::snPrintf(parent, sizeof(parent), "%s", destPath);
	char *slash = strrchr(parent, '/');
	if (slash) {
		*slash = '\0';
		if (!MakeDirs(parent)) {
			D3A_LOGE("MakeDirs(%s) failed: %s", parent, strerror(errno));
			AAsset_close(asset);
			return false;
		}
	}

	FILE *out = fopen(destPath, "wb");
	if (!out) {
		D3A_LOGE("fopen(%s) failed: %s", destPath, strerror(errno));
		AAsset_close(asset);
		return false;
	}

	char buf[8192];
	int readBytes;
	off_t total = 0;
	while ((readBytes = AAsset_read(asset, buf, sizeof(buf))) > 0) {
		if (fwrite(buf, 1, (size_t)readBytes, out) != (size_t)readBytes) {
			D3A_LOGE("fwrite(%s) failed", destPath);
			fclose(out);
			AAsset_close(asset);
			return false;
		}
		total += readBytes;
	}
	fclose(out);
	AAsset_close(asset);
	D3A_LOGI("Extracted %s (%ld bytes)", assetPath, (long)total);
	return true;
}

static bool Android_EnsureGameDataExtracted(const char *internalPath) {
	char baseDir[512];
	char markerPath[512];
	char pakCheck[512];

	idStr::snPrintf(baseDir, sizeof(baseDir), "%s/base", internalPath);
	idStr::snPrintf(markerPath, sizeof(markerPath), "%s/base/%s", internalPath, kExtractMarker);
	idStr::snPrintf(pakCheck, sizeof(pakCheck), "%s/base/pak000.pk4", internalPath);

	if (PathExists(markerPath) && PathExists(pakCheck)) {
		D3A_LOGI("Game data already extracted at %s", baseDir);
		return true;
	}

	D3A_LOGI("Extracting assets/base -> %s ...", baseDir);
	D3A_LOGI("(first launch may take a while - pak files are large)");

	AAssetManager *mgr = Android_GetAssetManager();
	if (!mgr) {
		return false;
	}

	if (!MakeDirs(baseDir)) {
		D3A_LOGE("Cannot create %s: %s", baseDir, strerror(errno));
		return false;
	}

	AAssetDir *dir = AAssetManager_openDir(mgr, "base");
	if (!dir) {
		D3A_LOGE("assets/base/ not found inside the APK");
		D3A_LOGE("Put pak000.pk4 etc. into app/src/main/assets/base/");
		return false;
	}

	bool ok = true;
	int count = 0;
	const char *entry;
	while ((entry = AAssetDir_getNextFileName(dir)) != NULL) {
		char assetPath[768];
		idStr::snPrintf(assetPath, sizeof(assetPath), "base/%s", entry);
		D3A_LOGI("Extracting %s ...", assetPath);
		if (!Android_ExtractAssetFile(mgr, assetPath, internalPath)) {
			D3A_LOGE("Failed to extract %s", assetPath);
			ok = false;
			break;
		}
		++count;
	}
	AAssetDir_close(dir);

	if (!ok || count == 0) {
		D3A_LOGE("Extraction failed or assets/base is empty (files=%d)", count);
		return false;
	}

	FILE *marker = fopen(markerPath, "wb");
	if (marker) {
		fprintf(marker, "ok\n");
		fclose(marker);
	}

	D3A_LOGI("Extracted %d files from assets/base", count);

	if (!PathExists(pakCheck)) {
		D3A_LOGE("pak000.pk4 missing after extraction - check assets/base/");
		return false;
	}
	return true;
}

static bool Android_SetWorkingDirectory(const char *path) {
	if (!path || !path[0]) {
		return false;
	}
	if (mkdir(path, 0770) != 0 && errno != EEXIST) {
		D3A_LOGE("mkdir(%s) failed: %d (%s)", path, errno, strerror(errno));
		return false;
	}
	if (chdir(path) != 0) {
		D3A_LOGE("chdir(%s) failed: %d (%s)", path, errno, strerror(errno));
		return false;
	}
	D3A_LOGI("Working directory: %s", path);
	return true;
}

extern "C" int SDL_main(int argc, char **argv) {
	D3A_LOGI("================================");
	D3A_LOGI("dhewm3 Android startup");
	D3A_LOGI("================================");

	const char *internal = SDL_AndroidGetInternalStoragePath();
	if (!internal || !internal[0]) {
		D3A_LOGE("SDL_AndroidGetInternalStoragePath() is empty");
		return 1;
	}

	if (!Android_SetWorkingDirectory(internal)) {
		return 1;
	}

	if (!Android_EnsureGameDataExtracted(internal)) {
		D3A_LOGE("Game data is not available. Aborting.");
		return 1;
	}

	const int kMaxArgs = 96;
	char *engineArgv[kMaxArgs];
	int engineArgc = 0;

	static char arg_set1[] = "+set";
	static char arg_fs_basepath[] = "fs_basepath";
	static char arg_set2[] = "+set";
	static char arg_fs_savepath[] = "fs_savepath";
	static char arg_basePath[512];
	static char arg_savePath[512];

	idStr::snPrintf(arg_basePath, sizeof(arg_basePath), "%s", internal);
	idStr::snPrintf(arg_savePath, sizeof(arg_savePath), "%s", internal);

	engineArgv[engineArgc++] = arg_set1;
	engineArgv[engineArgc++] = arg_fs_basepath;
	engineArgv[engineArgc++] = arg_basePath;

	engineArgv[engineArgc++] = arg_set2;
	engineArgv[engineArgc++] = arg_fs_savepath;
	engineArgv[engineArgc++] = arg_savePath;

	/* No tty on Android; write dhewm3log.txt; mirror engine output to logcat via posix_main. */
	static char arg_set3[] = "+set";
	static char arg_in_tty[] = "in_tty";
	static char arg_in_tty_val[] = "0";
	engineArgv[engineArgc++] = arg_set3;
	engineArgv[engineArgc++] = arg_in_tty;
	engineArgv[engineArgc++] = arg_in_tty_val;

	static char arg_set4[] = "+set";
	static char arg_log[] = "logFile";
	static char arg_log_val[] = "2";
	engineArgv[engineArgc++] = arg_set4;
	engineArgv[engineArgc++] = arg_log;
	engineArgv[engineArgc++] = arg_log_val;

	/* Fullscreen at native resolution, 16:9 aspect (reduces black bars) */
	static char arg_set5[] = "+set";
	static char arg_fs[] = "r_fullscreen";
	static char arg_fs_val[] = "1";
	engineArgv[engineArgc++] = arg_set5;
	engineArgv[engineArgc++] = arg_fs;
	engineArgv[engineArgc++] = arg_fs_val;

	static char arg_set6[] = "+set";
	static char arg_mode[] = "r_mode";
	static char arg_mode_val[] = "-1";
	engineArgv[engineArgc++] = arg_set6;
	engineArgv[engineArgc++] = arg_mode;
	engineArgv[engineArgc++] = arg_mode_val;

	/*
	 * Mobile render resolution: use native aspect but CAP the long side.
	 * Native 1080p/1200p through GL4ES is too heavy on mid-range SoCs
	 * (e.g. Redmi Note 13). 960 on the long side ≈ stable 40–60 FPS.
	 * Change kMaxLongSide to 1280 for stronger phones, 720 for weak ones.
	 */
	int dw = 1280, dh = 720;
	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
		dw = dm.w;
		dh = dm.h;
	}
	const int kMaxLongSide = 960;
	if (dw >= dh) {
		if (dw > kMaxLongSide) {
			dh = (int)((long)dh * kMaxLongSide / dw);
			dw = kMaxLongSide;
		}
	} else {
		if (dh > kMaxLongSide) {
			dw = (int)((long)dw * kMaxLongSide / dh);
			dh = kMaxLongSide;
		}
	}
	if (dw < 320) dw = 320;
	if (dh < 240) dh = 240;

	static char arg_set7[] = "+set";
	static char arg_cw[] = "r_customWidth";
	static char arg_cw_val[16];
	idStr::snPrintf(arg_cw_val, sizeof(arg_cw_val), "%d", dw);
	engineArgv[engineArgc++] = arg_set7;
	engineArgv[engineArgc++] = arg_cw;
	engineArgv[engineArgc++] = arg_cw_val;

	static char arg_set8[] = "+set";
	static char arg_ch[] = "r_customHeight";
	static char arg_ch_val[16];
	idStr::snPrintf(arg_ch_val, sizeof(arg_ch_val), "%d", dh);
	engineArgv[engineArgc++] = arg_set8;
	engineArgv[engineArgc++] = arg_ch;
	engineArgv[engineArgc++] = arg_ch_val;

	static char arg_set9[] = "+set";
	static char arg_ar[] = "r_aspectRatio";
	static char arg_ar_val[] = "1"; /* 16:9-ish; custom size overrides */
	engineArgv[engineArgc++] = arg_set9;
	engineArgv[engineArgc++] = arg_ar;
	engineArgv[engineArgc++] = arg_ar_val;

	/*
	 * Performance preset for GL4ES + mid-range Adreno/Mali.
	 * Biggest wins: no stencil shadows, no MSAA, downsized textures.
	 * Strings must be static — engine keeps the pointers.
	 */
	struct SetArg { const char *cvar; const char *val; };
	static const SetArg kMobile[] = {
		/* video / AA */
		{ "r_multiSamples",              "0" },
		{ "r_displayRefresh",            "0" },
		/* shadows — stencil shadows kill mobile GPUs */
		{ "r_shadows",                   "0" },
		{ "r_useOptimizedShadows",       "1" },
		{ "r_useShadowCulling",          "1" },
		{ "r_useCachedShadows",          "1" },
		/* expensive material features */
		{ "r_skipBump",                  "0" },  /* keep bump — looks bad without */
		{ "r_skipSpecular",              "1" },
		{ "r_skipNewAmbient",            "0" },
		{ "r_useSimpleLightId",          "1" },
		/* textures */
		{ "image_usePrecompressedTextures", "1" },
		{ "image_useNormalCompression",  "2" },
		{ "image_useAllFormats",         "1" },
		{ "image_downSize",              "1" },
		{ "image_forceDownSize",         "1" },
		{ "image_downSizeLimit",         "256" },
		{ "image_downSizeBump",          "1" },
		{ "image_downSizeBumpLimit",     "256" },
		{ "image_downSizeSpecular",      "1" },
		{ "image_downSizeSpecularLimit", "64" },
		{ "image_lodbias",               "0.5" },
		/* system / sound */
		{ "com_machineSpec",             "0" },
		{ "com_videoRam",                "256" },
		{ "s_clipVolumes",               "0" },
		{ "s_maxSoundsPerShader",        "2" },
		{ "s_numberOfSpeakers",          "2" },
		/* gameplay FX that cost CPU */
		{ "g_bloodEffects",              "0" },
		{ "g_decals",                    "1" },
		{ "g_showBrass",                 "0" },
		{ "r_useEntityCallbacks",        "1" },
	};

	for (unsigned i = 0; i < sizeof(kMobile) / sizeof(kMobile[0]); ++i) {
		if (engineArgc + 3 >= kMaxArgs) break;
		/* each +set needs durable storage — use static pool */
		static char setPool[64][8];
		static char cvarPool[64][48];
		static char valPool[64][16];
		if (i >= 64) break;
		idStr::snPrintf(setPool[i], sizeof(setPool[i]), "%s", "+set");
		idStr::snPrintf(cvarPool[i], sizeof(cvarPool[i]), "%s", kMobile[i].cvar);
		idStr::snPrintf(valPool[i], sizeof(valPool[i]), "%s", kMobile[i].val);
		engineArgv[engineArgc++] = setPool[i];
		engineArgv[engineArgc++] = cvarPool[i];
		engineArgv[engineArgc++] = valPool[i];
	}

	for (int i = 1; i < argc && engineArgc < kMaxArgs - 1; ++i) {
		engineArgv[engineArgc++] = argv[i];
	}

	D3A_LOGI("fs_basepath=%s", arg_basePath);
	D3A_LOGI("fs_savepath=%s", arg_savePath);
	D3A_LOGI("video %dx%d fullscreen (mobile preset, shadows off, tex<=256)", dw, dh);
	D3A_LOGI("Calling common->Init with %d args", engineArgc);

	/* Touch UI is Java overlay + JNI — do not init C++ GL HUD */
	common->Init(engineArgc, engineArgv);

	for (;;) {
		common->Frame();
	}

	return 0;
}
