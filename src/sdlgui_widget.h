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
 * 否则保持它原来的父布局和位置。tabs 的内容区从 selector 下边缘开始。
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
