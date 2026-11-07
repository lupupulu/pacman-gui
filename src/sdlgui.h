#ifndef SDLGUI_H
#define SDLGUI_H

#include <stddef.h>
#include "sdlgui_theme.h"

#ifndef SDL_render_h_
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
#endif
#ifndef SDL_video_h_
typedef struct SDL_Window SDL_Window;
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

typedef struct layout_t layout_t;   /* get_root() 等在定义之前就要用到 */

/* theme: 主题名。在 SDLGUI_THEME_PATH 中查找 <theme>/config.toml；
 * theme 为 NULL/空串，或指定主题不存在时，回退 Default/config.toml；
 * 仍然找不到则使用内置主题。 */
void sdlgui_init(const char *title,int width,int height,const char *iconpath,const char *theme);
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

void update_root(void);   /* 更新根尺寸，计算各布局对齐 */
void draw_root(void);     /* 递归绘制布局树，裁剪到各布局范围 */
void resize_root(int w,int h);
const layout_t *get_root(void);
layout_t *get_root_layout(void);   /* 可修改的 root，用于挂接顶层布局 */

float get_root_w(void);
float get_root_h(void);

/* 逻辑坐标（窗口点）到物理像素的比例：普通屏幕为 1.0，Retina 为 2.0。
 * 布局和绘制都用逻辑坐标，字体在内部按这个比例栅格化，保证高分屏清晰。 */
float sdlgui_get_scale(void);


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
    unsigned son_resizable:1; // sons是否可以被realloc
    unsigned need_free:1; // 是否需要free释放

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
layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align);




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
}layout_list_t;

/* create 时传入的子布局会从原 parent 摘下并交给 list 管理；
 * list_clear() 负责释放它们。direction=0 竖向排列，非 0 横向排列。 */
layout_list_t *create_layout_list(void *parent,float w,float h,align_t align,int direction,...);

void list_draw(void *l);
void list_update(void *l);
void list_clear(void *l);
void list_mouse(void *l);

/* 按逻辑像素滑动列表：off>0 向内容末尾（下/右）移动。
 * can_be_slided=0 时不生效；越界会夹到有效范围。 */
void list_slide(layout_list_t *l,float off);

#endif
