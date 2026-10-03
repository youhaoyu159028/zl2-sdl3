/*
 * sdl3_gamepad.c — SDL3 手柄事件注入桥 (JNI)
 * 目标基线: ZalithLauncher2 main (2.6.1, sdl3-migrate)
 *
 * 问题背景: main 的 SdlDirect 手柄路径把手柄事件写进 GLFW gamepad buffer
 *          (DirectGamepad.kt -> CallbackBridge.sGamepadButtonBuffer), 并 dlopen
 *          libSDL3.so 初始化了 SDL3 子系统, 但 SDL3 手柄事件队列是空的 ——
 *          纯 SDL3 手柄模组 (Controlify2.0/ControlFlex/MidnightControls)
 *          通过 SDL3 Gamepad API 读不到手柄。
 *
 * 本桥: 暴露 JNI, 让启动器输入层把手柄按键/摇杆事件同步注入 SDL3 事件队列,
 *       (SDL_SendGamepadButton / SDL_SendGamepadAxis), 使 SDL3 模组能读到手柄。
 *       只补 SDL3 手柄事件, 不动窗口/渲染/SDL2 原有链路。
 *
 * 依赖: SDL3 头文件 + libSDL3.so (官方完整版 arm64-v8a, 由 sdl_hook 已 dlopen)
 */
#include <jni.h>
#include <string.h>
#include <dlfcn.h>
#include <android/log.h>
#include <SDL3/SDL.h>

#define LOG_TAG "Sdl3Gamepad"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/* ---- 通过 dlopen 动态解析 SDL3 函数, 不静态链接, 避免与工程现有符号冲突 ---- */
static void *sdl3_handle = NULL;

static SDL_Gamepad *(*fn_SDL_GetGamepadFromID)(SDL_JoystickID instance_id);
static SDL_JoystickID *(*fn_SDL_GetGamepads)(int *count);
static int (*fn_SDL_SendGamepadButton)(SDL_Gamepad *gamepad, SDL_GamepadButton button, bool down);
static int (*fn_SDL_SendGamepadAxis)(SDL_Gamepad *gamepad, SDL_GamepadAxis axis, Sint16 value);

static SDL_Gamepad *g_pad = NULL;

/* GLFW 手柄编码 (与 DirectGamepad.kt 一致) -> SDL3 GamepadButton */
static SDL_GamepadButton glfw_to_sdl_button(int glfw) {
    switch (glfw) {
        case 0:  return SDL_GAMEPAD_BUTTON_SOUTH;      /* A */
        case 1:  return SDL_GAMEPAD_BUTTON_EAST;       /* B */
        case 2:  return SDL_GAMEPAD_BUTTON_WEST;       /* X */
        case 3:  return SDL_GAMEPAD_BUTTON_NORTH;      /* Y */
        case 4:  return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        case 5:  return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
        case 6:  return SDL_GAMEPAD_BUTTON_BACK;
        case 7:  return SDL_GAMEPAD_BUTTON_START;
        case 9:  return SDL_GAMEPAD_BUTTON_LEFT_STICK;
        case 10: return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
        case 11: return SDL_GAMEPAD_BUTTON_DPAD_UP;
        case 12: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
        case 13: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
        case 14: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
        default: return (SDL_GamepadButton)-1;
    }
}

/* GLFW 轴编码 (与 DirectGamepad.kt 一致) -> SDL3 GamepadAxis */
static SDL_GamepadAxis glfw_to_sdl_axis(int glfw) {
    switch (glfw) {
        case 0: return SDL_GAMEPAD_AXIS_LEFTX;
        case 1: return SDL_GAMEPAD_AXIS_LEFTY;
        case 2: return SDL_GAMEPAD_AXIS_RIGHTX;
        case 3: return SDL_GAMEPAD_AXIS_RIGHTY;
        case 4: return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        case 5: return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        default: return (SDL_GamepadAxis)-1;
    }
}

static void ensure_gamepad(void) {
    if (g_pad || !fn_SDL_GetGamepads) return;
    int count = 0;
    SDL_JoystickID *ids = fn_SDL_GetGamepads(&count);
    if (ids != NULL && count > 0 && fn_SDL_GetGamepadFromID) {
        g_pad = fn_SDL_GetGamepadFromID(ids[0]);
        if (g_pad) LOGI("SDL3 gamepad bound (id=%u)", ids[0]);
    }
}

/*
 * JNI: 手柄按键事件注入 SDL3
 * glfwButton: GLFW gamepad 按钮编号 (0..14, 与 DirectGamepad.kt 一致)
 * pressed:    1 按下 / 0 释放
 */
JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_game_sdl_Sdl3Bridge_injectGamepadButton(JNIEnv *env, jclass clazz,
                                                                        jint glfwButton, jint pressed) {
    if (!fn_SDL_SendGamepadButton) return;
    ensure_gamepad();
    if (!g_pad) return;
    SDL_GamepadButton b = glfw_to_sdl_button(glfwButton);
    if ((int)b == -1) return;
    fn_SDL_SendGamepadButton(g_pad, b, pressed != 0);
}

/*
 * JNI: 手柄摇杆/扳机事件注入 SDL3
 * glfwAxis:  GLFW gamepad 轴编号 (0..5, 与 DirectGamepad.kt 一致)
 * valueNorm: 归一化值 -1.0..1.0 (DirectGamepad 传入)
 */
JNIEXPORT void JNICALL
Java_com_movtery_zalithlauncher_game_sdl_Sdl3Bridge_injectGamepadAxis(JNIEnv *env, jclass clazz,
                                                                      jint glfwAxis, jfloat valueNorm) {
    if (!fn_SDL_SendGamepadAxis) return;
    ensure_gamepad();
    if (!g_pad) return;
    SDL_GamepadAxis a = glfw_to_sdl_axis(glfwAxis);
    if ((int)a == -1) return;
    Sint16 raw = (Sint16)(valueNorm * 32767.0f);
    if (raw > 32767) raw = 32767;
    if (raw < -32768) raw = -32768;
    fn_SDL_SendGamepadAxis(g_pad, a, raw);
}

/*
 * JNI: 解析 libSDL3.so 函数指针。须在首次注入前调用(幂等)。
 */
JNIEXPORT jboolean JNICALL
Java_com_movtery_zalithlauncher_game_sdl_Sdl3Bridge_init(JNIEnv *env, jclass clazz) {
    if (sdl3_handle != NULL) return JNI_TRUE;
    sdl3_handle = dlopen("libSDL3.so", RTLD_NOW);
    if (sdl3_handle == NULL) {
        LOGW("dlopen libSDL3.so failed: %s", dlerror());
        return JNI_FALSE;
    }
    fn_SDL_GetGamepadFromID = (SDL_Gamepad *(*)(SDL_JoystickID))dlsym(sdl3_handle, "SDL_GetGamepadFromID");
    fn_SDL_GetGamepads = (SDL_JoystickID *(*)(int *))dlsym(sdl3_handle, "SDL_GetGamepads");
    fn_SDL_SendGamepadButton = (int (*)(SDL_Gamepad *, SDL_GamepadButton, bool))dlsym(sdl3_handle, "SDL_SendGamepadButton");
    fn_SDL_SendGamepadAxis = (int (*)(SDL_Gamepad *, SDL_GamepadAxis, Sint16))dlsym(sdl3_handle, "SDL_SendGamepadAxis");
    if (!fn_SDL_GetGamepadFromID || !fn_SDL_GetGamepads || !fn_SDL_SendGamepadButton || !fn_SDL_SendGamepadAxis) {
        LOGW("SDL3 gamepad symbols missing (slim lib?): %s", dlerror());
        return JNI_FALSE;
    }
    LOGI("SDL3 gamepad bridge initialized");
    return JNI_TRUE;
}
