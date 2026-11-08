/*
 * On-screen virtual gamepad bridge.
 *
 * The Java overlay (TouchControlsView) drives an SDL *virtual* game controller,
 * so touch input flows through exactly the same path (and the same X360_* binds
 * in android.cfg) as a physical controller. Engine-side work happens on the
 * engine thread in Android_TouchPad_Frame(); the JNI setters only touch SDL's
 * virtual-joystick API, which is internally locked.
 */
#include "quakedef.h"
#include <SDL.h>
#include <jni.h>
#include <stdatomic.h>

static SDL_Joystick *_Atomic vpad;          // opened handle to our virtual device
static SDL_JoystickID vpad_instance = -1;
static atomic_int overlay_active;            // set by Java
static atomic_int engine_ingame;             // read by Java
static SDL_SpinLock look_lock;
static float look_dx, look_dy;               // accumulated drag-to-aim pixels

static void TouchPad_Attach(void)
{
	SDL_VirtualJoystickDesc desc;
	int index;
	if (vpad || !SDL_WasInit(SDL_INIT_JOYSTICK))
		return;
	SDL_zero(desc);
	desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	desc.naxes = SDL_CONTROLLER_AXIS_MAX;
	desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
	desc.name = "Xonotic Touch Controls";
	index = SDL_JoystickAttachVirtualEx(&desc);
	if (index < 0)
	{
		Con_Printf(CON_WARN "Touch controls: SDL_JoystickAttachVirtualEx failed: %s\n", SDL_GetError());
		return;
	}
	vpad = SDL_JoystickOpen(index);
	if (vpad)
		vpad_instance = SDL_JoystickInstanceID(vpad);
}

/* Called once per frame on the engine thread, before VID_EnableJoystick(). */
void Android_TouchPad_Frame(void)
{
	int i, n, want = -1, firstphysical = -1;

	TouchPad_Attach();

	atomic_store(&engine_ingame,
		cls.state == ca_connected && key_dest == key_game && !key_consoleactive);

	if (!vpad)
		return;

	n = SDL_NumJoysticks();
	for (i = 0; i < n; i++)
	{
		if (SDL_JoystickGetDeviceInstanceID(i) == vpad_instance)
		{
			if (atomic_load(&overlay_active))
				want = i;
		}
		else if (firstphysical < 0)
			firstphysical = i;
	}
	if (want < 0)
		want = firstphysical >= 0 ? firstphysical : 0;
	if (joy_index.integer != want)
		Cvar_SetValueQuick(&joy_index, want);
}

JNIEXPORT void JNICALL
Java_org_xonotic_android_TouchControlsView_nativeSetActive(JNIEnv *env, jclass cls_, jboolean active)
{
	atomic_store(&overlay_active, active ? 1 : 0);
}

JNIEXPORT jboolean JNICALL
Java_org_xonotic_android_TouchControlsView_nativeInGame(JNIEnv *env, jclass cls_)
{
	return atomic_load(&engine_ingame) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_xonotic_android_TouchControlsView_nativeSetAxis(JNIEnv *env, jclass cls_, jint axis, jfloat value)
{
	SDL_Joystick *j = atomic_load(&vpad);
	if (value > 1.0f) value = 1.0f;
	if (value < -1.0f) value = -1.0f;
	if (j)
		SDL_JoystickSetVirtualAxis(j, axis, (Sint16)(value * 32767.0f));
}

JNIEXPORT void JNICALL
Java_org_xonotic_android_TouchControlsView_nativeSetButton(JNIEnv *env, jclass cls_, jint button, jboolean down)
{
	SDL_Joystick *j = atomic_load(&vpad);
	if (j)
		SDL_JoystickSetVirtualButton(j, button, down ? SDL_PRESSED : SDL_RELEASED);
}

/* Engine thread, from IN_Move(): consume accumulated drag-to-aim motion. */
void Android_TouchPad_Look(float *dx, float *dy)
{
	SDL_AtomicLock(&look_lock);
	if (atomic_load(&overlay_active))
	{
		*dx += look_dx;
		*dy += look_dy;
	}
	look_dx = look_dy = 0;
	SDL_AtomicUnlock(&look_lock);
}

JNIEXPORT void JNICALL
Java_org_xonotic_android_TouchControlsView_nativeAddLook(JNIEnv *env, jclass cls_, jfloat dx, jfloat dy)
{
	SDL_AtomicLock(&look_lock);
	look_dx += dx;
	look_dy += dy;
	SDL_AtomicUnlock(&look_lock);
}
