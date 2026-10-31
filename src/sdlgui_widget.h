#ifndef SDLGUI_WIDGET_H
#define SDLGUI_WIDGET_H

#include "sdlgui.h"

typedef struct {
    layout_t l;
    void *callback_data;   /* 内部回调数据，widget 释放时统一 free */
}widget_button_t;

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,button_callback_func callback,void *data);

#endif
