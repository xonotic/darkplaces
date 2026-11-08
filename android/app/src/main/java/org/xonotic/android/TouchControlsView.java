package org.xonotic.android;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * On-screen controller drawn over the SDL surface.
 *
 * Left thumbstick and buttons feed an SDL virtual game controller (see
 * jni/android_touchpad.c), so they use the same X360_* binds as a real pad.
 * Dragging on free space on the right half feeds mouse-style look deltas
 * straight into the engine for precise aiming. The overlay hides itself in menus/console (touches then go
 * to the game as mouse taps) and while a physical gamepad is connected.
 */
public class TouchControlsView extends View implements InputManager.InputDeviceListener {
    static native void nativeSetActive(boolean active);
    static native boolean nativeInGame();
    static native void nativeSetAxis(int axis, float value);
    static native void nativeSetButton(int button, boolean down);
    static native void nativeAddLook(float dx, float dy);

    // SDL_GameControllerAxis / SDL_GameControllerButton values
    static final int AXIS_LEFTX = 0, AXIS_LEFTY = 1, AXIS_TRIGGERLEFT = 4, AXIS_TRIGGERRIGHT = 5;
    static final int BTN_A = 0, BTN_B = 1, BTN_X = 2, BTN_Y = 3, BTN_BACK = 4, BTN_START = 6,
            BTN_LSTICK = 7, BTN_RSTICK = 8, BTN_LSHOULDER = 9, BTN_RSHOULDER = 10, BTN_DPAD_UP = 11;
    /** Pseudo-ids for triggers, which are axes in SDL. */
    static final int TRIG_RIGHT = 100, TRIG_LEFT = 101;

    static final String PREFS = "touch";
    static final String PREF_MODE = "mode"; // 0 = auto, 1 = always on, 2 = off

    static final class Button {
        final String label; final int id; final float cx, cy, r; // cx/cy/r in "dp units" relative to edges
        final boolean fromRight, fromBottom;
        RectF hit = new RectF(); float px, py, pr; boolean down;
        Button(String label, int id, float cx, float cy, float r, boolean fromRight, boolean fromBottom) {
            this.label = label; this.id = id; this.cx = cx; this.cy = cy; this.r = r;
            this.fromRight = fromRight; this.fromBottom = fromBottom;
        }
    }

    private final List<Button> buttons = new ArrayList<>();
    private final Button toggle;
    private final SparseArray<Object> owners = new SparseArray<>(); // pointerId -> Button / STICK / LOOK
    private static final Object STICK = new Object(), LOOK = new Object();
    private final SparseArray<float[]> lookLast = new SparseArray<>();

    private float density;
    private float stickCx, stickCy, stickR, knobX, knobY, stickHomeX, stickHomeY;
    private boolean stickActive;
    private boolean ingame, hasGamepad, visibleNow;
    private int mode;
    private float lookScale = 1.6f;

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final SharedPreferences prefs;

    public TouchControlsView(Context ctx) {
        super(ctx);
        density = ctx.getResources().getDisplayMetrics().density;
        prefs = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        mode = prefs.getInt(PREF_MODE, 0);

        // Right-hand cluster (offsets from right/bottom edges, in dp)
        buttons.add(new Button("FIRE", TRIG_RIGHT, 90, 110, 52, true, true));
        buttons.add(new Button("ALT", TRIG_LEFT, 190, 70, 36, true, true));
        buttons.add(new Button("JUMP", BTN_A, 70, 220, 38, true, true));
        buttons.add(new Button("CRCH", BTN_B, 180, 170, 30, true, true));
        buttons.add(new Button("RLD", BTN_X, 260, 120, 26, true, true));
        buttons.add(new Button("HOOK", BTN_LSTICK, 255, 45, 26, true, true));
        buttons.add(new Button("ZOOM", BTN_RSTICK, 160, 255, 26, true, true));
        buttons.add(new Button("WPN+", BTN_RSHOULDER, 60, 320, 28, true, true));
        buttons.add(new Button("WPN-", BTN_LSHOULDER, 130, 320, 28, true, true));
        buttons.add(new Button("LAST", BTN_Y, 200, 320, 24, true, true));
        // Top row
        buttons.add(new Button("MENU", BTN_START, 40, 36, 24, false, false));
        buttons.add(new Button("TAB", BTN_DPAD_UP, 100, 36, 24, false, false));
        buttons.add(new Button("CHAT", 13 /* DPAD_LEFT */, 160, 36, 24, false, false));
        toggle = new Button("PAD", -1, 40, 36, 22, true, false);

        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(2 * density);
        text.setTextAlign(Paint.Align.CENTER);
        text.setColor(Color.argb(220, 255, 255, 255));
        text.setFakeBoldText(true);

        InputManager im = (InputManager) ctx.getSystemService(Context.INPUT_SERVICE);
        im.registerInputDeviceListener(this, handler);
        hasGamepad = detectGamepad();
        handler.post(poll);
    }

    // ------------------------------------------------------------ state

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            boolean g;
            try { g = nativeInGame(); } catch (UnsatisfiedLinkError e) { g = false; }
            if (g != ingame) { ingame = g; if (!g) releaseAll(); updateActive(); }
            handler.postDelayed(this, 150);
        }
    };

    private boolean overlayWanted() {
        if (mode == 2) return false;
        if (mode == 1) return ingame;
        return ingame && !hasGamepad;
    }

    private void updateActive() {
        boolean v = overlayWanted();
        if (v != visibleNow) {
            visibleNow = v;
            if (!v) releaseAll();
            try { nativeSetActive(v); } catch (UnsatisfiedLinkError ignored) { }
        }
        invalidate();
    }

    static boolean detectGamepad() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual()) continue;
            int s = d.getSources();
            if ((s & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                    || (s & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK)
                return true;
        }
        return false;
    }

    @Override public void onInputDeviceAdded(int id) { hasGamepad = detectGamepad(); updateActive(); }
    @Override public void onInputDeviceRemoved(int id) { hasGamepad = detectGamepad(); updateActive(); }
    @Override public void onInputDeviceChanged(int id) { hasGamepad = detectGamepad(); updateActive(); }

    private void cycleMode() {
        mode = (mode + 1) % 3;
        prefs.edit().putInt(PREF_MODE, mode).apply();
        updateActive();
    }

    // ------------------------------------------------------------ layout & draw

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        // Scale the layout down on small screens so the cluster fits.
        float s = Math.min(1f, h / (380f * density)) * density;
        for (Button b : buttons) place(b, w, h, s);
        place(toggle, w, h, s);
        stickR = 75 * s;
        stickCx = 40 * s + stickR + 20 * s;
        stickCy = h - stickR - 40 * s;
        stickHomeX = stickCx; stickHomeY = stickCy;
        knobX = stickCx; knobY = stickCy;
        text.setTextSize(13 * s);
    }

    private void place(Button b, int w, int h, float s) {
        b.px = b.fromRight ? w - b.cx * s : b.cx * s;
        b.py = b.fromBottom ? h - b.cy * s : b.cy * s;
        b.pr = b.r * s;
        float hr = b.pr * 1.25f; // generous hit area
        b.hit.set(b.px - hr, b.py - hr, b.px + hr, b.py + hr);
    }

    @Override
    protected void onDraw(Canvas c) {
        // Mode toggle is always reachable while in game.
        if (ingame) drawButton(c, toggle, mode == 0 ? "AUTO" : mode == 1 ? "ON" : "OFF");
        if (!visibleNow) return;

        fill.setColor(Color.argb(50, 255, 255, 255));
        stroke.setColor(Color.argb(110, 120, 220, 255));
        c.drawCircle(stickCx, stickCy, stickR, fill);
        c.drawCircle(stickCx, stickCy, stickR, stroke);
        fill.setColor(Color.argb(stickActive ? 150 : 90, 120, 220, 255));
        c.drawCircle(knobX, knobY, stickR * 0.42f, fill);

        for (Button b : buttons) drawButton(c, b, b.label);
    }

    private void drawButton(Canvas c, Button b, String label) {
        fill.setColor(b.down ? Color.argb(150, 120, 220, 255) : Color.argb(55, 255, 255, 255));
        stroke.setColor(Color.argb(120, 120, 220, 255));
        c.drawCircle(b.px, b.py, b.pr, fill);
        c.drawCircle(b.px, b.py, b.pr, stroke);
        c.drawText(label, b.px, b.py - (text.descent() + text.ascent()) / 2, text);
    }

    // ------------------------------------------------------------ input

    private Button hitButton(float x, float y) {
        if (ingame && toggle.hit.contains(x, y)) return toggle;
        if (!visibleNow) return null;
        Button best = null; float bestD = Float.MAX_VALUE;
        for (Button b : buttons) {
            if (!b.hit.contains(x, y)) continue;
            float d = (x - b.px) * (x - b.px) + (y - b.py) * (y - b.py);
            if (d < bestD) { bestD = d; best = b; }
        }
        return best;
    }

    private void press(Button b, boolean down) {
        if (b.down == down) return;
        b.down = down;
        if (b == toggle) { if (down) cycleMode(); return; }
        if (b.id == TRIG_RIGHT) nativeSetAxis(AXIS_TRIGGERRIGHT, down ? 1f : 0f);
        else if (b.id == TRIG_LEFT) nativeSetAxis(AXIS_TRIGGERLEFT, down ? 1f : 0f);
        else nativeSetButton(b.id, down);
    }

    private void moveStick(float x, float y) {
        float dx = x - stickCx, dy = y - stickCy;
        float len = (float) Math.hypot(dx, dy);
        if (len > stickR) { dx = dx / len * stickR; dy = dy / len * stickR; }
        knobX = stickCx + dx; knobY = stickCy + dy;
        nativeSetAxis(AXIS_LEFTX, dx / stickR);
        nativeSetAxis(AXIS_LEFTY, dy / stickR);
    }

    private void releaseStick() {
        stickActive = false;
        stickCx = stickHomeX; stickCy = stickHomeY;
        knobX = stickCx; knobY = stickCy;
        try { nativeSetAxis(AXIS_LEFTX, 0); nativeSetAxis(AXIS_LEFTY, 0); } catch (UnsatisfiedLinkError ignored) { }
    }

    private void releaseAll() {
        for (int i = 0; i < owners.size(); i++) {
            Object o = owners.valueAt(i);
            if (o instanceof Button) { try { press((Button) o, false); } catch (UnsatisfiedLinkError ignored) { } }
        }
        owners.clear();
        lookLast.clear();
        if (stickActive) releaseStick();
        toggle.down = false;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        int action = e.getActionMasked();
        // Not in game: let the SDL surface underneath get the touch (menu / mouse emulation),
        // except for the mode toggle.
        if (!visibleNow && owners.size() == 0) {
            if (!(ingame && action == MotionEvent.ACTION_DOWN && toggle.hit.contains(e.getX(), e.getY())))
                return false;
        }

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                int i = e.getActionIndex();
                int id = e.getPointerId(i);
                float x = e.getX(i), y = e.getY(i);
                Button b = hitButton(x, y);
                if (b != null) {
                    owners.put(id, b); press(b, true);
                } else if (!visibleNow) {
                    // ignore
                } else if (!stickActive && x < getWidth() * 0.45f && y > getHeight() * 0.3f) {
                    owners.put(id, STICK); stickActive = true;
                    // Floating stick: recenter under the thumb if touched outside the ring.
                    if (Math.hypot(x - stickCx, y - stickCy) > stickR) {
                        stickCx = Math.max(stickR, Math.min(getWidth() * 0.45f, x));
                        stickCy = Math.max(stickR, Math.min(getHeight() - stickR, y));
                    }
                    moveStick(x, y);
                } else if (x >= getWidth() * 0.45f || y <= getHeight() * 0.3f) {
                    owners.put(id, LOOK); lookLast.put(id, new float[] { x, y });
                }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < e.getPointerCount(); i++) {
                    int id = e.getPointerId(i);
                    Object o = owners.get(id);
                    float x = e.getX(i), y = e.getY(i);
                    if (o == STICK) {
                        moveStick(x, y);
                    } else if (o == LOOK) {
                        float[] last = lookLast.get(id);
                        float dx = (x - last[0]) * lookScale, dy = (y - last[1]) * lookScale;
                        last[0] = x; last[1] = y;
                        if (dx != 0 || dy != 0)
                            nativeAddLook(dx, dy);
                    } else if (o instanceof Button) {
                        // Slide off a button releases it; sliding onto FIRE keeps aiming natural.
                        Button b = (Button) o;
                        if (!b.hit.contains(x, y) && b != toggle) { press(b, false); owners.remove(id); }
                    }
                }
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                if (action == MotionEvent.ACTION_CANCEL) { releaseAll(); invalidate(); return true; }
                int id = e.getPointerId(e.getActionIndex());
                Object o = owners.get(id);
                if (o == STICK) releaseStick();
                else if (o instanceof Button) press((Button) o, false);
                owners.remove(id);
                lookLast.remove(id);
                invalidate();
                return true;
            }
        }
        return true;
    }

    void shutdown() {
        handler.removeCallbacks(poll);
        ((InputManager) getContext().getSystemService(Context.INPUT_SERVICE)).unregisterInputDeviceListener(this);
    }
}
