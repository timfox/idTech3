/*
 * Copyright (C) 2026 Gopex LLC. All rights reserved.
 *
 * On-screen dual sticks + action buttons for FPS play without physical controller.
 * Touches outside defined HUD regions pass through to the engine (e.g. UI mouse).
 */

package com.gopex.idtech3;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

public class TouchHudView extends View {

    private static final float MOVE_CX = 0.20f;
    private static final float MOVE_CY = 0.82f;
    private static final float LOOK_CX = 0.82f;
    private static final float LOOK_CY = 0.82f;
    /** Stick zone as fraction of min(w,h) - touch must start inside to capture */
    private static final float STICK_ZONE_FRAC = 0.22f;

    private final Paint ringPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint knobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint btnPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private int movePointerId = -1;
    private int lookPointerId = -1;
    private float moveBaseX, moveBaseY;
    private float lookBaseX, lookBaseY;
    private float moveKnobX, moveKnobY;
    private float lookKnobX, lookKnobY;
    private boolean moveForward, moveBack, moveLeft, moveRight;

    private float stickRadiusPx;
    private float lookSens = 2.8f;
    private float moveKeyThresh = 0.28f;

    public TouchHudView(Context ctx) {
        super(ctx);
        setWillNotDraw(false);
        ringPaint.setStyle(Paint.Style.STROKE);
        ringPaint.setStrokeWidth(dp(2));
        ringPaint.setColor(Color.argb(100, 255, 255, 255));
        knobPaint.setStyle(Paint.Style.FILL);
        knobPaint.setColor(Color.argb(140, 200, 220, 255));
        btnPaint.setStyle(Paint.Style.STROKE);
        btnPaint.setStrokeWidth(dp(2));
        btnPaint.setColor(Color.argb(120, 255, 255, 255));
        labelPaint.setColor(Color.argb(180, 255, 255, 255));
        labelPaint.setTextAlign(Paint.Align.CENTER);
        labelPaint.setTextSize(sp(12));
    }

    private int dp(int d) {
        return Math.round(d * getResources().getDisplayMetrics().density);
    }

    private float sp(float s) {
        return s * getResources().getDisplayMetrics().scaledDensity;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        stickRadiusPx = Math.min(w, h) * STICK_ZONE_FRAC;
    }

    private boolean inCircle(float x, float y, float cx, float cy, float r) {
        float dx = x - cx;
        float dy = y - cy;
        return dx * dx + dy * dy <= r * r;
    }

    private void releaseMoveKeys() {
        if (moveForward) {
            TouchOverlayBridge.nativeCommand("-forward");
            moveForward = false;
        }
        if (moveBack) {
            TouchOverlayBridge.nativeCommand("-back");
            moveBack = false;
        }
        if (moveLeft) {
            TouchOverlayBridge.nativeCommand("-moveleft");
            moveLeft = false;
        }
        if (moveRight) {
            TouchOverlayBridge.nativeCommand("-moveright");
            moveRight = false;
        }
    }

    private void updateMoveKeys(float nx, float ny) {
        boolean f = ny < -moveKeyThresh;
        boolean b = ny > moveKeyThresh;
        boolean l = nx < -moveKeyThresh;
        boolean r = nx > moveKeyThresh;

        if (f != moveForward) {
            TouchOverlayBridge.nativeCommand(f ? "+forward" : "-forward");
            moveForward = f;
        }
        if (b != moveBack) {
            TouchOverlayBridge.nativeCommand(b ? "+back" : "-back");
            moveBack = b;
        }
        if (l != moveLeft) {
            TouchOverlayBridge.nativeCommand(l ? "+moveleft" : "-moveleft");
            moveLeft = l;
        }
        if (r != moveRight) {
            TouchOverlayBridge.nativeCommand(r ? "+moveright" : "-moveright");
            moveRight = r;
        }
    }

    private boolean handleButton(int action, float x, float y) {
        int w = getWidth();
        int h = getHeight();
        if (w < 8 || h < 8) {
            return false;
        }
        /* Right column: x 72%–98%, buttons from y 12% downward */
        if (x < w * 0.70f || x > w * 0.99f) {
            return false;
        }

        float y0 = h * 0.10f;
        float bh = h * 0.085f;
        float gap = h * 0.012f;

        final boolean down = (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN);
        final boolean up = (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP
                || action == MotionEvent.ACTION_CANCEL);

        /* Menu */
        if (y >= y0 && y < y0 + bh) {
            if (down) {
                TouchOverlayBridge.nativeKey(27 /* K_ESCAPE */, true);
            } else if (up) {
                TouchOverlayBridge.nativeKey(27, false);
            }
            return true;
        }
        y0 += bh + gap;
        /* Next weapon - default Q3 bind is ] (weapnext) */
        if (y >= y0 && y < y0 + bh) {
            if (down) {
                TouchOverlayBridge.nativeKey(']', true);
                TouchOverlayBridge.nativeKey(']', false);
            }
            return true;
        }
        y0 += bh + gap;
        /* Sprint */
        if (y >= y0 && y < y0 + bh) {
            if (down) {
                TouchOverlayBridge.nativeCommand("+speed");
            } else if (up) {
                TouchOverlayBridge.nativeCommand("-speed");
            }
            return true;
        }
        y0 += bh + gap;
        /* Jump */
        if (y >= y0 && y < y0 + bh) {
            if (down) {
                TouchOverlayBridge.nativeCommand("+moveup");
            } else if (up) {
                TouchOverlayBridge.nativeCommand("-moveup");
            }
            return true;
        }
        y0 += bh + gap;
        /* Fire */
        if (y >= y0 && y < y0 + bh * 1.15f) {
            if (down) {
                TouchOverlayBridge.nativeCommand("+attack");
            } else if (up) {
                TouchOverlayBridge.nativeCommand("-attack");
            }
            return true;
        }
        return false;
    }

    /**
     * Called from GameActivity.dispatchTouchEvent before super. Return true if the HUD consumed
     * the event (sticks/buttons); otherwise the native layer receives the touch.
     */
    @SuppressLint("ClickableViewAccessibility")
    public boolean onOverlayTouch(MotionEvent event) {
        int action = event.getActionMasked();
        int w = getWidth();
        int h = getHeight();
        if (w < 8 || h < 8) {
            return false;
        }

        float mcx = w * MOVE_CX;
        float mcy = h * MOVE_CY;
        float lcx = w * LOOK_CX;
        float lcy = h * LOOK_CY;

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int idx = event.getActionIndex();
            float x = event.getX(idx);
            float y = event.getY(idx);
            int pid = event.getPointerId(idx);

            if (handleButton(action, x, y)) {
                invalidate();
                return true;
            }

            if (movePointerId < 0 && inCircle(x, y, mcx, mcy, stickRadiusPx * 1.2f)) {
                movePointerId = pid;
                moveBaseX = mcx;
                moveBaseY = mcy;
                moveKnobX = x;
                moveKnobY = y;
                invalidate();
                return true;
            }
            if (lookPointerId < 0 && inCircle(x, y, lcx, lcy, stickRadiusPx * 1.2f)) {
                lookPointerId = pid;
                lookBaseX = lcx;
                lookBaseY = lcy;
                lookKnobX = x;
                lookKnobY = y;
                invalidate();
                return true;
            }
            return false;
        }

        if (action == MotionEvent.ACTION_MOVE) {
            boolean handled = false;
            for (int i = 0; i < event.getPointerCount(); i++) {
                int pid = event.getPointerId(i);
                float x = event.getX(i);
                float y = event.getY(i);
                if (pid == movePointerId) {
                    float dx = x - moveBaseX;
                    float dy = y - moveBaseY;
                    float dist = (float) Math.hypot(dx, dy);
                    if (dist > stickRadiusPx) {
                        dx = dx * (stickRadiusPx / dist);
                        dy = dy * (stickRadiusPx / dist);
                    }
                    moveKnobX = moveBaseX + dx;
                    moveKnobY = moveBaseY + dy;
                    updateMoveKeys(dx / stickRadiusPx, dy / stickRadiusPx);
                    handled = true;
                } else if (pid == lookPointerId) {
                    float dx = x - lookKnobX;
                    float dy = y - lookKnobY;
                    lookKnobX = x;
                    lookKnobY = y;
                    TouchOverlayBridge.nativeMouseDelta(
                            Math.round(dx * lookSens),
                            Math.round(dy * lookSens));
                    handled = true;
                }
            }
            if (handled) {
                invalidate();
                return true;
            }
            return false;
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP
                || action == MotionEvent.ACTION_CANCEL) {
            int idx = event.getActionIndex();
            int pid = event.getPointerId(idx);
            float x = event.getX(idx);
            float y = event.getY(idx);

            if (handleButton(action, x, y)) {
                invalidate();
                return true;
            }

            if (pid == movePointerId) {
                movePointerId = -1;
                moveKnobX = moveBaseX;
                moveKnobY = moveBaseY;
                releaseMoveKeys();
                invalidate();
                return true;
            }
            if (pid == lookPointerId) {
                lookPointerId = -1;
                lookKnobX = lookBaseX;
                lookKnobY = lookBaseY;
                invalidate();
                return true;
            }
        }
        return false;
    }

    @Override
    protected void onDraw(Canvas c) {
        super.onDraw(c);
        int w = getWidth();
        int h = getHeight();
        if (w < 8) {
            return;
        }
        float mcx = w * MOVE_CX;
        float mcy = h * MOVE_CY;
        float lcx = w * LOOK_CX;
        float lcy = h * LOOK_CY;

        if (movePointerId < 0) {
            moveKnobX = mcx;
            moveKnobY = mcy;
        }
        if (lookPointerId < 0) {
            lookKnobX = lcx;
            lookKnobY = lcy;
        }

        c.drawCircle(mcx, mcy, stickRadiusPx, ringPaint);
        c.drawCircle(moveKnobX, moveKnobY, stickRadiusPx * 0.28f, knobPaint);
        c.drawCircle(lcx, lcy, stickRadiusPx, ringPaint);
        c.drawCircle(lookKnobX, lookKnobY, stickRadiusPx * 0.28f, knobPaint);

        float y0 = h * 0.10f;
        float bh = h * 0.085f;
        float gap = h * 0.012f;
        float bx0 = w * 0.72f;
        float bx1 = w * 0.98f;
        drawBtn(c, bx0, y0, bx1, y0 + bh, "Menu");
        y0 += bh + gap;
        drawBtn(c, bx0, y0, bx1, y0 + bh, "Weap");
        y0 += bh + gap;
        drawBtn(c, bx0, y0, bx1, y0 + bh, "Run");
        y0 += bh + gap;
        drawBtn(c, bx0, y0, bx1, y0 + bh, "Jump");
        y0 += bh + gap;
        drawBtn(c, bx0, y0, bx1, y0 + bh * 1.15f, "Fire");
    }

    private void drawBtn(Canvas c, float l, float t, float r, float b, String label) {
        c.drawRoundRect(l, t, r, b, dp(6), dp(6), btnPaint);
        float cx = (l + r) * 0.5f;
        float cy = (t + b) * 0.5f - labelPaint.descent();
        c.drawText(label, cx, cy, labelPaint);
    }
}
