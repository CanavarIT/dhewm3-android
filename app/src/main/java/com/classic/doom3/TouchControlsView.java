package com.classic.doom3;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.util.SparseArray;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;

/**
 * Stick = movement (WASD) only.
 * Any free finger = look (relative mouse via JNI) — works together with stick.
 * Controls that are not stick/look still pass nothing; we own multi-touch fully
 * so "stick first then look" works (no dependency on SDL getting the 2nd pointer).
 *
 * SKIP is always visible, small and secondary (top-left).
 */
public class TouchControlsView extends View {

    private static final float STICK_RADIUS_DP = 52f;
    private static final float KNOB_RADIUS_DP  = 21f;
    private static final float TOUCH_SLOP_DP   = 10f;
    private static final float LOOK_SENS      = 2.0f; // tuned; fractional accum avoids stutter

    private static final float DEAD_ON  = 0.24f;
    private static final float DEAD_OFF = 0.16f;
    private static final float DIR_ON   = 0.38f;
    private static final float DIR_OFF  = 0.30f;

    private static final float STICK_ZONE_X = 0.42f;
    private static final float STICK_ZONE_Y = 0.30f;

    private static final int WHITE  = 0xFFFFFF;
    private static final int ACCENT = 0xFF6B5E;

    private enum Role {
        NONE, STICK, LOOK, FIRE, JUMP, USE, CROUCH, WEP_PREV, WEP_NEXT, PAUSE, SKIP
    }

    private static final class Btn {
        final Role role;
        final RectF rect = new RectF();
        float press;
        Btn(Role r) { role = r; }
    }

    private static final class Finger {
        Role role = Role.NONE;
        float x, y;
        float prevX, prevY;
    }

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF tmp = new RectF();
    private final Path path = new Path();

    private final Btn fire    = new Btn(Role.FIRE);
    private final Btn jump    = new Btn(Role.JUMP);
    private final Btn use     = new Btn(Role.USE);
    private final Btn crouch  = new Btn(Role.CROUCH);
    private final Btn wepPrev = new Btn(Role.WEP_PREV);
    private final Btn wepNext = new Btn(Role.WEP_NEXT);
    private final Btn pause   = new Btn(Role.PAUSE);
    private final Btn skip    = new Btn(Role.SKIP);
    private final Btn[] buttons = { fire, jump, use, crouch, wepPrev, wepNext, pause, skip };

    private final SparseArray<Finger> fingers = new SparseArray<>();

    private final float density;
    private int insL, insR, insT, insB;

    private float stickR, knobR;
    private float homeX, homeY;
    private float ox, oy;
    private float stickDx, stickDy;
    private float stickP;
    private boolean stickActive;

    /* sub-pixel remainder so slow swipes are not rounded to 0 */
    private float lookAccX, lookAccY;

    private boolean keyW, keyA, keyS, keyD;
    private boolean keyFire, keyJump, keyUse, keyCrouch;
    private boolean keyWepPrev, keyWepNext, keyPause, keySkip;
    private boolean prevW, prevA, prevS, prevD;
    private boolean prevFire, prevJump, prevUse, prevCrouch;
    private boolean prevWepPrev, prevWepNext, prevPause, prevSkip;

    public TouchControlsView(Context context) {
        super(context);
        density = context.getResources().getDisplayMetrics().density;
        setClickable(true);
        setFocusable(false);
        setWillNotDraw(false);
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);
        paint.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        layoutControls(w, h);
    }

    @SuppressWarnings("deprecation")
    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets in) {
        insL = in.getSystemWindowInsetLeft();
        insR = in.getSystemWindowInsetRight();
        insT = in.getSystemWindowInsetTop();
        insB = in.getSystemWindowInsetBottom();
        if (Build.VERSION.SDK_INT >= 28 && in.getDisplayCutout() != null) {
            insL = Math.max(insL, in.getDisplayCutout().getSafeInsetLeft());
            insR = Math.max(insR, in.getDisplayCutout().getSafeInsetRight());
            insT = Math.max(insT, in.getDisplayCutout().getSafeInsetTop());
            insB = Math.max(insB, in.getDisplayCutout().getSafeInsetBottom());
        }
        layoutControls(getWidth(), getHeight());
        invalidate();
        return super.onApplyWindowInsets(in);
    }

    private void circle(Btn b, float cx, float cy, float r) {
        b.rect.set(cx - r, cy - r, cx + r, cy + r);
    }

    private void layoutControls(int w, int h) {
        if (w <= 0 || h <= 0) return;
        final float d = density;

        final float left   = insL + 24 * d;
        final float right  = w - insR - 24 * d;
        final float bottom = h - insB - 20 * d;
        final float top    = insT + 14 * d;

        stickR = STICK_RADIUS_DP * d;
        knobR  = KNOB_RADIUS_DP * d;
        homeX = left + stickR;
        homeY = bottom - stickR;
        if (!stickActive) { ox = homeX; oy = homeY; }

        final float fr = 36 * d;
        final float fx = right - fr;
        final float fy = bottom - fr;
        circle(fire,   fx,          fy,          fr);
        circle(jump,   fx - 62 * d, fy - 38 * d, 26 * d);
        circle(crouch, fx - 82 * d, fy + 14 * d, 22 * d);
        circle(use,    fx,          fy - 84 * d, 24 * d);

        final float pr = 16 * d;
        circle(pause, right - pr, top + pr, pr);
        final float pauseLeft = right - 2 * pr;
        wepNext.rect.set(pauseLeft - 8 * d - 40 * d, top + 1 * d, pauseLeft - 8 * d, top + 31 * d);
        wepPrev.rect.set(wepNext.rect.left - 6 * d - 40 * d, top + 1 * d,
                         wepNext.rect.left - 6 * d, top + 31 * d);

        /*
         * SKIP — always on, secondary: small, top-left, out of the way.
         */
        float sw = 48 * d;
        float sh = 22 * d;
        float sx = left + sw * 0.5f;
        float sy = top + sh * 0.5f;
        skip.rect.set(sx - sw * 0.5f, sy - sh * 0.5f, sx + sw * 0.5f, sy + sh * 0.5f);
    }

    private Role roleAt(float x, float y) {
        final float slop = TOUCH_SLOP_DP * density;
        Role best = Role.NONE;
        float bestDist = Float.MAX_VALUE;
        for (Btn b : buttons) {
            if (b.rect.isEmpty()) continue;
            tmp.set(b.rect);
            tmp.inset(-slop, -slop);
            if (tmp.contains(x, y)) {
                float dx = x - b.rect.centerX();
                float dy = y - b.rect.centerY();
                float dist = dx * dx + dy * dy;
                if (dist < bestDist) { bestDist = dist; best = b.role; }
            }
        }
        if (best != Role.NONE) return best;

        if (x < getWidth() * STICK_ZONE_X && y > getHeight() * STICK_ZONE_Y) {
            return Role.STICK;
        }
        /* Everything else is look — so stick + look multi-touch works. */
        return Role.LOOK;
    }

    private boolean hasStickFinger() {
        for (int i = 0; i < fingers.size(); i++) {
            if (fingers.valueAt(i).role == Role.STICK) return true;
        }
        return false;
    }

    private void updateStick(Finger f) {
        float dx = f.x - ox;
        float dy = f.y - oy;
        float len = (float) Math.hypot(dx, dy);
        if (len > stickR) {
            float k = (len - stickR) / len;
            ox += dx * k;
            oy += dy * k;
            dx = f.x - ox;
            dy = f.y - oy;
        }
        stickDx = dx / stickR;
        stickDy = dy / stickR;
    }

    private void recomputeKeys() {
        keyW = keyA = keyS = keyD = false;
        keyFire = keyJump = keyUse = keyCrouch = false;
        keyWepPrev = keyWepNext = keyPause = keySkip = false;
        stickDx = stickDy = 0;
        stickActive = false;

        for (int i = 0; i < fingers.size(); i++) {
            Finger f = fingers.valueAt(i);
            switch (f.role) {
                case STICK:    stickActive = true; updateStick(f); break;
                case FIRE:     keyFire = true; break;
                case JUMP:     keyJump = true; break;
                case USE:      keyUse = true; break;
                case CROUCH:   keyCrouch = true; break;
                case WEP_PREV: keyWepPrev = true; break;
                case WEP_NEXT: keyWepNext = true; break;
                case PAUSE:    keyPause = true; break;
                case SKIP:     keySkip = true; break;
                default: break;
            }
        }

        final float len = (float) Math.hypot(stickDx, stickDy);
        final boolean wasMoving = prevW || prevA || prevS || prevD;
        if (len > (wasMoving ? DEAD_OFF : DEAD_ON)) {
            float ux = stickDx / len;
            float uy = stickDy / len;
            keyW = -uy > (prevW ? DIR_OFF : DIR_ON);
            keyS =  uy > (prevS ? DIR_OFF : DIR_ON);
            keyA = -ux > (prevA ? DIR_OFF : DIR_ON);
            keyD =  ux > (prevD ? DIR_OFF : DIR_ON);
        }
        flushEdges();
    }

    private void flushEdges() {
        edgeKey(keyW, prevW, 'w'); prevW = keyW;
        edgeKey(keyS, prevS, 's'); prevS = keyS;
        edgeKey(keyA, prevA, 'a'); prevA = keyA;
        edgeKey(keyD, prevD, 'd'); prevD = keyD;
        edgeKey(keyJump, prevJump, Doom3Native.K_SPACE); prevJump = keyJump;
        edgeKey(keyCrouch, prevCrouch, 'c'); prevCrouch = keyCrouch;
        edgeKey(keyPause, prevPause, Doom3Native.K_ESCAPE); prevPause = keyPause;
        edgeKey(keyWepPrev, prevWepPrev, Doom3Native.K_MWHEELUP); prevWepPrev = keyWepPrev;
        edgeKey(keyWepNext, prevWepNext, Doom3Native.K_MWHEELDOWN); prevWepNext = keyWepNext;

        if (keyFire != prevFire) {
            Doom3Native.touchMouseButton(1, keyFire);
            prevFire = keyFire;
        }
        if (keyUse != prevUse) {
            Doom3Native.touchMouseButton(2, keyUse);
            prevUse = keyUse;
        }
        if (keySkip != prevSkip) {
            /* cinematic skip: attack + esc pulse */
            Doom3Native.touchMouseButton(1, keySkip);
            Doom3Native.touchKey(Doom3Native.K_ESCAPE, keySkip);
            prevSkip = keySkip;
        }
    }

    private static void edgeKey(boolean now, boolean prev, int key) {
        if (now != prev) {
            Doom3Native.touchKey(key, now);
        }
    }

    private void emitLook(float rawDx, float rawDy) {
        lookAccX += rawDx * LOOK_SENS;
        lookAccY += rawDy * LOOK_SENS;
        int dx = (int) lookAccX;
        int dy = (int) lookAccY;
        if (dx != 0 || dy != 0) {
            lookAccX -= dx;
            lookAccY -= dy;
            Doom3Native.touchMouseDelta(dx, dy);
        }
    }

    private void releaseAll() {
        fingers.clear();
        stickActive = false;
        recomputeKeys();
        invalidate();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        releaseAll();
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (!hasWindowFocus) releaseAll();
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        final int w = Math.max(1, getWidth());
        final int h = Math.max(1, getHeight());
        final int action = e.getActionMasked();
        final int index = e.getActionIndex();
        final int pid = e.getPointerId(index);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                float x = e.getX(index);
                float y = e.getY(index);
                Role role = roleAt(x, y);

                /* Only one stick finger */
                if (role == Role.STICK && hasStickFinger()) {
                    role = Role.LOOK;
                }

                Finger f = new Finger();
                f.role = role;
                f.x = f.prevX = x;
                f.y = f.prevY = y;

                if (role == Role.STICK) {
                    float pad = 8 * density;
                    ox = clamp(x, insL + pad + stickR, Math.max(insL + pad + stickR, getWidth() * 0.5f));
                    oy = clamp(y, pad + stickR, Math.max(pad + stickR, getHeight() - pad - stickR));
                } else if (role != Role.LOOK) {
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);
                }

                fingers.put(pid, f);
                recomputeKeys();
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < e.getPointerCount(); i++) {
                    int id = e.getPointerId(i);
                    Finger f = fingers.get(id);
                    if (f == null) continue;

                    if (f.role == Role.LOOK) {
                        /* historical samples = smoother look (Android batches moves) */
                        final int hist = e.getHistorySize();
                        float lastX = f.prevX;
                        float lastY = f.prevY;
                        for (int hi = 0; hi < hist; hi++) {
                            float hx = e.getHistoricalX(i, hi);
                            float hy = e.getHistoricalY(i, hi);
                            emitLook(hx - lastX, hy - lastY);
                            lastX = hx;
                            lastY = hy;
                        }
                        float nx = e.getX(i);
                        float ny = e.getY(i);
                        emitLook(nx - lastX, ny - lastY);
                        f.prevX = nx;
                        f.prevY = ny;
                        f.x = nx;
                        f.y = ny;
                    } else {
                        f.prevX = f.x;
                        f.prevY = f.y;
                        f.x = e.getX(i);
                        f.y = e.getY(i);
                    }
                }
                recomputeKeys();
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                Finger gone = fingers.get(pid);
                if (gone != null && gone.role == Role.LOOK) {
                    lookAccX = lookAccY = 0f;
                }
                fingers.remove(pid);
                if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                    fingers.clear();
                    lookAccX = lookAccY = 0f;
                }
                recomputeKeys();
                invalidate();
                return true;
            }
        }
        return true;
    }

    @Override
    protected void onDraw(Canvas c) {
        super.onDraw(c);
        boolean animating = false;

        animating |= drawStick(c);

        animating |= ease(fire,    keyFire);
        animating |= ease(jump,    keyJump);
        animating |= ease(use,     keyUse);
        animating |= ease(crouch,  keyCrouch);
        animating |= ease(wepPrev, keyWepPrev);
        animating |= ease(wepNext, keyWepNext);
        animating |= ease(pause,   keyPause);
        animating |= ease(skip,    keySkip);

        drawBtn(c, fire,    ACCENT, true,  null, 1f);
        drawBtn(c, jump,    WHITE,  false, null, 1f);
        drawBtn(c, use,     WHITE,  false, null, 1f);
        drawBtn(c, crouch,  WHITE,  false, null, 1f);
        drawBtn(c, wepPrev, WHITE,  false, null, 1f);
        drawBtn(c, wepNext, WHITE,  false, null, 1f);
        drawBtn(c, pause,   WHITE,  false, null, 1f);
        /* secondary: lower opacity */
        drawBtn(c, skip,    WHITE,  false, "SKIP", 0.45f);

        if (animating) postInvalidateOnAnimation();
    }

    private static boolean ease(Btn b, boolean held) {
        final float target = held ? 1f : 0f;
        b.press += (target - b.press) * 0.35f;
        if (Math.abs(target - b.press) < 0.02f) b.press = target;
        return b.press != target;
    }

    private boolean drawStick(Canvas c) {
        final float target = stickActive ? 1f : 0f;
        stickP += (target - stickP) * 0.3f;
        boolean animating = true;
        if (Math.abs(target - stickP) < 0.02f) { stickP = target; animating = false; }

        final float sw = 1.5f * density;

        if (stickP < 1f) {
            float a = 1f - stickP;
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(sw);
            paint.setColor(alpha(WHITE, 0.16f * a));
            c.drawCircle(homeX, homeY, stickR, paint);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(alpha(WHITE, 0.20f * a));
            c.drawCircle(homeX, homeY, 3 * density, paint);
        }

        if (stickP > 0f) {
            float a = stickP;
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(alpha(WHITE, 0.07f * a));
            c.drawCircle(ox, oy, stickR, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(sw);
            paint.setColor(alpha(WHITE, 0.38f * a));
            c.drawCircle(ox, oy, stickR, paint);

            float kx = ox + stickDx * stickR * 0.8f;
            float ky = oy + stickDy * stickR * 0.8f;
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(alpha(WHITE, 0.30f * a));
            c.drawCircle(kx, ky, knobR, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setColor(alpha(WHITE, 0.70f * a));
            c.drawCircle(kx, ky, knobR, paint);
        }
        return animating;
    }

    private void drawBtn(Canvas c, Btn b, int rgb, boolean accent, String labelOverride, float opacity) {
        if (b.rect.isEmpty()) return;
        final float p = b.press;
        final float s = 1f - 0.07f * p;
        final float cx = b.rect.centerX();
        final float cy = b.rect.centerY();
        final float hw = b.rect.width() * 0.5f * s;
        final float hh = b.rect.height() * 0.5f * s;
        final float rad = Math.min(hw, hh);
        tmp.set(cx - hw, cy - hh, cx + hw, cy + hh);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(alpha(rgb, lerp(0.08f, 0.28f, p) * opacity));
        c.drawRoundRect(tmp, rad, rad, paint);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1.2f * density);
        paint.setColor(alpha(rgb, lerp(accent ? 0.55f : 0.28f, 0.75f, p) * opacity));
        c.drawRoundRect(tmp, rad, rad, paint);

        if (labelOverride != null) {
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(alpha(WHITE, lerp(0.55f, 0.85f, p) * opacity));
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(hh * 0.65f);
            c.drawText(labelOverride, cx, cy - (paint.descent() + paint.ascent()) / 2f, paint);
        } else {
            drawIcon(c, b, cx, cy, Math.min(hw, hh), p);
        }
    }

    private void drawIcon(Canvas c, Btn b, float cx, float cy, float r, float p) {
        paint.setColor(alpha(WHITE, lerp(0.80f, 1f, p)));
        paint.setStrokeWidth(2f * density);
        paint.setStyle(Paint.Style.STROKE);

        switch (b.role) {
            case FIRE:
                c.drawCircle(cx, cy, r * 0.30f, paint);
                paint.setStyle(Paint.Style.FILL);
                c.drawCircle(cx, cy, r * 0.09f, paint);
                break;
            case JUMP:
                chevron(c, cx, cy, r * 0.36f, 0, -1);
                break;
            case CROUCH:
                chevron(c, cx, cy, r * 0.36f, 0, 1);
                break;
            case WEP_PREV:
                chevron(c, cx, cy, b.rect.height() * 0.17f, -1, 0);
                break;
            case WEP_NEXT:
                chevron(c, cx, cy, b.rect.height() * 0.17f, 1, 0);
                break;
            case USE:
                paint.setStyle(Paint.Style.FILL);
                paint.setTextAlign(Paint.Align.CENTER);
                paint.setTextSize(r * 0.95f);
                c.drawText("E", cx, cy - (paint.descent() + paint.ascent()) / 2f, paint);
                break;
            case PAUSE: {
                float bw = r * 0.26f;
                float bh = r * 0.34f;
                c.drawLine(cx - bw, cy - bh, cx - bw, cy + bh, paint);
                c.drawLine(cx + bw, cy - bh, cx + bw, cy + bh, paint);
                break;
            }
            default:
                break;
        }
    }

    private void chevron(Canvas c, float cx, float cy, float s, int dx, int dy) {
        final float px = -dy, py = dx;
        final float bx = cx - dx * s * 0.5f;
        final float by = cy - dy * s * 0.5f;
        path.reset();
        path.moveTo(bx + px * s, by + py * s);
        path.lineTo(cx + dx * s * 0.5f, cy + dy * s * 0.5f);
        path.lineTo(bx - px * s, by - py * s);
        c.drawPath(path, paint);
    }

    private static int alpha(int rgb, float a) {
        int ia = (int) (clamp(a, 0f, 1f) * 255f + 0.5f);
        return (rgb & 0x00FFFFFF) | (ia << 24);
    }

    private static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    private static float clamp(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }
}

