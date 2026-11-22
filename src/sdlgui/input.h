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

#ifndef SDLGUI_INPUT_H
#define SDLGUI_INPUT_H

#include <stddef.h>
#include <SDL3/SDL.h>
#include "layout.h"

/* 输入框布局处理鼠标形状、焦点、键盘输入和文字 draft；
 * 背景与光标由外部绘制，文字通过内部 draft 绘制。
 * 外部可以通过 layout_input_text()/layout_input_cursor()/layout_input_is_focused()
 * 读取状态。 */

typedef void (*input_callback_func)(const char *text,void *data);

typedef struct {
    layout_t l;

    char *text;       // 以 '\0' 结尾的 UTF-8 文本，内部持有
    size_t length;    // 字节长度
    size_t capacity;  // 缓冲区容量
    size_t cursor;    // 光标字节下标，总在 UTF-8 字符边界
    size_t max_bytes; // 允许的最大字节数，0=不限制

    TTF_Font *font;
    unsigned text_color;
    draft_text_t *text_draft; // 输入文字 draft
    float text_width;         // 当前文字的逻辑宽度
    float text_scroll;        // 文字横向滚动偏移

    input_callback_func callback;
    void *data;

    unsigned focused:1;
    unsigned last_left:1;
}layout_input_t;

/* 创建输入框布局；text 可为 NULL，font 必须非 NULL；
 * max_bytes=0 表示不限制文本字节数。 */
layout_input_t *create_layout_input(void *parent,float w,float h,align_t align,
    const char *text,TTF_Font *font,unsigned color,size_t max_bytes,
    input_callback_func callback,void *data);

/* 把 SDL 输入事件交给当前焦点输入框；处理了返回 1，未处理返回 0。
 * 需要主循环里对每个 SDL_Event 调用一次。 */
int layout_input_event(const SDL_Event *event);

void layout_input_focus(layout_input_t *input);
void layout_input_blur(void);
const char *layout_input_text(const layout_input_t *input);
size_t layout_input_cursor(const layout_input_t *input);
int layout_input_is_focused(const layout_input_t *input);

/* ---- 程序化编辑，供 widget 层的输入框控件组合使用 ---- */

/* 光标前缀的逻辑宽度；文字实际从 l.x-text_scroll 开始绘制。 */
float layout_input_prefix_width(const layout_input_t *input);
/* 替换全部文字：按 max_bytes 截断到字符边界，光标夹进文本范围并滚到可见处。 */
void layout_input_set_text(layout_input_t *input,const char *text);
/* 设置光标字节下标；会自动对齐到字符边界，并保证光标在文字区内可见。 */
void layout_input_set_cursor(layout_input_t *input,size_t cursor);
/* 按窗口横坐标就近定位光标，用于鼠标点击/拖拽。 */
void layout_input_set_cursor_at(layout_input_t *input,float x);
/* 调整 text_scroll，让当前光标落在文字区内；不会响应横向滚轮，只跟随光标。 */
void layout_input_scroll_into_view(layout_input_t *input);


/* 释放输入框的全局焦点、悬停和光标资源；sdlgui_quit() 调用。 */
void input_quit(void);
/* 根布局鼠标遍历时处理“点击输入框外失焦”。 */
void layout_input_root_mouse(void);

/* ---- 输入框控件 ----
 * 在 layout_input 的文字/焦点/键盘/输入法之上补全外观与交互：
 * 圆角背景、焦点边框渐变、占位文字、闪烁光标、左侧图标、清除按钮。
 * 点击文字区会把光标移到最近的字符处，拖拽可继续移动；点击清除按钮清空内容。
 * 文本变化（键盘/粘贴/清除按钮/set_text）后调用 change_callback；
 * 回车调用 submit_callback。 */

typedef void (*input_change_func)(const char *text,void *data);

typedef struct {
    layout_t l;

    layout_input_t *input;       /* 文字、焦点、键盘与输入法，归控件所有 */
    layout_t *placeholder_box;   /* 占位文字区域，裁剪到文字区；先于文字绘制 */
    layout_t *overlay;           /* 光标与清除按钮；最后绘制，盖住文字 */

    draft_rrect_t *box;          /* 背景 + 边框 */
    draft_rrect_t *clear_bg;     /* 清除按钮的悬停圆底 */
    draft_texture_t *icon;       /* 左侧图标，可为 NULL */
    draft_text_t *placeholder;
    draft_texture_t *clear_icon; /* 程序化生成的 × */
    char *placeholder_text;      /* 占位文字副本，draft 借用它 */

    unsigned color;              /* 背景：常态 */
    unsigned hover_color;
    unsigned focus_color;
    unsigned border_color;       /* 0 表示不画边框 */
    unsigned focus_border_color;
    unsigned text_color;
    unsigned placeholder_color;
    unsigned caret_color;
    unsigned clear_color;
    unsigned icon_color;         /* 只对控件自己生成的图标生效 */

    float padding;
    float corner_radius;
    float icon_size;
    float icon_gap;
    float clear_size;
    float clear_gap;
    float caret_width;
    float caret_inset;           /* 光标在字体行高之外增加的高度 */
    float line_height;           /* 字体行高（逻辑像素），光标高度用 */
    unsigned long animation_time;   /* 背景/边框渐变时长，0=立即切换 */
    unsigned long caret_blink_time; /* 光标一个亮灭周期的时长，0=常亮 */

    unsigned shown_color;        /* 当前显示色（渐变中间值） */
    unsigned shown_border;
    unsigned anim_from_color;
    unsigned anim_from_border;
    unsigned anim_to_color;
    unsigned anim_to_border;
    unsigned long anim_start;
    unsigned long caret_time;    /* 最近一次编辑时间，用于闪烁复位 */

    size_t last_length;          /* 上一帧的文本状态，用于检测变化 */
    size_t last_cursor;
    char *last_text;

    unsigned controllable:1;     /* 0 时忽略鼠标点击，键盘仍可用 */
    unsigned show_clear:1;
    unsigned clear_visible:1;    /* 有内容且聚焦/悬停时才显示 */
    unsigned clear_hover:1;
    unsigned clear_press:1;
    unsigned last_left:1;
    unsigned last_focused:1;
    unsigned caret_visible:1;
    unsigned icon_owned:1;       /* 图标纹理由控件创建，绘制时按 icon_color 染色 */

    input_change_func change_callback;
    void *change_data;
    input_callback_func submit_callback;
    void *submit_data;
}widget_input_t;

/* 不读取主题的输入框：尺寸、圆角、字体、颜色和动画时长全部由调用方传入。
 * text 可为 NULL；r<0 表示取高度的一半。
 * 左侧图标和清除按钮用 widget_input_set_icon()/widget_input_set_clear() 配置。 */
widget_input_t *widget_create_raw_input(void *parent,float w,float h,float r,align_t align,
    TTF_Font *font,const char *text,
    unsigned color,unsigned hover_color,unsigned focus_color,
    unsigned border_color,unsigned focus_border_color,
    unsigned text_color,unsigned placeholder_color,unsigned caret_color,unsigned clear_color,
    unsigned long animation_time,unsigned long caret_blink_time,
    input_change_func change_callback,void *change_data,
    input_callback_func submit_callback,void *submit_data);

/* 读取主题的高度/圆角/字体/配色和动画时长，再调用 raw 版本。
 * width<=0 时使用父布局宽度；placeholder 可以为 NULL。 */
widget_input_t *widget_create_input(void *parent,float width,align_t align,
    const char *placeholder,
    input_change_func change_callback,void *change_data,
    input_callback_func submit_callback,void *submit_data);

/* 搜索框：主题输入框 + 程序化生成的放大镜图标 + 清除按钮。 */
widget_input_t *widget_create_search_input(void *parent,float width,align_t align,
    const char *placeholder,
    input_change_func change_callback,void *change_data,
    input_callback_func submit_callback,void *submit_data);

/* 文本相关：set_text 把光标移到末尾，不触发 submit，但会触发 change_callback
 * （下一帧检测到变化）。max_bytes=0 表示不限制字节数。 */
void widget_input_set_text(widget_input_t *input,const char *text);
const char *widget_input_get_text(const widget_input_t *input);
void widget_input_set_placeholder(widget_input_t *input,const char *placeholder);
void widget_input_set_max_bytes(widget_input_t *input,size_t max_bytes);
/* 左侧图标借用调用方的纹理，控件不释放；为 NULL 时移除图标。 */
void widget_input_set_icon(widget_input_t *input,SDL_Texture *icon);
/* 启用/关闭清除按钮；启用时立即生成 × 纹理。 */
void widget_input_set_clear(widget_input_t *input,int enable);

void widget_input_focus(widget_input_t *input);
void widget_input_blur(widget_input_t *input);
int widget_input_is_focused(const widget_input_t *input);
void widget_input_set_caret_blink(widget_input_t *input,unsigned long blink_time);



#endif
