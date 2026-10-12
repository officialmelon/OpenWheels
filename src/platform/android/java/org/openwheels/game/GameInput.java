// PAD (PC addition): game controllers, haptics and tilt steering on Android (not part of the
// original game). The native side is src/platform/android/AndroidGamepad.cpp; the shared logic
// is in src/input/ (see src/input/Gamepad.h).
//
//   * Controllers: every input device with gamepad buttons or joystick axes - Bluetooth and USB
//     pads and the built-in controls of Android handhelds (AYN Odin / Thor, Retroid, Anbernic...).
//     AppActivity hands their key and motion events here before cocos2d-x sees them; they are
//     forwarded to the native side (any thread: it queues them for the GL thread) and consumed,
//     so a pad's B never turns into the system Back and its d-pad never moves Android's focus.
//     KEYCODE_BACK itself is left alone (the game's own back handling).
//   * Rumble: the controller's own vibrator when it has one, else - for a handheld's built-in
//     controls, or when "phone vibration" is on - the device's vibrator.
//   * Tilt: the gravity sensor (fused with the gyroscope on devices that have one; the plain
//     accelerometer otherwise), turned into the screen's roll for the current display rotation.
package org.openwheels.game;

import android.app.Activity;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.WindowManager;

final class GameInput implements InputManager.InputDeviceListener, SensorEventListener {

    // The axes the native side reads, in this order (AndroidGamepad.cpp kAxes).
    static final int[] AXES = {
        MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ,
        MotionEvent.AXIS_RX, MotionEvent.AXIS_RY, MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y,
        MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_BRAKE, MotionEvent.AXIS_GAS,
    };

    private static native void nativeGamepadConnected(int deviceId, String name);
    private static native void nativeGamepadRemoved(int deviceId);
    private static native void nativeGamepadKey(int deviceId, int keyCode, boolean down);
    private static native void nativeGamepadAxes(int deviceId, float[] values);
    private static native void nativeTilt(float roll);

    private static GameInput sInstance;

    private final Activity mActivity;
    private final InputManager mInputManager;
    private final SensorManager mSensorManager;
    private Sensor mTiltSensor;
    private boolean mTiltWanted;
    private boolean mTiltRegistered;
    private boolean mResumed;
    private final float[] mAxisValues = new float[AXES.length];

    private GameInput(Activity activity) {
        mActivity = activity;
        mInputManager = (InputManager) activity.getSystemService(Context.INPUT_SERVICE);
        mSensorManager = (SensorManager) activity.getSystemService(Context.SENSOR_SERVICE);
        if (mSensorManager != null) {
            mTiltSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_GRAVITY);
            if (mTiltSensor == null) mTiltSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        }
    }

    static synchronized GameInput install(Activity activity) {
        if (sInstance == null) sInstance = new GameInput(activity);
        return sInstance;
    }

    // ---- controllers ------------------------------------------------------------------------------

    static boolean isGameController(InputDevice device) {
        if (device == null || device.isVirtual()) return false;
        final int sources = device.getSources();
        if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            // Some phones tag odd key devices as gamepads: require the face buttons.
            if (Build.VERSION.SDK_INT >= 19) {
                boolean[] has = device.hasKeys(KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_B);
                if (has[0] || has[1]) return true;
            } else {
                return true;
            }
        }
        if ((sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
            return device.getMotionRange(MotionEvent.AXIS_X) != null && device.getMotionRange(MotionEvent.AXIS_Y) != null;
        }
        return false;
    }

    private static boolean fromController(int source, InputDevice device) {
        final boolean padSource = (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
                || (source & InputDevice.SOURCE_DPAD) == InputDevice.SOURCE_DPAD;
        return padSource && isGameController(device);
    }

    private static boolean isControllerKey(int keyCode) {
        if (keyCode == KeyEvent.KEYCODE_BACK) return false;  // the game's own back handling
        if (KeyEvent.isGamepadButton(keyCode)) return true;
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_CENTER:
                return true;
            default:
                return false;
        }
    }

    static String deviceName(InputDevice device) {
        String name = device != null ? device.getName() : null;
        return name != null ? name : "controller";
    }

    void start() {
        if (mInputManager != null) {
            mInputManager.registerInputDeviceListener(this, null);
            for (int id : mInputManager.getInputDeviceIds()) onInputDeviceAdded(id);
        }
    }

    /** AppActivity.dispatchKeyEvent: true when consumed. */
    boolean onKeyEvent(KeyEvent event) {
        if (!isControllerKey(event.getKeyCode()) || !fromController(event.getSource(), event.getDevice())) return false;
        final int action = event.getAction();
        if (action == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
            nativeGamepadKey(event.getDeviceId(), event.getKeyCode(), true);
        } else if (action == KeyEvent.ACTION_UP) {
            nativeGamepadKey(event.getDeviceId(), event.getKeyCode(), false);
        }
        return true;
    }

    /** AppActivity.dispatchGenericMotionEvent: true when consumed. */
    boolean onMotionEvent(MotionEvent event) {
        final int source = event.getSource();
        if ((source & InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK
                || event.getActionMasked() != MotionEvent.ACTION_MOVE) {
            return false;
        }
        final InputDevice device = event.getDevice();
        if (!isGameController(device)) return false;
        for (int i = 0; i < AXES.length; i++) {
            float v = event.getAxisValue(AXES[i]);
            // The device's own dead zone ("flat") around the centre of the sticks.
            InputDevice.MotionRange range = device.getMotionRange(AXES[i], source);
            if (range != null && Math.abs(v) <= range.getFlat()) v = 0.0f;
            mAxisValues[i] = v;
        }
        nativeGamepadAxes(event.getDeviceId(), mAxisValues);
        return true;
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        InputDevice device = InputDevice.getDevice(deviceId);
        if (isGameController(device)) nativeGamepadConnected(deviceId, deviceName(device));
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        nativeGamepadRemoved(deviceId);
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        InputDevice device = InputDevice.getDevice(deviceId);
        if (isGameController(device)) nativeGamepadConnected(deviceId, deviceName(device));
        else nativeGamepadRemoved(deviceId);
    }

    // ---- rumble ---------------------------------------------------------------------------------------

    private static Vibrator deviceVibrator(InputDevice device) {
        if (Build.VERSION.SDK_INT >= 31) {
            VibratorManager manager = device.getVibratorManager();
            int[] ids = manager.getVibratorIds();
            return ids.length > 0 ? manager.getVibrator(ids[0]) : null;
        }
        return device.getVibrator();
    }

    private static Vibrator systemVibrator(Context context) {
        if (Build.VERSION.SDK_INT >= 31) {
            VibratorManager manager = (VibratorManager) context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            return manager != null ? manager.getDefaultVibrator() : null;
        }
        return (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
    }

    private static void play(Vibrator vibrator, float strength, int ms) {
        if (vibrator == null || !vibrator.hasVibrator()) return;
        if (strength <= 0.0f) {
            vibrator.cancel();
            return;
        }
        if (Build.VERSION.SDK_INT >= 26) {
            int amplitude = vibrator.hasAmplitudeControl()
                    ? Math.max(1, Math.min(255, Math.round(strength * 255.0f)))
                    : VibrationEffect.DEFAULT_AMPLITUDE;
            // Without amplitude control a weak hit is a shorter buzz instead.
            int length = vibrator.hasAmplitudeControl() ? ms : Math.max(15, Math.round(ms * strength));
            vibrator.vibrate(VibrationEffect.createOneShot(length, amplitude));
        } else {
            vibrator.vibrate(Math.max(15, Math.round(ms * strength)));
        }
    }

    /**
     * Native (GL thread): strength 0..1 for ms milliseconds, 0 = stop. phoneAllowed: the "phone
     * vibration" option is on.
     */
    static void rumble(float strength, int ms, boolean phoneAllowed) {
        final GameInput self = sInstance;
        if (self == null) return;
        try {
            boolean played = false;
            boolean builtIn = false;
            for (int id : InputDevice.getDeviceIds()) {
                InputDevice device = InputDevice.getDevice(id);
                if (!isGameController(device)) continue;
                if (Build.VERSION.SDK_INT >= 29 && !device.isExternal()) builtIn = true;
                Vibrator vibrator = deviceVibrator(device);
                if (vibrator != null && vibrator.hasVibrator()) {
                    play(vibrator, strength, ms);
                    played = true;
                }
            }
            // A handheld's built-in controls have no vibrator of their own: the device's is theirs.
            if (!played && (builtIn || phoneAllowed)) {
                play(systemVibrator(self.mActivity), strength, ms);
            }
        } catch (RuntimeException e) {
            // no permission / vibrator service: no rumble
        }
    }

    // ---- tilt -------------------------------------------------------------------------------------------

    /** Native (GL thread): start / stop the tilt sensor. */
    static void setTiltSensor(final boolean on) {
        final GameInput self = sInstance;
        if (self == null) return;
        self.mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                self.mTiltWanted = on;
                self.updateTiltRegistration();
            }
        });
    }

    void onResume() {
        mResumed = true;
        updateTiltRegistration();
    }

    void onPause() {
        mResumed = false;
        updateTiltRegistration();
    }

    private void updateTiltRegistration() {
        final boolean want = mTiltWanted && mResumed && mTiltSensor != null;
        if (want == mTiltRegistered || mSensorManager == null) return;
        mTiltRegistered = want;
        if (want) mSensorManager.registerListener(this, mTiltSensor, SensorManager.SENSOR_DELAY_GAME);
        else mSensorManager.unregisterListener(this);
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        // Device axes: x right, y up (portrait); the reading points away from the ground.
        float x = event.values[0], y = event.values[1];
        float sx, sy;  // the same in screen axes
        int rotation = Surface.ROTATION_0;
        WindowManager wm = mActivity.getWindowManager();
        if (wm != null) rotation = wm.getDefaultDisplay().getRotation();
        switch (rotation) {
            case Surface.ROTATION_90: sx = -y; sy = x; break;
            case Surface.ROTATION_180: sx = -x; sy = -y; break;
            case Surface.ROTATION_270: sx = y; sy = -x; break;
            default: sx = x; sy = y; break;
        }
        // Held level, "up" on the screen points away from the ground: roll 0. Turned
        // counter-clockwise (left side down) the reading leans to +x: positive roll.
        nativeTilt((float) Math.atan2(sx, sy));
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }
}
