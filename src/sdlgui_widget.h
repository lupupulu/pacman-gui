#ifndef SDLGUI_WIDGET_H
#define SDLGUI_WIDGET_H

#include "sdlgui.h"

typedef struct {
    layout_t l;
    layout_animation_t *animation;   /* 普通按钮的动画子布局；switch 为 NULL */
    button_callback_func callback;
    void *data;                      /* 用户回调数据，由调用方持有 */

    /* 以下字段只对开关控件有效 */
    unsigned is_switch:1;
    unsigned switch_on:1;
    unsigned long switch_timer;      /* 旋钮动画起点 */
    float switch_from;               /* 本次动画的起始 x */
    float switch_knob_x;             /* 当前旋钮相对开关的 x */
    draft_rrect_t *switch_track;
    draft_rrect_t *switch_knob;
}widget_button_t;

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,button_callback_func callback,void *data);

/* 开关控件。总长取主题 switch_width，高度取 button_height。
 * 点击切换状态，callback 的 status 是切换后的状态：0=关，1=开。
 * 返回的 widget 可以直接读 switch_on，也可以用下面两个函数读写。 */
widget_button_t *widget_create_button_switch(void *parent,align_t align,button_callback_func callback,void *data);
int widget_switch_get(const widget_button_t *widget);
/* 程序化设置状态；会启动旋钮动画，但不触发 callback。 */
void widget_switch_set(widget_button_t *widget,int on);

#endif
