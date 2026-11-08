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
    int controllable:1;   /* 0 时忽略鼠标点击，widget_switch_set() 仍可设置 */
    switch_callback_func callback;
    void *data;
}widget_switch_t;

/* 开关总长取主题 switch_width，高度取 button_height。
 * 点击切换状态；callback 的参数是切换后的状态：0=关，1=开。 */
widget_switch_t *widget_create_switch(void *parent,align_t align,switch_callback_func callback,void *data);

/* 程序化设置状态；会启动旋钮动画，但不触发 callback。 */
void widget_switch_set(widget_switch_t *sw,int on);


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




#endif
