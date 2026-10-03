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

@SuppressLint("JvmStaticInCompanion")
object Sdl3Bridge {
    @Volatile
    private var inited = false

    /** 解析 libSDL3.so 函数指针(幂等) */
    fun init(): Boolean {
        if (inited) return true
        inited = nativeInit()
        return inited
    }

    /** 手柄按键注入 SDL3 (glfwButton: GLFW 编码 0..14) */
    fun injectGamepadButton(glfwButton: Int, pressed: Boolean) {
        if (!inited) return
        nativeInjectGamepadButton(glfwButton, if (pressed) 1 else 0)
    }

    /** 手柄摇杆/扳机注入 SDL3 (glfwAxis: GLFW 编码 0..5, value: -1..1) */
    fun injectGamepadAxis(glfwAxis: Int, value: Float) {
        if (!inited) return
        nativeInjectGamepadAxis(glfwAxis, value)
    }

    @JvmStatic
    private external fun nativeInit(): Boolean

    @JvmStatic
    private external fun nativeInjectGamepadButton(glfwButton: Int, pressed: Int)

    @JvmStatic
    private external fun nativeInjectGamepadAxis(glfwAxis: Int, value: Float)
}
