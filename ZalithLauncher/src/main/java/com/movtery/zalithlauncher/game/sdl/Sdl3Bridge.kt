/*
 * Zalith Launcher 2
 * Copyright (C) 2025 MovTery <movtery228@qq.com> and contributors
 *
 * SDL3 手柄事件注入桥 (JNI)
 * 把手柄事件同步注入 SDL3 事件队列, 使纯 SDL3 手柄模组
 * (Controlify2.0/ControlFlex/MidnightControls) 能读到手柄。
 */
package com.movtery.zalithlauncher.game.sdl

import android.annotation.SuppressLint
import com.movtery.zalithlauncher.setting.AllSettings

@SuppressLint("JvmStaticInCompanion")
object Sdl3Bridge {
    @Volatile
    private var inited = false

    /**
     * 手动开关是否允许启用 SDL3 注入。
     * 由设置项 sdl3Compat 控制: 关闭时 Sdl3Bridge 不初始化、不注入任何事件。
     */
    fun isEnabled(): Boolean = AllSettings.sdl3Compat.state

    /** 解析 libSDL3.so 函数指针(幂等), 受 sdl3Compat 手动开关控制 */
    fun init(): Boolean {
        if (!isEnabled()) return false
        if (inited) return true
        inited = nativeInit()
        return inited
    }

    /** 手柄按键注入 SDL3 (glfwButton: GLFW 编码 0..14), 受手动开关控制 */
    fun injectGamepadButton(glfwButton: Int, pressed: Boolean) {
        if (!isEnabled() || !inited) return
        nativeInjectGamepadButton(glfwButton, if (pressed) 1 else 0)
    }

    /** 手柄摇杆/扳机注入 SDL3 (glfwAxis: GLFW 编码 0..5, value: -1..1), 受手动开关控制 */
    fun injectGamepadAxis(glfwAxis: Int, value: Float) {
        if (!isEnabled() || !inited) return
        nativeInjectGamepadAxis(glfwAxis, value)
    }

    @JvmStatic
    private external fun nativeInit(): Boolean

    @JvmStatic
    private external fun nativeInjectGamepadButton(glfwButton: Int, pressed: Int)

    @JvmStatic
    private external fun nativeInjectGamepadAxis(glfwAxis: Int, value: Float)
}
