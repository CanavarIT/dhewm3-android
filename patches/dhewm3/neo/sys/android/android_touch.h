#ifndef __ANDROID_TOUCH_H__
#define __ANDROID_TOUCH_H__

/*
 * Compact virtual controls for dhewm3 on Android.
 *
 * Layout (landscape, normalized 0..1):
 *
 *   [WEP-][WEP+]                              [PAUSE]
 *
 *   ( left stick )                 ( look drag zone )
 *
 *   [CROUCH][USE]                    [JUMP]  [FIRE]
 *
 * Stick → WASD keys
 * Look drag → relative mouse
 * Fire → mouse1, Jump → space, Use → mouse2, Crouch → c
 * Weapon ± → mwheel, Pause → escape
 */

void Android_Touch_Init();
void Android_Touch_Shutdown();

/* Called from the SDL event loop. Returns true if the event was consumed. */
bool Android_Touch_HandleEvent(const void *sdlEvent);

/* Inject held keys / mouse deltas into the engine poll queues each frame. */
void Android_Touch_Frame();

/* Draw translucent HUD on top of the GL framebuffer (before SwapBuffers). */
void Android_Touch_Draw();

#endif
