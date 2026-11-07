#ifndef SDLGUI_WIDGET_H
#define SDLGUI_WIDGET_H

#include "sdlgui.h"

typedef struct {
    layout_t l;
    button_callback_func callback;
    void *data;                      /* 用户回调数据，由调用方持有 */
}widget_button_t;

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,button_callback_func callback,void *data);

/* 开关状态回调：on 是切换后的状态，0=关，1=开 */
typedef void (*switch_callback_func)(int on,void *data);

typedef struct {
    layout_t l;
    int on:1;
    int controllable:1;
    switch_callback_func callback;
    void *data;
}widget_switch_t;

/* 开关总长取主题 switch_width，高度取 button_height。
 * 点击切换状态；callback 的参数是切换后的状态：0=关，1=开。 */
widget_switch_t *widget_create_switch(void *parent,align_t align,switch_callback_func callback,void *data);

int widget_switch_get(const widget_switch_t *sw);

/* 程序化设置状态；会启动旋钮动画，但不触发 callback。 */
void widget_switch_set(widget_switch_t *sw,int on);



#endif
