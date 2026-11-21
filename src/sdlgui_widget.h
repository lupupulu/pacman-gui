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

#ifndef SDLGUI_WIDGET_H
#define SDLGUI_WIDGET_H

#include "sdlgui.h"

typedef struct {
    layout_t l;
    button_callback_func callback;
    void *data;                      /* 用户回调数据，由调用方持有 */
}widget_button_t;

/* 按钮的尺寸、圆角、字体、常态/悬停/按下颜色、文字颜色和动画时长
 * 全部由调用方传入，不读取主题。文字里的 \icon 仍通过图标注册表解析。
 * 需要主题样式时用 widget_create_button()。 */
widget_button_t *widget_create_raw_button(void *parent,float w,float h,float r,align_t align,
    TTF_Font *font,const char *text,
    unsigned color,unsigned hover_color,unsigned press_color,unsigned text_color,
    unsigned long animation_time,
    button_callback_func callback,void *data);
/* 读取主题高度/圆角/字体/配色和动画时长，然后调用 widget_create_raw_button()。 */
widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,
    button_callback_func callback,void *data);
/* 与 widget_create_button() 相同，但字体和文字颜色使用 TEXT_SUBTITLE。 */
widget_button_t *widget_create_button_subtitle(void *parent,float width,align_t align,const char *text,
    button_callback_func callback,void *data);

/* 开关状态回调：on 是切换后的状态，0=关，1=开 */
typedef void (*switch_callback_func)(int on,void *data);

typedef struct {
    layout_t l;
    int on:1;
    unsigned controllable:1;   /* 0 时忽略鼠标点击，widget_switch_set() 仍可设置 */
    unsigned off_color;
    unsigned on_color;
    float corner_radius;
    unsigned long animation_time;
    switch_callback_func callback;
    void *data;
}widget_switch_t;

/* 不读取主题的开关：轨道宽高、圆角半径、旋钮高度、关/开轨道颜色、
 * 动画时长、旋钮纹理全部由调用方传入。r<0 表示取轨道高度的一半。
 * 旋钮纹理不由开关释放。 */
widget_switch_t *widget_create_raw_switch(void *parent,float w,float track_h,float knob_h,
    float r,align_t align,unsigned off_color,unsigned on_color,
    unsigned long animation_time,SDL_Texture *knob_icon,
    switch_callback_func callback,void *data);

/* 开关总长取主题 switch_width，高度取 button_height，旋钮高度取
 * switch_icon_height，颜色/动画/旋钮纹理取主题值，然后调用 raw 版本。 */
widget_switch_t *widget_create_switch(void *parent,align_t align,switch_callback_func callback,void *data);

/* 程序化设置状态；会启动旋钮动画，但不触发 callback。 */
void widget_switch_set(widget_switch_t *sw,int on);


typedef void (*selector_callback_func)(size_t now,size_t from,void *data);
typedef struct {
    layout_list_t l;
    size_t now_button;
    unsigned controllable:1;   /* 0 时忽略鼠标点击 */
    selector_callback_func callback;
    void *data;

    /* 内部状态 */
    size_t item_count;
    float item_width;
    float corner_radius;
    float from_x;
    float to_x;
    unsigned last_left:1;

    TTF_Font *font;
    float item_height;
    unsigned text_color;
    unsigned track_color;
    unsigned selected_color;
    unsigned long animation_time;
    char **items;
}widget_selector_t;

/* 不读取主题的 selector：项目宽度/高度、圆角半径、字体、项目数组、
 * 轨道/选中/文字颜色、动画时长全部由调用方传入。r<0 表示取高度的一半。
 * items 可以为 NULL，表示 count 个空项目；数组中的 NULL 项同样按空文字处理。 */
widget_selector_t *widget_create_raw_selector(void *parent,align_t align,float item_width,
    float height,float r,TTF_Font *font,const char *const *items,size_t count,
    unsigned track_color,unsigned selected_color,unsigned text_color,
    unsigned long animation_time,
    selector_callback_func callback,void *data);

/* 可变参数部分是若干项目文字，以 NULL 结尾；每项宽度不超过 item_width。
 * 文字支持 draft_create_icon_with_text() 的 \icon 标记。
 * 点击切换后 callback(now,from,data)。 */
widget_selector_t *widget_create_selector(void *parent,align_t align,float item_width,
    selector_callback_func callback,void *data,...);
/* 大号选择器：文字使用 TEXT_TITLE，高度取标题行高（至少 button_height）。 */
widget_selector_t *widget_create_selector_title(void *parent,align_t align,float item_width,
    selector_callback_func callback,void *data,...);

/* 设置/重设第 i 项文字（会重建该项文字纹理）和项目宽度。 */
void set_selector_item(widget_selector_t *selector,size_t i,const char *text);
void set_selector_item_width(widget_selector_t *selector,float width);

typedef void (*widget_size_func)(float *w,float *h);
typedef struct {
    layout_t l;
    widget_size_func get_size;

    widget_selector_t *selector;

    /* selector 原来的 callback/data；tabs 会覆盖成内部转发函数 */
    selector_callback_func selector_callback;
    void *selector_data;

    layout_t **pages;
    size_t page_count;
    size_t now_page;

    unsigned selector_attached:1;
}widget_tabs_t;

/* get_size 在 update 时返回 tabs 的总宽高；NULL 或返回 0 时沿用布局尺寸。
 * selector 必须由调用方创建；只有 selector->l.parent == NULL 时才把它挂到 tabs 下，
 * 否则保持它原来的父布局和位置。页面与 tabs 等大，selector 只是浮在页面
 * 上方（会被标记为 draw_last）；需要避免遮挡时由页面/列表自己预留偏移量。
 * tabs 会保存 selector 原来的 callback/data，并覆盖为内部转发函数：
 * 先切换页面，再调用原 callback。删除 tabs 时，外部 selector 的 callback 会恢复。
 * widget_set_tabs 把页面挂到 tabs 内，只有当前页 enable/active=1；
 * 页面布局由 tabs 接管，删除 tabs 时会一并删除。 */
widget_tabs_t *widget_create_tabs(void *parent,align_t align,widget_size_func get_size,widget_selector_t *selector);

/* l 是 layout_t* 数组，n 是数组长度；传 NULL/0 可清空页面。 */
void widget_set_tabs(widget_tabs_t *tabs,void *l,size_t n);
/* 单独设置/替换第 index 页；l 为 NULL 时移除该页。 */
void widget_set_tab(widget_tabs_t *tabs,void *l,size_t index);
/* 程序化切换到第 index 页；有 selector 时同步其选中状态。 */
void widget_tabs_select(widget_tabs_t *tabs,size_t index);


/* offset 是内容滚动偏移（逻辑像素），不是相对增量 */
typedef void (*slider_callback_func)(float offset,void *data);
typedef struct {
    layout_t l;
    draft_rrect_t *slider;

    unsigned long lasting_time; // 多少 ms 后开始逐渐消失，0 为始终保持绘制
    unsigned long slider_animation_time;

    float all_height;    // 内容总长度（竖向为高度，横向为宽度）
    float least_height;  // 滑块最小长度
    float offset;        // 当前滚动偏移

    unsigned long active_time;
    unsigned direction:1; // 0=竖向 1=横向
    unsigned dragging:1;
    unsigned last_left:1;

    slider_callback_func callback;
    void *data;
}widget_slider_t;

/* all_height 是内容总长度，direction=0 竖向、1 横向；滑块长度取视口与内容的
 * 比例并限制在 least_height 以上。callback 收到绝对滚动偏移，
 * 可直接用于 list_set_slide()。 */
widget_slider_t *create_layout_slider(void *parent,align_t align,align_t slider_align,
    float all_height,int direction,slider_callback_func callback,void *data);

/* 同步滑块位置（例如列表滚轮滚动后调用），不触发 callback。 */
void widget_slider_set(widget_slider_t *slider,float offset);
float widget_slider_get(const widget_slider_t *slider);


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



typedef struct {
    layout_t l;
    enum{
        WIDGET_ITEM_SWITCH,
        WIDGET_ITEM_WITH_BUTTON,
        WIDGET_ITEM_SELECTOR,
        WIDGET_ITEM_BAR, // 暂未实现
        WIDGET_ITEM_BUTTON // 暂未实现
    }type;
    draft_texture_t *text;   /* 左侧文字（可含 \icon），由 item 释放 */
    void *widget;            /* 右侧控件，作为 item 的子布局 */
}widget_item_t;

/* 左侧文字 + 右侧控件的通用行布局。
 * text 的所有权会在成功后交给 item；widget 会被挂到 item 下。
 * w<=0 时由 align 决定宽度，h<=0 时使用主题 item_height。 */
widget_item_t *create_raw_item(void *parent,align_t align,float w,float h,
    draft_texture_t *text,int type,void *widget);

/* 右侧开关/按钮；按钮宽度取主题 switch_width。 */
widget_item_t *create_item_switch(void *parent,align_t align,const char *text,
    switch_callback_func callback,void *data);
widget_item_t *create_item_with_button(void *parent,align_t align,const char *text,
    const char *button,button_callback_func callback,void *data);
/* 右侧 selector；每一项宽度取主题 selector_item_width。 */
widget_item_t *create_item_selector(void *parent,align_t align,const char *text,
    selector_callback_func callback,void *data,...);


#endif
