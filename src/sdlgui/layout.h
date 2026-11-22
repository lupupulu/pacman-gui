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

#ifndef SDLGUI_LAYOUT_H
#define SDLGUI_LAYOUT_H

#include <stddef.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

typedef struct layout_t layout_t;

typedef struct {
    unsigned horizon:2;  // 0:不在水平上对齐 1:向左对齐 2:向右对齐 3:水平对齐中央
    unsigned vertical:2; // 0:不在竖直上对齐 1:向下对齐 2:向上对齐 3:竖直对齐中央
    unsigned width:2;    // 0:自定宽度 1:以指定倍数的父布局宽度 2:指定偏移量的父布局宽度
    unsigned height:2;   // 0:自定高度 1:以指定倍数的父布局高度 2:指定偏移量的父布局高度
    float horizon_offset;  // 水平对齐偏移量，正数向右，负数向左
    float vertical_offset; // 竖直对齐偏移量，正数向下，负数向上
    float width_offset;
    float height_offset;
}align_t;

#define make_align(hor_,ver_,w_,h_,hor_off,ver_off,w_off,h_off) \
    ((align_t){ \
        .horizon=hor_,.vertical=ver_,.width=w_,.height=h_, \
        .horizon_offset=hor_off,.vertical_offset=ver_off,.width_offset=w_off,.height_offset=h_off  \
    })
#define make_align_free() make_align(0,0,0,0,0,0,0,0)

typedef void(*update_func)(void *l);
typedef void(*draw_func)(void *l);
typedef void(*clear_func)(void *l);
typedef void(*mouse_func)(void *l);

typedef struct sublayout_t {
    struct sublayout_t *parent;   /* 自引用必须带 tag，否则 typedef 名字在这里还不可见 */
    align_t align;
    float x;
    float y;
    float width;
    float height;
}sublayout_t;
#define SUBLAYOUT_DEF    \
    sublayout_t *parent; \
    align_t align;       \
    float x;             \
    float y;             \
    float width;         \
    float height;

struct layout_t{
    SUBLAYOUT_DEF

    unsigned enable:1; // 是否启用，禁用时，不更新此布局
    unsigned active:1; // 是否活跃，活跃时更新鼠标事件
    unsigned mouse_inside:1;   // 上一帧鼠标是否在这个布局内
    unsigned mouse_tracking:1; // 鼠标离开后仍按住左键，继续跟踪到松开
    unsigned son_resizable:1; // sons是否可以被realloc
    unsigned need_free:1; // 是否需要free释放
    unsigned draw_last:1; // 是否延后到同级之后再绘制（悬浮控件覆盖内容用）

    layout_t **sons;  // 依赖此布局的子布局
    size_t son_size;

    draw_func draw;
    update_func update;
    clear_func clear;
    mouse_func mouse;
};

/* parent==NULL 时创建不挂接的孤儿布局；使用前需要 layout_attach()。
 * update/mouse 只处理该布局自身；子布局的遍历由 update_sons()/layout_mouse()
 * 统一递归完成。容器布局没有自己的鼠标行为时 mouse 传 NULL。 */
void *create_layout(size_t siz,layout_t *parent,float w,float h,draw_func draw,update_func update,clear_func clear,mouse_func mouse,align_t align);
void delete_layout(layout_t *l);

int layout_attach(layout_t *child,layout_t *parent);
void layout_detach(layout_t *child);

void update_sons(void *l);   /* 调用子布局的 update，并递归更新子布局的子树 */
void layout_update(void *l); /* 只计算一个布局相对父布局的对齐 */
void layout_mouse(void *l);  /* 调用子布局的 mouse，并递归遍历子布局的子树 */


typedef struct {
    layout_t l;
    unsigned color;
}layout_rect_t; // 测试用矩形布局
void rect_draw(void *l);
layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align,unsigned color);




typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF
}draft_t;

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF
    unsigned color;
    unsigned border_color;
}draft_default_t;

draft_default_t *draft_create_default(draw_func draw,align_t align,float w,float h,unsigned color,unsigned border_color);
void draft_draw_rect(void *l);
void draft_draw_circle_w(void *l);
void draft_draw_circle_h(void *l);

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF
    float points[3][2];   /* 以包围盒左上角为原点的局部坐标 */
    unsigned color;
    unsigned border_color;
}draft_triangle_t;

draft_triangle_t *draft_create_triangle(align_t align,float *vertex,unsigned color,unsigned border_color);
void draft_draw_triangle(void *l);

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF
    float r;  // 圆角半径；auto_radius 时按当前短边的一半重新计算
    unsigned auto_radius:1;
    unsigned color;
    unsigned border_color;
}draft_rrect_t;

draft_rrect_t *draft_create_rrect(align_t align,float w,float h,float r,unsigned color,unsigned border_color);
void draft_draw_rrect(void *l);

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF

    unsigned can_enter:1; // 允许多行显示
    unsigned can_change:1; // 显示内容可以发生变化

    TTF_Font *font;    // 行高由 TTF_GetFontHeight(font) 在绘制时读取
    char *context;    // 借用调用方的字符串，create 不复制也不负责释放

    /* SUBLAYOUT_DEF 里的 width/height 是最终渲染尺寸；
     * 下面两个只是约束，0 表示不限制。 */
    float width_limit;
    float height_limit;

    unsigned color;

    SDL_Texture *cache;   // can_change=0 时的整段缓存纹理
    float cache_width;
    float cache_height;
}draft_text_t;

/* wl/hl 是最大宽高限制，0 表示不限制；width/height 在 create 或 draw 时
 * 更新为最终渲染尺寸。can_change=0 时 create 会预先测量并建立缓存。 */
draft_text_t *draft_create_text(align_t align,TTF_Font *font,char *context,float wl,float hl,unsigned color);
void draft_draw_text(void *l);
void draft_text_clear(void *l);   // 释放 can_change=0 的缓存纹理

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF

    SDL_Texture *tex;
    unsigned owns_texture:1;   /* clear 时是否销毁 tex；用户传入的纹理由用户释放 */
}draft_texture_t;

/* 纹理 draft 的 w/h 是目标逻辑尺寸：
 * 任一 <=0 时按原图宽高比缩放；两者都 <=0 时使用原图尺寸。 */
/* 用用户已有的纹理创建 draft；draft 不接管所有权，clear 时不会销毁 tex。 */
draft_texture_t *draft_create_texture(align_t align,SDL_Texture *tex,float w,float h);
/* 图片版本自己创建纹理，clear 时自动销毁。 */
draft_texture_t *draft_create_picture_from_memory(align_t align,const char *file,size_t size,float w,float h);
draft_texture_t *draft_create_picture(align_t align,const char *path,float w,float h);
/* 图标 + 文字合成：context 里的 \name 替换为对应图标，\\ 输出一个反斜杠，
 * 末尾单独的 \ 被忽略。文字不换行，wl>0 时按逻辑宽度裁剪；
 * hl>0 时图标高度取 hl（逻辑坐标）、宽度等比缩放，hl<=0 时用字体行高。 */
draft_texture_t *draft_create_icon_with_text(align_t align,TTF_Font *font,char *context,float wl,float hl,unsigned color);
void draft_draw_texture(void *l);
void draft_texture_clear(void *l);   // 只释放由图片创建的贴图，用户传入的纹理不动

typedef void(*animate_func)(void *animation);

typedef struct layout_animation_t{
    layout_t l;

    unsigned draft_resizable:1;
    draft_t **drafts;
    size_t draft_size;

    animate_func animation;
    unsigned long timer;  // 计时器
    int lasting; // 动画还在持续
    int status;  // 状态，0为不刷新动画
    void *data;
}layout_animation_t;

/* 可变参数是若干 draft 指针，以 NULL 结尾；draft 所有权交给 animation，
 * clear 时会释放 draft 及其缓存纹理/贴图。 */
layout_animation_t *create_layout_animation(void *parent,animate_func animation,...);

void animation_draw(void *l);
void animation_clear(void *l);


typedef struct {
    layout_t l;
    draft_text_t *text;   // 内部文字 draft，由 label 释放
    char *context;        // label 自己复制的一份文字
    align_t text_align;   // 文字在 label 内的对齐
}layout_label_t;

/* 在 label 内显示一段会按 w 换行、按 h 裁剪的文字。
 * text_align 只使用 horizon/vertical 与两个 offset：
 *   horizon: 0=左 1=右 3=居中；vertical: 0=上 1=下 3=居中。
 * context 会被复制；字体和颜色沿用 draft_create_text() 的语义。 */
void label_draw(void *l);
layout_label_t *create_layout_label(void *parent,float w,float h,align_t align,
    align_t text_align,TTF_Font *font,const char *context,unsigned color);




/* 每种判定框一个 check 函数，签名与 bbox_check_func 一致：
   参数是子布局自己和鼠标的窗口坐标；子布局的 x/y 已经是窗口绝对坐标 */
typedef int(*bbox_check_func)(void*,float x,float y);

typedef struct {
    bbox_check_func check;
    SUBLAYOUT_DEF
}bbox_t; // 判定框公共前缀

typedef bbox_t bbox_default_t; // 默认判定框（矩形/圆）

bbox_default_t *bbox_create_default(bbox_check_func check,align_t align,float w,float h);
int bbox_check_rect(void *sl,float x,float y);   /* 矩形 */
/* 圆形，判定框用 bbox_default_t：宽高不等时圆要选一个直径
   align_w：高向宽看齐，直径 = width
   align_h：宽向高看齐，直径 = height
   两者圆心都取判定框中心 */
int bbox_check_circle_w(void *sublayout,float x,float y);
int bbox_check_circle_h(void *sublayout,float x,float y);
#define bbox_check_circle bbox_check_circle_h


typedef struct {
    bbox_check_func check;
    SUBLAYOUT_DEF
    float points[3][2];   /* 以包围盒左上角为原点的局部坐标 */
}bbox_triangle_t;

bbox_triangle_t *bbox_create_triangle(align_t align,float *vertex);
int bbox_check_triangle(void *sublayout,float x,float y);  /* 三角形 */


typedef struct {
    bbox_check_func check;
    SUBLAYOUT_DEF
    float r;  // 圆角半径；auto_radius 时按当前短边的一半重新计算
    unsigned auto_radius:1;
}bbox_rrect_t;

bbox_rrect_t *bbox_create_rrect(align_t align,float w,float h,float r);
int bbox_check_rrect(void *sublayout,float x,float y);     /* 圆角矩形 */


typedef enum {
    BUTTON_STATUS_OUT=0,      /* 鼠标不在按钮上 */
    BUTTON_STATUS_TRIGGER=1,  /* 触发 */
    BUTTON_STATUS_HOVER=2     /* 鼠标在按钮上 */
}button_status_t;

/* status 是 button_status_t；只在状态变化时调用 */
typedef void(*button_callback_func)(int status,void *data);
typedef struct {
    layout_t l;
    unsigned sublayout_resizable:1;
    unsigned left_last_state:1; // 鼠标左键上次状态
    unsigned hover_last:1;      // 上一帧鼠标是否在判定框内
    unsigned pressed_inside:1;  // 本次按下是否落在判定框内
    unsigned need_bounce:2;     // 是否等到鼠标回弹再触发 0:否 1:无论回弹时在哪，回弹即触发 2:只有在子布局内回弹才触发

    button_status_t status;

    bbox_t **bboxes;
    size_t bboxes_size;

    button_callback_func callback;
    void *data;
}layout_button_t;

/* 可变参数是若干 bbox 指针，以 NULL 结尾；所有权交给按钮，button_clear() 会释放。 */
layout_button_t *create_layout_button(void *parent,float w,float h,align_t align,button_callback_func callback,void *data,...);

void button_clear(void *l);
void button_mouse(void *l);



typedef struct {
    layout_t l;

    unsigned direction:1;       // 延伸方向 0:竖向 1:横向
    unsigned can_be_slided:1;   // 内容超出视口时可滚动，由 list 自动计算

    /* list 自己持有的子布局，不进入 layout_t.sons。
     * 布局与 darray_t(layout_t*) 一致，源文件中直接交给 da_* 管理。 */
    struct {
        layout_t **arr;
        size_t size;
        size_t real;
    } sons;

    size_t i;     // 渲染开始点指向的布局
    float start;  // 渲染开始点相对该布局的偏移量

    /* 内容前/后留白（逻辑像素）。滚动到 0 时第一个 item 从
     * list 前部 + front_offset 开始；滚到末尾时最后一个 item 在
     * list 后部 - back_offset 结束。用于给悬浮 selector 让位，
     * 同时让内容滚动时可以从它底下经过。 */
    float front_offset;
    float back_offset;
}layout_list_t;

/* create 时传入的子布局会从原 parent 摘下并交给 list 管理；
 * list_clear() 负责释放它们。direction=0 竖向排列，非 0 横向排列。 */
layout_list_t *create_layout_list(void *parent,float w,float h,align_t align,int direction,...);

/* 向 list 追加一个已经创建的布局；成功后布局归 list 所有。 */
int list_add(layout_list_t *l,layout_t *item);

void list_draw(void *l);
void list_update(void *l);
void list_clear(void *l);
void list_mouse(void *l);

/* 按逻辑像素滑动列表：off>0 向内容末尾（下/右）移动。
 * can_be_slided=0 时不生效；越界会夹到有效范围。 */
void list_slide(layout_list_t *l,float off);
/* 读取/设置列表的绝对滚动偏移，供滑块控件同步。 */
float list_get_slide(const layout_list_t *l);
/* 内容在滚动方向上的总长度；没有子布局时返回 0。 */
float list_get_content_height(const layout_list_t *l);
void list_set_slide(layout_list_t *l,float offset);



/* 可上下滚动的页面容器：
 * - l 是可视区域（tab 页本身）；
 * - to_draw 是“整页内容”布局，宽度等于页面宽度，高度是整页高度；
 *   子布局挂在 to_draw 下，所以 align 是相对整页计算的，不受当前滚动位置影响；
 * - now 是当前滚动偏移，front_offset/back_offset 是内容前后留白。
 * 子布局的 y 会随 to_draw.y 一起移动，绘制和鼠标命中都被 l 的视口裁剪。 */
typedef struct {
    layout_t l;

    layout_t to_draw;

    float now;
    float front_offset;
    float back_offset;

    unsigned auto_height:1;  // 1: 按子布局下边界自动计算整页高度
    unsigned can_be_slided:1;
}layout_tab_t;

/* content_height>0 时固定整页高度；<=0 时按子布局自动计算（至少为视口高度）。
 * 变参是若干 layout_t*，以 NULL 结尾；子布局归 tab 所有。 */
layout_tab_t *create_layout_tab(void *parent,float w,float h,align_t align,double content_height,...);

/* 追加一个页面子布局；它的 align 相对整页（to_draw）计算。成功后归 tab 所有。 */
int tab_add(layout_tab_t *tab,layout_t *item);

void tab_draw(void *l);
void tab_update(void *l);
void tab_clear(void *l);
void tab_mouse(void *l);

/* 按逻辑像素滚动：off>0 向内容末尾（下）移动。 */
void tab_slide(layout_tab_t *tab,float off);
float tab_get_slide(const layout_tab_t *tab);
/* 整页总长度（含 front/back 留白），可直接交给滑块。 */
float tab_get_content_height(const layout_tab_t *tab);
void tab_set_slide(layout_tab_t *tab,float offset);
/* height>0 固定整页高度；height<=0 恢复自动高度。 */
void tab_set_content_height(layout_tab_t *tab,float height);





/* 根布局：update_root 读取窗口逻辑尺寸，更新整棵布局树。 */
void resize_root(int w,int h);
void update_root(void);
void draw_root(void);
const layout_t *get_root(void);
layout_t *get_root_layout(void);
float get_root_w(void);
float get_root_h(void);
/* 释放根布局的所有子布局；sdlgui_quit() 调用。 */
void layout_quit(void);

/* 重建 draft_text 的缓存纹理；输入框等控件编辑文字后使用。 */
int draft_text_rebuild_cache(draft_text_t *d);

#endif
