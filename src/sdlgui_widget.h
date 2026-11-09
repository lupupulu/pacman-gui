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
    unsigned controllable:1;   /* 0 时忽略鼠标点击，widget_switch_set() 仍可设置 */
    switch_callback_func callback;
    void *data;
}widget_switch_t;

/* 开关总长取主题 switch_width，高度取 button_height。
 * 点击切换状态；callback 的参数是切换后的状态：0=关，1=开。 */
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
    float from_x;
    float to_x;
    unsigned last_left:1;
}widget_selector_t;

/* 可变参数部分是若干按钮文字，以 NULL 结尾；每项宽度不超过 item_width。
 * 文字支持 draft_create_icon_with_text() 的 \icon 标记。
 * 点击切换后 callback(now,from,data)，动画时长取 switch_animation_time。 */
widget_selector_t *widget_create_selector(void *parent,align_t align,float item_width,
    selector_callback_func callback,void *data,...);
/* 大号选择器：文字使用 TEXT_TITLE，高度取标题行高（至少 button_height）。 */
widget_selector_t *widget_create_selector_title(void *parent,align_t align,float item_width,
    selector_callback_func callback,void *data,...);

typedef void (*widget_size_func)(float *w,float *h);
typedef struct {
    layout_list_t l;
    widget_size_func get_size;

    widget_selector_t *selector;
    layout_t *content;
    layout_t **pages;
    size_t page_count;
    size_t now_page;
    selector_callback_func callback;
    void *data;
}widget_tabs_t;

/* get_size 在 update 时返回 tabs 的总宽高；NULL 或返回 0 时沿用布局尺寸。
 * 可变参数是 tab 文字，NULL 结尾；只传 NULL 时没有内部 selector，
 * 可用 widget_tabs_select() 由外部 selector 驱动。
 * widget_set_tabs 把页面挂到 tabs 内，只有当前页 enable/active=1；
 * 页面布局由 tabs 接管，删除 tabs 时会一并删除。 */
widget_tabs_t *widget_create_tabs(void *parent,align_t align,widget_size_func get_size,
    selector_callback_func callback,void *data,...);

/* l 是 layout_t* 数组，n 是数组长度；传 NULL/0 可清空页面。 */
void widget_set_tabs(widget_tabs_t *tabs,void *l,size_t n);
/* 单独设置/替换第 index 页；l 为 NULL 时移除该页。 */
void widget_set_tab(widget_tabs_t *tabs,void *l,size_t index);
/* 程序化切换到第 index 页；会同步内部 selector 和 callback。 */
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




#endif
