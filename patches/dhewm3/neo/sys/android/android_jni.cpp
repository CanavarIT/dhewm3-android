/*
 * JNI entry points for Java touch overlay (com.classic.doom3.Doom3Native).
 */
#include <jni.h>
#include <android/log.h>

#define JTAG "Doom3JNI"
#define JLOGI(...) __android_log_print(ANDROID_LOG_INFO, JTAG, __VA_ARGS__)

extern "C" {
void Android_Touch_PushKey(int key, bool down);
void Android_Touch_PushMouseDelta(int dx, int dy);
void Android_Touch_PushMouseButton(int button, bool down);
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_classic_doom3_Doom3Native_touchKey(JNIEnv *, jclass, jint key, jboolean down) {
	JLOGI("touchKey key=%d down=%d", (int)key, (int)down);
	Android_Touch_PushKey((int)key, down == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_classic_doom3_Doom3Native_touchMouseDelta(JNIEnv *, jclass, jint dx, jint dy) {
	/* no per-move log — it was flooding logcat and adding latency */
	Android_Touch_PushMouseDelta((int)dx, (int)dy);
}

JNIEXPORT void JNICALL
Java_com_classic_doom3_Doom3Native_touchMouseButton(JNIEnv *, jclass, jint button, jboolean down) {
	JLOGI("touchMouseButton btn=%d down=%d", (int)button, (int)down);
	Android_Touch_PushMouseButton((int)button, down == JNI_TRUE);
}

} // extern "C"
