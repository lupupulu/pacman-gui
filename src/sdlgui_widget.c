#include <SDL3/SDL.h>
#include "sdlgui_widget.h"
#include <stdlib.h>


/* timer 是已过去的时间，whole 是总时长；返回 origin -> target 的插值颜色 */
static unsigned color_gradient(unsigned target,unsigned origin,size_t timer,size_t whole){
    if(whole==0||timer>=whole){
        return target;
    }
    if(timer==0){
        return origin;
    }

    float t=(float)timer/(float)whole;
    unsigned tr=(target>>24)&0xFF, tg=(target>>16)&0xFF, tb=(target>>8)&0xFF, ta=target&0xFF;
    unsigned or=(origin>>24)&0xFF, og=(origin>>16)&0xFF, ob=(origin>>8)&0xFF, oa=origin&0xFF;

    unsigned r=(unsigned)((float)or+((float)tr-(float)or)*t+0.5f);
    unsigned g=(unsigned)((float)og+((float)tg-(float)og)*t+0.5f);
    unsigned b=(unsigned)((float)ob+((float)tb-(float)ob)*t+0.5f);
    unsigned a=(unsigned)((float)oa+((float)ta-(float)oa)*t+0.5f);
    return (r<<24)|(g<<16)|(b<<8)|a;
}

static void button_animation(void *a){
    layout_animation_t *l=a;
    if(l==NULL||l->drafts==NULL||l->draft_size==0){
        return;
    }

    draft_rrect_t *draft=(draft_rrect_t *)l->drafts[0];
    if(draft==NULL){
        return;
    }

    unsigned target=get_color(COLOR_BUTTON);
    if(l->status==BUTTON_STATUS_HOVER){
        target=get_color(COLOR_BUTTON_HIGHLIGHT);
    }else if(l->status==BUTTON_STATUS_TRIGGER){
        target=get_color(COLOR_BUTTON_CONFIRM);
        l->lasting=1;
    }

    if(l->lasting){
        unsigned long now=SDL_GetTicks();
        unsigned color=color_gradient(target,get_color(COLOR_BUTTON),now-l->timer,get_theme()->button_animation_time);
        draft->color=color;
        draft->border_color=color;
        if(now-l->timer>get_theme()->button_animation_time){
            l->lasting=0;
        }
    }else{
        draft->color=target;
        draft->border_color=target;
    }
}

static void button_callback(int status,void *data){
    widget_button_t *widget=data;
    if(widget==NULL||widget->animation==NULL){
        return;
    }

    layout_animation_t *animation=widget->animation;
    animation->status=status;
    if(animation->status!=BUTTON_STATUS_TRIGGER||SDL_GetTicks()-animation->timer>get_theme()->button_animation_time){
        animation->timer=SDL_GetTicks();
    }
    if(animation->animation!=NULL){
        animation->animation(animation);
    }

    if(status==BUTTON_STATUS_TRIGGER&&widget->callback!=NULL){
        widget->callback(status,widget->data);
    }
}

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,button_callback_func callback,void *data){
    float height=get_theme()->button_height;
    if(get_theme()->item_height>0.0f&&height>get_theme()->item_height){
        height=get_theme()->item_height;
    }
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    widget_button_t *widget=create_layout(
        sizeof(*widget),
        parent,
        width,
        height,
        NULL,
        layout_update,
        NULL,
        NULL,
        align
    );
    if(widget==NULL){
        return NULL;
    }

    widget->callback=callback;
    widget->data=data;

    align_t full=make_align(3,3,1,1,0,0,1.0f,1.0f);

    layout_animation_t *animation=create_layout_animation(widget,button_animation,
        draft_create_rrect(full,width,height,get_theme()->rect_round_corner,get_color(COLOR_BUTTON),get_color(COLOR_BUTTON)),
        draft_create_text(
            make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
            obtain_font(get_font(TEXT_CONTEXT),
            get_text_size(TEXT_CONTEXT)),
            (char *)text,
            width,
            height,
            get_text_color(TEXT_CONTEXT)
        ),
        NULL
    );
    widget->animation=animation;

    bbox_rrect_t *bbox=bbox_create_rrect(full,width,height,get_theme()->rect_round_corner);
    layout_button_t *button=create_layout_button(widget,width,height,full,button_callback,widget,bbox,NULL);
    (void)button;

    return widget;
}


/* ---- 开关控件 ---- */

/* 旧主题的 color[] 可能还没有开关颜色，按原来的角色回退 */
static unsigned switch_theme_color(theme_color_t index,theme_color_t fallback){
    const theme_slice_t *slice=get_theme_slice();
    if(slice->color_size>(size_t)index){
        return get_color(index);
    }
    return get_color(fallback);
}

static unsigned switch_knob_color(void){
    const theme_slice_t *slice=get_theme_slice();
    if(slice->color_size>(size_t)COLOR_SWITCH_KNOB){
        return get_color(COLOR_SWITCH_KNOB);
    }
    return get_text_color(TEXT_CONTEXT);
}

static void switch_update(void *p){
    widget_button_t *widget=p;
    if(widget==NULL||!widget->is_switch){
        return;
    }

    layout_update(&widget->l);
    if(widget->switch_track==NULL||widget->switch_knob==NULL){
        return;
    }

    float width=widget->l.width;
    float height=widget->l.height;
    if(width<=0.0f||height<=0.0f){
        return;
    }

    /* 旋钮边长由主题 switch_icon_height 决定；0 或过大时取开关高度，
     * 这样默认情况下图标正好与开关上下外沿对齐。 */
    float knob=get_theme()->switch_icon_height;
    if(knob<=0.0f||knob>height){
        knob=height;
    }
    if(knob>width){
        knob=width;
    }
    float pad=(height-knob)/2.0f;

    float off_x=pad;
    float on_x=width-pad-knob;
    if(on_x<off_x){
        on_x=off_x;
    }

    unsigned long duration=get_theme()->switch_animation_time;
    float t=1.0f;
    if(duration>0){
        unsigned long elapsed=SDL_GetTicks()-widget->switch_timer;
        t=(float)elapsed/(float)duration;
        if(t>1.0f){
            t=1.0f;
        }
    }
    float eased=t*t*(3.0f-2.0f*t);

    float target=widget->switch_on?on_x:off_x;
    widget->switch_knob_x=widget->switch_from+(target-widget->switch_from)*eased;

    unsigned off_color=switch_theme_color(COLOR_SWITCH_OFF,COLOR_SECONDARY);
    unsigned on_color=switch_theme_color(COLOR_SWITCH_ON,COLOR_BUTTON);
    unsigned knob_color=switch_knob_color();

    unsigned track_color=widget->switch_on?on_color:off_color;

    widget->switch_track->x=widget->l.x;
    widget->switch_track->y=widget->l.y;
    widget->switch_track->width=width;
    widget->switch_track->height=height;
    widget->switch_track->r=height/2.0f;
    widget->switch_track->auto_radius=0;
    widget->switch_track->color=track_color;
    widget->switch_track->border_color=0;

    widget->switch_knob->x=widget->l.x+widget->switch_knob_x;
    widget->switch_knob->y=widget->l.y+pad;
    widget->switch_knob->width=knob;
    widget->switch_knob->height=knob;
    widget->switch_knob->r=knob/2.0f;
    widget->switch_knob->auto_radius=0;
    widget->switch_knob->color=knob_color;
    widget->switch_knob->border_color=0;
}

static void switch_draw(void *p){
    widget_button_t *widget=p;
    if(widget==NULL||!widget->is_switch||widget->switch_track==NULL||widget->switch_knob==NULL){
        return;
    }
    if(get_renderer()==NULL){
        return;
    }
    draft_draw_rrect(widget->switch_track);

    SDL_Texture *knob_icon=get_icon(ICON_SWITCH);
    if(knob_icon!=NULL){
        unsigned color=switch_knob_color();
        SDL_SetTextureColorMod(knob_icon,
                               (Uint8)((color>>24)&0xFF),
                               (Uint8)((color>>16)&0xFF),
                               (Uint8)((color>>8)&0xFF));
        SDL_SetTextureAlphaMod(knob_icon,(Uint8)(color&0xFF));
        SDL_SetTextureScaleMode(knob_icon,SDL_SCALEMODE_LINEAR);
        SDL_FRect dst={
            .x=widget->switch_knob->x,
            .y=widget->switch_knob->y,
            .w=widget->switch_knob->width,
            .h=widget->switch_knob->height
        };
        SDL_RenderTexture(get_renderer(),knob_icon,NULL,&dst);
    }else{
        draft_draw_rrect(widget->switch_knob);
    }
}

static void switch_clear(void *p){
    widget_button_t *widget=p;
    if(widget==NULL){
        return;
    }
    free(widget->switch_track);
    widget->switch_track=NULL;
    free(widget->switch_knob);
    widget->switch_knob=NULL;
}

static void switch_callback(int status,void *data){
    widget_button_t *widget=data;
    if(widget==NULL||!widget->is_switch){
        return;
    }

    if(status==BUTTON_STATUS_TRIGGER){
        widget->switch_from=widget->switch_knob_x;
        widget->switch_timer=SDL_GetTicks();
        widget->switch_on=widget->switch_on?0u:1u;
        switch_update(widget);

        if(widget->callback!=NULL){
            widget->callback((int)widget->switch_on,widget->data);
        }
    }
}

widget_button_t *widget_create_button_switch(void *parent,align_t align,button_callback_func callback,void *data){
    float width=get_theme()->switch_width;
    float height=get_theme()->button_height;
    if(get_theme()->item_height>0.0f&&height>get_theme()->item_height){
        height=get_theme()->item_height;
    }
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    widget_button_t *widget=create_layout(
        sizeof(*widget),
        parent,
        width,
        height,
        switch_draw,
        switch_update,
        switch_clear,
        NULL,
        align
    );
    if(widget==NULL){
        return NULL;
    }

    widget->is_switch=1;
    widget->switch_on=0;
    widget->switch_timer=0;
    widget->switch_from=0.0f;
    widget->switch_knob_x=0.0f;
    widget->callback=callback;
    widget->data=data;
    widget->animation=NULL;

    align_t draft_align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    widget->switch_track=draft_create_rrect(draft_align,width,height,height/2.0f,
        switch_theme_color(COLOR_SWITCH_OFF,COLOR_SECONDARY),0);
    widget->switch_knob=draft_create_rrect(draft_align,height,height,height/2.0f,
        switch_knob_color(),0);
    if(widget->switch_track==NULL||widget->switch_knob==NULL){
        delete_layout((layout_t *)widget);
        return NULL;
    }

    align_t full=make_align(3,3,1,1,0,0,1.0f,1.0f);
    bbox_rrect_t *bbox=bbox_create_rrect(full,width,height,height/2.0f);
    if(bbox==NULL){
        delete_layout((layout_t *)widget);
        return NULL;
    }

    layout_button_t *button=create_layout_button(widget,width,height,full,switch_callback,widget,bbox,NULL);
    (void)button;

    switch_update(widget);
    return widget;
}

int widget_switch_get(const widget_button_t *widget){
    if(widget==NULL||!widget->is_switch){
        return 0;
    }
    return widget->switch_on?1:0;
}

void widget_switch_set(widget_button_t *widget,int on){
    if(widget==NULL||!widget->is_switch){
        return;
    }
    unsigned state=on?1u:0u;
    if(widget->switch_on==state){
        return;
    }

    widget->switch_from=widget->switch_knob_x;
    widget->switch_timer=SDL_GetTicks();
    widget->switch_on=state;
    switch_update(widget);
}
