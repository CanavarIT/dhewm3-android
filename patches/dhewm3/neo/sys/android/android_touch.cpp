/*
 * Android virtual touch controls for dhewm3.
 */
#include "sys/android/android_touch.h"

#include <SDL.h>
#include <android/log.h>
#include <math.h>
#include <string.h>

#include "sys/platform.h"
#include "framework/KeyInput.h"
#include "renderer/qgl.h"

#define ATAG "D3Touch"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, ATAG, __VA_ARGS__)

/* Key codes used by dhewm3 (from KeyInput.h / keys) */

/* -----------------------------------------------------------------------
 * Geometry (fraction of screen, landscape assumed)
 * -------------------------------------------------------------------- */

struct TouchRect {
	float x0, y0, x1, y1; /* normalized 0..1, origin top-left */
};

static const TouchRect kStickBase  = { 0.02f, 0.55f, 0.32f, 0.98f };
static const TouchRect kLookZone   = { 0.40f, 0.15f, 0.98f, 0.85f };
static const TouchRect kFireBtn    = { 0.78f, 0.70f, 0.97f, 0.96f };
static const TouchRect kJumpBtn    = { 0.62f, 0.72f, 0.76f, 0.92f };
static const TouchRect kUseBtn     = { 0.18f, 0.82f, 0.30f, 0.96f };
static const TouchRect kCrouchBtn  = { 0.04f, 0.82f, 0.16f, 0.96f };
static const TouchRect kWepPrev    = { 0.02f, 0.04f, 0.12f, 0.14f };
static const TouchRect kWepNext    = { 0.13f, 0.04f, 0.23f, 0.14f };
static const TouchRect kPauseBtn   = { 0.88f, 0.04f, 0.98f, 0.14f };

/* -----------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------- */

enum FingerRole {
	FINGER_NONE = 0,
	FINGER_STICK,
	FINGER_LOOK,
	FINGER_FIRE,
	FINGER_JUMP,
	FINGER_USE,
	FINGER_CROUCH,
	FINGER_WEPPREV,
	FINGER_WEPNEXT,
	FINGER_PAUSE
};

struct FingerState {
	bool active;
	FingerRole role;
	float x, y;       /* normalized */
	float startX, startY;
};

static const int kMaxFingers = 10;
static FingerState gFingers[kMaxFingers];

static float gStickDX = 0.f; /* -1..1 */
static float gStickDY = 0.f;
static float gLookDX = 0.f;  /* pixels this frame */
static float gLookDY = 0.f;
static float gLookSens = 2.8f;

static bool gKeyForward = false, gKeyBack = false, gKeyLeft = false, gKeyRight = false;
static bool gKeyFire = false, gKeyJump = false, gKeyUse = false, gKeyCrouch = false;
static bool gKeyWepPrev = false, gKeyWepNext = false, gKeyPause = false;

/* previous key state for edge detection into kbd/mouse polls */
static bool gPrevForward, gPrevBack, gPrevLeft, gPrevRight;
static bool gPrevFire, gPrevJump, gPrevUse, gPrevCrouch;
static bool gPrevWepPrev, gPrevWepNext, gPrevPause;

static int gScreenW = 1, gScreenH = 1;
static bool gInited = false;

/* Forward decls of poll helpers from events.cpp */
extern void Android_Touch_PushKey(int key, bool down);
extern void Android_Touch_PushMouseDelta(int dx, int dy);
extern void Android_Touch_PushMouseButton(int button /*1=left*/, bool down);

/* ----------------------------------------------------------------------- */

static bool InRect(const TouchRect &r, float x, float y) {
	return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
}

static FingerRole RoleForPoint(float x, float y) {
	if (InRect(kFireBtn, x, y))   return FINGER_FIRE;
	if (InRect(kJumpBtn, x, y))   return FINGER_JUMP;
	if (InRect(kUseBtn, x, y))    return FINGER_USE;
	if (InRect(kCrouchBtn, x, y)) return FINGER_CROUCH;
	if (InRect(kWepPrev, x, y))   return FINGER_WEPPREV;
	if (InRect(kWepNext, x, y))   return FINGER_WEPNEXT;
	if (InRect(kPauseBtn, x, y))  return FINGER_PAUSE;
	if (InRect(kStickBase, x, y)) return FINGER_STICK;
	if (InRect(kLookZone, x, y))  return FINGER_LOOK;
	return FINGER_NONE;
}

static void UpdateStickFromFinger(const FingerState &f) {
	float cx = (kStickBase.x0 + kStickBase.x1) * 0.5f;
	float cy = (kStickBase.y0 + kStickBase.y1) * 0.5f;
	float rw = (kStickBase.x1 - kStickBase.x0) * 0.5f;
	float rh = (kStickBase.y1 - kStickBase.y0) * 0.5f;
	float dx = (f.x - cx) / (rw > 0.01f ? rw : 0.01f);
	float dy = (f.y - cy) / (rh > 0.01f ? rh : 0.01f);
	if (dx > 1.f) dx = 1.f; if (dx < -1.f) dx = -1.f;
	if (dy > 1.f) dy = 1.f; if (dy < -1.f) dy = -1.f;
	/* deadzone */
	const float dead = 0.18f;
	if (fabsf(dx) < dead) dx = 0.f;
	if (fabsf(dy) < dead) dy = 0.f;
	gStickDX = dx;
	gStickDY = dy;
}

static void RecomputeHeldKeys() {
	gKeyForward = gKeyBack = gKeyLeft = gKeyRight = false;
	gKeyFire = gKeyJump = gKeyUse = gKeyCrouch = false;
	gKeyWepPrev = gKeyWepNext = gKeyPause = false;
	gStickDX = gStickDY = 0.f;

	for (int i = 0; i < kMaxFingers; ++i) {
		if (!gFingers[i].active) continue;
		switch (gFingers[i].role) {
		case FINGER_STICK:
			UpdateStickFromFinger(gFingers[i]);
			break;
		case FINGER_FIRE:    gKeyFire = true; break;
		case FINGER_JUMP:    gKeyJump = true; break;
		case FINGER_USE:     gKeyUse = true; break;
		case FINGER_CROUCH:  gKeyCrouch = true; break;
		case FINGER_WEPPREV: gKeyWepPrev = true; break;
		case FINGER_WEPNEXT: gKeyWepNext = true; break;
		case FINGER_PAUSE:   gKeyPause = true; break;
		default: break;
		}
	}

	/* stick → digital WASD with slight analog threshold */
	if (gStickDY < -0.25f) gKeyForward = true;
	if (gStickDY >  0.25f) gKeyBack = true;
	if (gStickDX < -0.25f) gKeyLeft = true;
	if (gStickDX >  0.25f) gKeyRight = true;
}

void Android_Touch_Init() {
	memset(gFingers, 0, sizeof(gFingers));
	gInited = true;
	ALOGI("Touch controls initialized");
}

void Android_Touch_Shutdown() {
	gInited = false;
}

bool Android_Touch_HandleEvent(const void *sdlEvent) {
	if (!gInited || !sdlEvent) return false;
	const SDL_Event *ev = (const SDL_Event *)sdlEvent;

	float x = 0.f, y = 0.f;
	SDL_FingerID fid = 0;
	int action = 0; /* 1=down 2=up 3=motion */

	switch (ev->type) {
	case SDL_FINGERDOWN:
		fid = ev->tfinger.fingerId;
		x = ev->tfinger.x;
		y = ev->tfinger.y;
		action = 1;
		break;
	case SDL_FINGERUP:
		fid = ev->tfinger.fingerId;
		x = ev->tfinger.x;
		y = ev->tfinger.y;
		action = 2;
		break;
	case SDL_FINGERMOTION:
		fid = ev->tfinger.fingerId;
		x = ev->tfinger.x;
		y = ev->tfinger.y;
		action = 3;
		break;
	default:
		return false;
	}

	/* map fingerId to slot */
	int slot = -1;
	for (int i = 0; i < kMaxFingers; ++i) {
		if (gFingers[i].active && (SDL_FingerID)(intptr_t)i == fid) {
			/* we store finger id in startX bits? better: parallel array */
		}
	}

	/* Use separate id storage */
	static SDL_FingerID sIds[kMaxFingers];
	static bool sIdsInit = false;
	if (!sIdsInit) {
		memset(sIds, 0, sizeof(sIds));
		sIdsInit = true;
	}

	if (action == 1) {
		for (int i = 0; i < kMaxFingers; ++i) {
			if (!gFingers[i].active) {
				slot = i;
				sIds[i] = fid;
				break;
			}
		}
		if (slot < 0) return true;
		gFingers[slot].active = true;
		gFingers[slot].role = RoleForPoint(x, y);
		gFingers[slot].x = x;
		gFingers[slot].y = y;
		gFingers[slot].startX = x;
		gFingers[slot].startY = y;
		RecomputeHeldKeys();
		return true;
	}

	/* find slot by finger id */
	for (int i = 0; i < kMaxFingers; ++i) {
		if (gFingers[i].active && sIds[i] == fid) {
			slot = i;
			break;
		}
	}
	if (slot < 0) return true;

	if (action == 2) {
		gFingers[slot].active = false;
		gFingers[slot].role = FINGER_NONE;
		sIds[slot] = 0;
		RecomputeHeldKeys();
		return true;
	}

	/* motion */
	float prevX = gFingers[slot].x;
	float prevY = gFingers[slot].y;
	gFingers[slot].x = x;
	gFingers[slot].y = y;

	if (gFingers[slot].role == FINGER_LOOK) {
		/* accumulate pixel deltas */
		int w = gScreenW > 0 ? gScreenW : 1280;
		int h = gScreenH > 0 ? gScreenH : 720;
		gLookDX += (x - prevX) * (float)w * gLookSens;
		gLookDY += (y - prevY) * (float)h * gLookSens;
	} else if (gFingers[slot].role == FINGER_STICK) {
		UpdateStickFromFinger(gFingers[slot]);
		RecomputeHeldKeys();
	}
	return true;
}

void Android_Touch_Frame() {
	if (!gInited) return;

	/* Query drawable size for look sensitivity scaling */
	SDL_Window *win = SDL_GL_GetCurrentWindow();
	if (win) {
		SDL_GL_GetDrawableSize(win, &gScreenW, &gScreenH);
	}

	/* Edge-trigger key changes into the engine */
	auto edgeKey = [](bool now, bool &prev, int key) {
		if (now != prev) {
			Android_Touch_PushKey(key, now);
			prev = now;
		}
	};

	edgeKey(gKeyForward, gPrevForward, 'w');
	edgeKey(gKeyBack,    gPrevBack,    's');
	edgeKey(gKeyLeft,    gPrevLeft,    'a');
	edgeKey(gKeyRight,   gPrevRight,   'd');
	edgeKey(gKeyJump,    gPrevJump,    K_SPACE);
	edgeKey(gKeyCrouch,  gPrevCrouch,  'c');
	edgeKey(gKeyPause,   gPrevPause,   K_ESCAPE);
	edgeKey(gKeyWepPrev, gPrevWepPrev, K_MWHEELUP);
	edgeKey(gKeyWepNext, gPrevWepNext, K_MWHEELDOWN);

	/* Fire / Use as mouse buttons */
	if (gKeyFire != gPrevFire) {
		Android_Touch_PushMouseButton(1, gKeyFire);
		gPrevFire = gKeyFire;
	}
	if (gKeyUse != gPrevUse) {
		Android_Touch_PushMouseButton(2, gKeyUse);
		gPrevUse = gKeyUse;
	}

	/* Look deltas */
	if ((int)gLookDX != 0 || (int)gLookDY != 0) {
		Android_Touch_PushMouseDelta((int)gLookDX, (int)gLookDY);
		gLookDX = 0.f;
		gLookDY = 0.f;
	}
}

/* -----------------------------------------------------------------------
 * Simple GL overlay (GL4ES / GLES compatible immediate-ish path)
 * -------------------------------------------------------------------- */

static void DrawCircle(float cx, float cy, float r, float R, float G, float B, float A, int segments = 24) {
	glColor4f(R, G, B, A);
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(cx, cy);
	for (int i = 0; i <= segments; ++i) {
		float a = (float)i / (float)segments * 6.2831853f;
		glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r);
	}
	glEnd();
}

static void DrawRect(float x0, float y0, float x1, float y1, float R, float G, float B, float A) {
	glColor4f(R, G, B, A);
	glBegin(GL_QUADS);
	glVertex2f(x0, y0);
	glVertex2f(x1, y0);
	glVertex2f(x1, y1);
	glVertex2f(x0, y1);
	glEnd();
}

void Android_Touch_Draw() {
	if (!gInited) return;
	if (gScreenW <= 0 || gScreenH <= 0) return;

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	/* top-left origin to match touch coords */
	glOrtho(0, 1, 1, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* Stick base */
	float scx = (kStickBase.x0 + kStickBase.x1) * 0.5f;
	float scy = (kStickBase.y0 + kStickBase.y1) * 0.5f;
	float sr  = (kStickBase.x1 - kStickBase.x0) * 0.42f;
	DrawCircle(scx, scy, sr, 1.f, 1.f, 1.f, 0.12f);
	DrawCircle(scx + gStickDX * sr * 0.55f, scy + gStickDY * sr * 0.55f, sr * 0.35f, 1.f, 1.f, 1.f, 0.28f);

	auto btn = [](const TouchRect &r, bool held, float R, float G, float B) {
		float a = held ? 0.45f : 0.18f;
		DrawRect(r.x0, r.y0, r.x1, r.y1, R, G, B, a);
		/* border */
		glColor4f(1.f, 1.f, 1.f, held ? 0.55f : 0.25f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(r.x0, r.y0); glVertex2f(r.x1, r.y0);
		glVertex2f(r.x1, r.y1); glVertex2f(r.x0, r.y1);
		glEnd();
	};

	btn(kFireBtn,   gKeyFire,   1.f, 0.25f, 0.2f);
	btn(kJumpBtn,   gKeyJump,   0.3f, 0.7f, 1.f);
	btn(kUseBtn,    gKeyUse,    0.3f, 1.f, 0.45f);
	btn(kCrouchBtn, gKeyCrouch, 0.9f, 0.8f, 0.2f);
	btn(kWepPrev,   gKeyWepPrev, 0.7f, 0.7f, 0.7f);
	btn(kWepNext,   gKeyWepNext, 0.7f, 0.7f, 0.7f);
	btn(kPauseBtn,  gKeyPause,  0.6f, 0.6f, 0.9f);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}
