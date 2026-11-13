#include <SDL3/SDL.h>
#include <stdio.h>
#include "pacman_gui.h"
#include "sdlgui_widget.h"

#define PACKAGE_CARD_HEIGHT 104.0f
#define PACKAGE_MARGIN 16.0f
#define PACKAGE_BUTTON_GAP 8.0f
#define PACKAGE_PRIMARY_WIDTH 148.0f
#define PACKAGE_DELETE_WIDTH PACKAGE_PRIMARY_WIDTH

typedef struct {
    layout_rect_t rect;
    package_t *package;

    layout_label_t *name;
    layout_label_t *version;
    layout_label_t *people;
    layout_label_t *license;

    widget_button_t *primary;
    widget_button_t *remove;
}layout_package_t;

static void package_set_box(layout_t *l,float x,float y,float w,float h){
    if(l==NULL){
        return;
    }
    l->x=x;
    l->y=y;
    l->width=w;
    l->height=h;
}

static void package_draw(void *p){
    layout_package_t *card=p;
    if(card==NULL){
        return;
    }

    rect_draw(&card->rect);

    SDL_Renderer *renderer=get_renderer();
    if(renderer==NULL){
        return;
    }

    unsigned color=get_divider_color();
    SDL_SetRenderDrawColor(renderer,
        (Uint8)((color>>24)&0xFF),
        (Uint8)((color>>16)&0xFF),
        (Uint8)((color>>8)&0xFF),
        (Uint8)(color&0xFF));
    float y=card->rect.l.y+card->rect.l.height-1.0f;
    SDL_RenderLine(renderer,
        card->rect.l.x+PACKAGE_MARGIN,y,
        card->rect.l.x+card->rect.l.width-PACKAGE_MARGIN,y);
}

static void package_update(void *p){
    layout_package_t *card=p;
    if(card==NULL){
        return;
    }

    layout_update(&card->rect.l);
    float width=card->rect.l.width;
    float height=card->rect.l.height;
    if(width<=0.0f||height<=0.0f){
        return;
    }

    package_t *package=card->package;
    int installed=package!=NULL&&package->stat!=PKG_STAT_NULL;
    float info_width=width-PACKAGE_MARGIN*2.0f-PACKAGE_PRIMARY_WIDTH-PACKAGE_MARGIN;
    if(info_width<40.0f){
        info_width=40.0f;
    }

    float left=card->rect.l.x+PACKAGE_MARGIN;
    float top=card->rect.l.y;
    package_set_box(&card->name->l,left,top+8.0f,info_width,22.0f);
    package_set_box(&card->version->l,left,top+32.0f,info_width,20.0f);
    if(card->people!=NULL){
        package_set_box(&card->people->l,left,top+54.0f,info_width,20.0f);
        package_set_box(&card->license->l,left,top+76.0f,info_width,20.0f);
    }else{
        package_set_box(&card->license->l,left,top+60.0f,info_width,20.0f);
    }

    float button_height=get_theme()->button_height;
    float primary_x=card->rect.l.x+width-PACKAGE_MARGIN-PACKAGE_PRIMARY_WIDTH;
    if(card->primary!=NULL){
        float button_y=card->rect.l.y+(height-button_height)/2.0f;
        if(installed){
            float total=button_height*2.0f+PACKAGE_BUTTON_GAP;
            button_y=card->rect.l.y+(height-total)/2.0f;
        }
        package_set_box(&card->primary->l,primary_x,button_y,
            PACKAGE_PRIMARY_WIDTH,button_height);
    }

    if(card->remove!=NULL){
        float remove_x=card->rect.l.x+width-PACKAGE_MARGIN-PACKAGE_DELETE_WIDTH;
        float remove_y=card->rect.l.y+(height-button_height)/2.0f;
        if(installed){
            float total=button_height*2.0f+PACKAGE_BUTTON_GAP;
            remove_y=card->rect.l.y+(height-total)/2.0f+button_height+PACKAGE_BUTTON_GAP;
        }
        package_set_box(&card->remove->l,remove_x,remove_y,
            PACKAGE_DELETE_WIDTH,button_height);
    }
}

static layout_label_t *package_label(layout_t *card,TTF_Font *font,
    const char *text,unsigned color){
    layout_label_t *label=create_layout_label(
        card,
        120.0f,
        18.0f,
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        font,
        text,
        color
    );
    if(label!=NULL){
        label->l.align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    }
    return label;
}

layout_rect_t *create_layout_package(package_t *package){
    if(package==NULL){
        return NULL;
    }

    layout_package_t *card=create_layout(
        sizeof(*card),
        NULL,
        0.0f,
        PACKAGE_CARD_HEIGHT,
        package_draw,
        package_update,
        NULL,
        NULL,
        make_align(0,0,1,0,0.0f,0.0f,1.0f,0.0f)
    );
    if(card==NULL){
        return NULL;
    }

    card->package=package;
    card->rect.color=get_color(COLOR_MAIN);

    TTF_Font *subtitle=obtain_font(get_font(TEXT_SUBTITLE),get_text_size(TEXT_SUBTITLE));
    TTF_Font *context=obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT));
    if(subtitle==NULL||context==NULL){
        delete_layout((layout_t *)card);
        return NULL;
    }

    const char *name=package->name!=NULL?package->name:"-";
    const char *version=package->version!=NULL?package->version:"-";
    const char *submitter=package->submitter;
    const char *license=package->license!=NULL?package->license:"-";
    char maintainer[256];
    maintainer[0]='\0';
    size_t maintainer_used=0;
    if(package->maintainers!=NULL){
        for(size_t i=0;package->maintainers[i]!=NULL;i++){
            const char *item=package->maintainers[i];
            if(item==NULL||item[0]=='\0'){
                continue;
            }
            int written=snprintf(maintainer+maintainer_used,
                sizeof(maintainer)-maintainer_used,
                "%s%s",maintainer_used>0?", ":"",item);
            if(written<0||(size_t)written>=sizeof(maintainer)-maintainer_used){
                break;
            }
            maintainer_used+=(size_t)written;
        }
    }
    card->name=package_label((layout_t *)card,subtitle,name,get_text_color(TEXT_TITLE));

    char version_text[256];
    snprintf(version_text,sizeof(version_text),"Version: %s",version);
    card->version=package_label((layout_t *)card,context,version_text,get_text_color(TEXT_CONTEXT));

    int has_maintainer=maintainer[0]!='\0';
    int has_submitter=submitter!=NULL&&submitter[0]!='\0';
    if(has_maintainer||has_submitter){
        char people_text[512];
        people_text[0]='\0';
        if(has_maintainer){
            snprintf(people_text,sizeof(people_text),"Maintainer: %s",maintainer);
        }
        if(has_submitter){
            size_t used=strlen(people_text);
            snprintf(people_text+used,sizeof(people_text)-used,
                     "%sSubmitter: %s",used>0?"    ":"",submitter);
        }
        card->people=package_label((layout_t *)card,context,people_text,
            get_text_color(TEXT_CONTEXT));
        if(card->people==NULL){
            delete_layout((layout_t *)card);
            return NULL;
        }
    }

    char license_text[256];
    snprintf(license_text,sizeof(license_text),"License: %s",license);
    card->license=package_label((layout_t *)card,context,license_text,get_text_color(TEXT_CONTEXT));

    if(card->name==NULL||card->version==NULL||card->license==NULL){
        delete_layout((layout_t *)card);
        return NULL;
    }

    const char *primary_text=package->stat==PKG_STAT_NULL
        ?"\\install install"
        :"\\update update";
    card->primary=widget_create_button(
        card,
        PACKAGE_PRIMARY_WIDTH,
        make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
        primary_text,
        NULL,
        NULL
    );
    if(card->primary!=NULL){
        card->primary->l.align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    }

    if(package->stat!=PKG_STAT_NULL){
        card->remove=widget_create_button(
            card,
            PACKAGE_DELETE_WIDTH,
            make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
            "\\delete delete",
            NULL,
            NULL
        );
        if(card->remove!=NULL){
            card->remove->l.align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
        }
    }

    package_update(card);
    return &card->rect;
}
