#include <SDL3/SDL.h>
#include "sdlgui.h"
#include "sdlgui_i18n.h"
#include <stdlib.h>

#define WINDOW_TITLE "pacman-gui"
#define WINDOW_W 640
#define WINDOW_H 480

#define BUTTON_ANIMATION_MS 600.0f

#define TRUE 1
#define FALSE 0

void update_mouse(const SDL_Event *ev){
    if(ev->type!=SDL_EVENT_MOUSE_MOTION){
        return;
    }
    if(get_window()==NULL||ev->motion.windowID!=SDL_GetWindowID(get_window())){
        return;   /* 不是本窗口的移动，不更新 */
    }
    set_mouse_pos(ev->motion.x,ev->motion.y);
}

/* 按下/回弹时把五个按键状态交给 gui，坐标各用各的事件坐标 */
void update_mouse_button(const SDL_Event *ev){
    if(ev->type!=SDL_EVENT_MOUSE_BUTTON_DOWN&&ev->type!=SDL_EVENT_MOUSE_BUTTON_UP){
        return;
    }
    if(get_window()==NULL||ev->button.windowID!=SDL_GetWindowID(get_window())){
        return;   /* 不是本窗口的按键 */
    }

    /* 按下就用按下时的坐标，回弹就用回弹时的坐标 */
    set_mouse_pos(ev->button.x,ev->button.y);

    /* 事件本身只带"哪个键变了"，整张掩码要现查一次（同一批 pump 的状态，是准的） */
    SDL_MouseButtonFlags m=SDL_GetMouseState(NULL,NULL);
    set_mouse_button((m&SDL_BUTTON_LMASK)!=0,
                     (m&SDL_BUTTON_MMASK)!=0,
                     (m&SDL_BUTTON_RMASK)!=0,
                     (m&SDL_BUTTON_X1MASK)!=0,
                     (m&SDL_BUTTON_X2MASK)!=0);
}

static unsigned blend_color(unsigned from,unsigned to,float t){
    if(t<0.0f){ t=0.0f; }
    if(t>1.0f){ t=1.0f; }

    unsigned fr=(from>>24)&0xFF, fg=(from>>16)&0xFF, fb=(from>>8)&0xFF, fa=from&0xFF;
    unsigned tr=(to>>24)&0xFF, tg=(to>>16)&0xFF, tb=(to>>8)&0xFF, ta=to&0xFF;

    unsigned r=(unsigned)((float)fr+((float)tr-(float)fr)*t+0.5f);
    unsigned g=(unsigned)((float)fg+((float)tg-(float)fg)*t+0.5f);
    unsigned b=(unsigned)((float)fb+((float)tb-(float)fb)*t+0.5f);
    unsigned a=(unsigned)((float)fa+((float)ta-(float)fa)*t+0.5f);
    return (r<<24)|(g<<16)|(b<<8)|a;
}

static void button_animation(void *p){
    layout_animation_t *anim=p;
    if(anim==NULL||anim->status==0||anim->drafts==NULL||anim->draft_size==0){
        return;
    }

    draft_rrect_t *rect=(draft_rrect_t *)anim->drafts[0];
    if(rect==NULL){
        anim->status=0;
        return;
    }

    Uint64 now=SDL_GetTicks();
    Uint64 start=(Uint64)anim->timer;
    float t=now>=start?(float)(now-start)/BUTTON_ANIMATION_MS:1.0f;
    if(t>=1.0f){
        t=1.0f;
        anim->status=0;
    }

    /* 0 -> 1 -> 0 的脉冲：颜色和圆角一起动 */
    float pulse=t<0.5f?t*2.0f:(1.0f-t)*2.0f;
    unsigned base=get_color(COLOR_HIGHLIGHT);
    unsigned accent=0xFF8844FFu;

    rect->color=blend_color(base,accent,pulse);
    rect->border_color=blend_color(0xFFFFFFFFu,accent,pulse);
    rect->r=10.0f+8.0f*pulse;
}

static void button_clicked(void *data){
    layout_animation_t *anim=data;
    static int clicks=0;
    clicks++;

    if(anim!=NULL){
        anim->timer=(size_t)SDL_GetTicks();
        anim->status=1;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,_("Button clicked: %d"),clicks);
}
static void update_theme_colors(layout_rect_t *top_bar,draft_rrect_t *button,draft_text_t *label){
    if(top_bar!=NULL){
        top_bar->color=get_color(COLOR_SECONDARY);
    }
    if(button!=NULL){
        button->color=get_color(COLOR_HIGHLIGHT);
    }
    if(label!=NULL){
        label->color=get_text_color(0);
        draft_text_clear(label);   /* 文字颜色已烘焙进缓存，清掉后按新颜色重建 */
    }
}

static void apply_system_theme(layout_rect_t *top_bar,draft_rrect_t *button,draft_text_t *label){
    theme_select_system();
    update_theme_colors(top_bar,button,label);
}

int main(int argc,char *argv[]){
    (void)argc;
    (void)argv;

    sdlgui_init("pacman-gui",WINDOW_W,WINDOW_H,"assets/icon-64.png","Default");

    /* 顶部测试条：颜色跟随当前主题 */
    layout_rect_t *top_bar=create_layout_rect(NULL,0,45,make_align(0,0,1,0,0,0,1.0f,0));
    if(top_bar!=NULL){
        top_bar->color=get_color(COLOR_SECONDARY);
        top_bar->l.enable=1;
    }

    /* 判定框先创建，parent 由 create_layout_button() 挂接时指定 */
    bbox_default_t *hitbox=bbox_create_default(
        bbox_check_rect,make_align(3,3,1,1,0.0f,0.0f,1.0f,1.0f),200.0f,60.0f);

    draft_rrect_t *button_draft=NULL;
    draft_text_t *button_label=NULL;

    /* 居中按钮：按钮本身只负责鼠标，显示用它的子矩形 */
    layout_button_t *button=create_layout_button(
        NULL,200.0f,60.0f,
        make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
        NULL,NULL,hitbox,NULL);
    if(button==NULL){
        free(hitbox);
    }
    if(button!=NULL){
        /* 用 animation 当按钮外观：一个圆角 draft + 点击脉冲动画 */
        unsigned base=get_color(COLOR_HIGHLIGHT);
        button_draft=draft_create_rrect(
            make_align(3,3,1,1,0.0f,0.0f,1.0f,1.0f),
            200.0f,60.0f,10.0f,base,0xFFFFFFFFu);

        /* 按钮文字：create 时测量最终宽高并建缓存，按中心对齐放进动画 */
        TTF_Font *font=get_font(0);
        const char *label_text=_("Click me");
        if(font!=NULL){
            button_label=draft_create_text(
                make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
                font,(char *)label_text,0.0f,0.0f,get_text_color(0));
            if(button_label!=NULL){
                button_label->can_enter=0;
            }
        }

        layout_animation_t *button_anim=NULL;
        if(button_draft!=NULL){
            button_anim=create_layout_animation(
                (layout_t *)button,button_animation,button_draft,button_label,NULL);
            if(button_anim==NULL){
                free(button_draft);
                free(button_label);
                button_draft=NULL;
                button_label=NULL;
            }
        }else{
            free(button_label);
            button_label=NULL;
        }

        button->callback=button_clicked;
        button->data=button_anim;
    }

    /* 先更新一次布局，鼠标判定才能拿到按钮位置 */
    update_root();

    int running=TRUE;

    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_EVENT_QUIT){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_KEY_DOWN&&ev.key.key==SDLK_ESCAPE){
                running=FALSE;
            }else if(ev.type==SDL_EVENT_WINDOW_RESIZED
                  || ev.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED){
                update_root();
            }else if(ev.type==SDL_EVENT_SYSTEM_THEME_CHANGED){
                apply_system_theme(top_bar,button_draft,button_label);
            }else if(ev.type==SDL_EVENT_MOUSE_MOTION){
                update_mouse(&ev);
            }else if(ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN
                  || ev.type==SDL_EVENT_MOUSE_BUTTON_UP){
                update_mouse_button(&ev);
            }
        }


        /* 3.2 绘制：用主题主色清屏 -> 画界面 -> 提交 */
        unsigned background=get_color(COLOR_MAIN);
        SDL_SetRenderDrawColor(get_renderer(),
            (background>>24)&0xFF,(background>>16)&0xFF,
            (background>>8)&0xFF,background&0xFF);
        SDL_RenderClear(get_renderer());

        update_root();

        SDL_RenderPresent(get_renderer());
    }

    sdlgui_quit();
    return 0;
}
