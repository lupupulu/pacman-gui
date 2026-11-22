/*
 * Copyright (C) 2026 Zhang Cw and pacman-gui contributors
 *
 * This file is part of pacman-gui.
 *
 * pacman-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pacman-gui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with pacman-gui.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef SDLGUI_H
#define SDLGUI_H

#include <stddef.h>
#include "theme.h"
#include "layout.h"

#ifndef SDL_render_h_
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
#endif
#ifndef SDL_video_h_
typedef struct SDL_Window SDL_Window;
#endif
#ifndef SDL_events_h_
typedef union SDL_Event SDL_Event;
#endif

/* 主题搜索路径的兜底值。运行时还会自动加入可执行文件/应用包里的 assets 目录、
 * 以及当前平台的用户/系统主题目录；设置 SDLGUI_THEME_PATH 环境变量会完全覆盖。 */
#ifndef SDLGUI_THEME_PATH
#if defined(__APPLE__)
#define SDLGUI_THEME_PATH "~/Library/Application Support/sdlgui:/Library/Application Support/sdlgui:/opt/homebrew/share/sdlgui:/usr/local/share/sdlgui"
#else
#define SDLGUI_THEME_PATH "/usr/share/sdlgui:~/.local/share/sdlgui:~/.config/sdlgui"
#endif
#endif


/* theme: 主题名。先在主题搜索路径的 theme.toml 中查同名键，命中时用其值作为
 * 实际主题名；theme 为 NULL/空串时用 "Default" 作为键。
 * 然后查找 <主题名>/config.toml，找不到再回退 Default/config.toml，
 * 仍然找不到则使用内置主题。 */
void sdlgui_init(const char *title,int width,int height,const char *iconpath,const char *theme);
/* 独立的主题加载入口：theme.toml 映射 -> 加载主题配置 -> 系统亮/暗选择。
 * 由 sdlgui_init() 调用；也可在 theme_init() 之后单独调用。 */
void sdlgui_theme_init(const char *theme);
void sdlgui_quit(void);

void set_window(SDL_Window *w);
SDL_Window *get_window(void);
void set_renderer(SDL_Renderer *r);
SDL_Renderer *get_renderer(void);

void set_mouse_pos(float x,float y);
float get_mouse_x(void);
float get_mouse_y(void);

/* 滚轮/触摸板滚动增量，正方向与 SDL_MouseWheelEvent 一致；
 * 累积到下一次 update_root() 时由鼠标下的可滑动列表消费。 */
void set_mouse_wheel(float x,float y);
float get_mouse_wheel_x(void);
float get_mouse_wheel_y(void);
#define get_mouse_wheel get_mouse_wheel_y

/* 五个鼠标按键状态：1=按下，0=未按下（左 中 右 侧键X1 侧键X2） */
void set_mouse_button(int left,int mid,int right,int x1,int x2);
int get_mouse_left(void);
int get_mouse_mid(void);
int get_mouse_right(void);
int get_mouse_x1(void);
int get_mouse_x2(void);

/* 处理一个 SDL 事件；返回 0 表示应用应退出，1 表示继续。
 * SDL_PollEvent 仍由调用方主循环负责。 */
int sdlgui_handle_event(const SDL_Event *event);
/* 清屏、更新布局、绘制并 present 一帧；主循环和 live resize 共用。 */
void sdlgui_render_frame(void);
/* macOS 拖动窗口边缘时的 live resize 事件 watch。 */
void sdlgui_enable_live_resize(void);
void sdlgui_disable_live_resize(void);


/* 逻辑坐标（窗口点）到物理像素的比例：普通屏幕为 1.0，Retina 为 2.0。
 * 布局和绘制都用逻辑坐标，字体在内部按这个比例栅格化，保证高分屏清晰。 */
float sdlgui_get_scale(void);

#endif
