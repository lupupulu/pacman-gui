#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "sdlgui.h"
#include "sdlgui_theme.h"
#include "sdlgui_i18n.h"
#include "sdlgui_fontlookup.h"
#include "CDS/darray.h"
#include "CDS/confc.h"


/* theme_t 里的 color/text 是 unsigned *，内建主题需要各自准备静态后备数组。 */
static unsigned builtin_bright_color[]={
    0xEFF1F5FF,0xCCD0DAFF,0x8CAAEEFF,0xFE640BFF,0xABC4FFFF,
    0xCCD0DAFF,0x8CAAEEFF,0x4C4F69FF
};
static unsigned builtin_bright_text_color[]={0x4C4F69FF};
static unsigned builtin_dark_color[]={
    0x1E1E2EFF,0x313244FF,0x1E66F5FF,0xFAB387FF,0x89B4FAFF,
    0x313244FF,0x1E66F5FF,0xCDD6F4FF
};
static unsigned builtin_dark_text_color[]={0xCDD6F4FF};
static unsigned builtin_text_size[]={20,16,14};
static char *builtin_font_names[]={NULL};   /* NULL 表示使用内嵌字体 */

/* 默认嵌入字体：由 tools/ttf_to_c.py 生成到 build/pacman_pixel_font.c。 */
extern const unsigned char pacman_pixel_ttf[];
extern const size_t pacman_pixel_ttf_len;

static theme_slice_t builtin_theme_slice[]={
    {
        .name="Bright",
        .color=builtin_bright_color,
        .color_size=8,
        .text_color=builtin_bright_text_color,
        .text_color_size=1,
        .divider_color=0xBCC0CCFF
    },
    {
        .name="Dark",
        .color=builtin_dark_color,
        .color_size=8,
        .text_color=builtin_dark_text_color,
        .text_color_size=1,
        .divider_color=0x45475AFF
    }
};

static theme_t theme={
    .version="0.0.1",
    .name=NULL,   /* NULL：内联的内建主题；theme_set 成功后才换成加载进来的名字 */
    .now_slice=&builtin_theme_slice[0],

    .fonts=builtin_font_names,
    .font_size=1,

    .text=builtin_text_size,
    .text_size=3,

    .item_height=32.0f,
    .button_animation_time=200,
    .switch_animation_time=120,
    .button_height=32.0f,
    .switch_width=64.0f,
    .switch_icon_height=0.0f,
    .rect_round_corner=10.0f,

    .slices=builtin_theme_slice,
    .slice_size=2
};

/* 内置 lost 图标：由 tools/gen_lost_icon.py 生成到 build/lost_icon.c。
 * 64x64 RGBA，白色问号 + 透明背景，绘制时按主题文字色着色。 */
extern const unsigned char pacman_lost_icon[];
extern const size_t pacman_lost_icon_len;
extern const int pacman_lost_icon_w;
extern const int pacman_lost_icon_h;

typedef struct {
    char *name;          /* 规则名；icon_rules 按 name 升序排列 */
    unsigned long index;
}icon_pair_t;

static const icon_pair_t icon_default_rules[]={
    {.name="lost",.index=ICON_LOST},
    {.name="knob",.index=ICON_KNOB}
};

static darray_t(icon_pair_t) icon_rules;
static size_t icon_index_max=0;       /* 下一个可分配的图标下标 */
static SDL_Texture *builtin_lost_texture=NULL;

static toml_t theme_toml;


static void theme_free_loaded(void);   /* 定义在 theme_set 前面 */
static void release_font_cache(void);
static void release_icon_rules(void);
static void theme_load_icons(void);
static int theme_ensure_icon_slots(void);
static int theme_create_builtin_lost(void);

/* 复制字符串；源为 NULL 时返回 NULL */
static char *theme_dup_string(const char *s){
    if(s==NULL){
        return NULL;
    }
    size_t size=strlen(s)+1;
    char *copy=malloc(size);
    if(copy!=NULL){
        memcpy(copy,s,size);
    }
    return copy;
}

/* 拼目录和文件名；dir 末尾的 '/' 不会重复 */
static char *theme_join_path(const char *dir,const char *name){
    if(dir==NULL||dir[0]=='\0'){
        return theme_dup_string(name);
    }
    if(name==NULL||name[0]=='\0'){
        return theme_dup_string(dir);
    }

    size_t dir_size=strlen(dir);
    size_t name_size=strlen(name);
    int slash=dir[dir_size-1]!='/';
    char *path=malloc(dir_size+(size_t)slash+name_size+1);
    if(path==NULL){
        return NULL;
    }
    memcpy(path,dir,dir_size);
    size_t pos=dir_size;
    if(slash){
        path[pos++]='/';
    }
    memcpy(path+pos,name,name_size+1);
    return path;
}

/* 取路径的目录部分；没有 '/' 时返回 "." */
static char *theme_dirname(const char *path){
    if(path==NULL||path[0]=='\0'){
        return theme_dup_string(".");
    }
    const char *slash=strrchr(path,'/');
    if(slash==NULL){
        return theme_dup_string(".");
    }
    if(slash==path){
        return theme_dup_string("/");
    }
    size_t size=(size_t)(slash-path);
    char *dir=malloc(size+1);
    if(dir!=NULL){
        memcpy(dir,path,size);
        dir[size]='\0';
    }
    return dir;
}

/* 图标名补 .png 后缀；已经带 .png（大小写不敏感）就不重复添加 */
static int theme_ends_with_png(const char *name){
    size_t size=strlen(name);
    if(size<4){
        return 0;
    }
    return name[size-4]=='.' &&
           (name[size-3]=='p'||name[size-3]=='P') &&
           (name[size-2]=='n'||name[size-2]=='N') &&
           (name[size-1]=='g'||name[size-1]=='G');
}

static char *theme_icon_filename(const char *name){
    if(name==NULL||name[0]=='\0'){
        return NULL;
    }
    if(theme_ends_with_png(name)){
        return theme_dup_string(name);
    }
    size_t size=strlen(name);
    char *filename=malloc(size+sizeof(".png"));
    if(filename!=NULL){
        memcpy(filename,name,size);
        memcpy(filename+size,".png",sizeof(".png"));
    }
    return filename;
}

/* 按指定像素尺寸打开内嵌字体，失败返回 NULL */
static TTF_Font *create_embedded_font(float ptsize){
    if(!TTF_WasInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_embedded_font: TTF is not initialized"));
        return NULL;
    }
    SDL_IOStream *io=SDL_IOFromConstMem(pacman_pixel_ttf,pacman_pixel_ttf_len);
    if(io==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_embedded_font: SDL_IOFromConstMem failed: %s"),SDL_GetError());
        return NULL;
    }
    TTF_Font *font=TTF_OpenFontIO(io,true,ptsize);
    if(font==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_embedded_font: TTF_OpenFontIO(%.0f) failed: %s"),ptsize,SDL_GetError());
    }else{
        TTF_SetFontHinting(font,TTF_HINTING_MONO);
    }
    return font;
}


/* 预热默认主题要用的内嵌字体；实际加载统一走 obtain_font() */
int theme_load_default_font(void){
    float ptsize=20.0f;
    if(theme.text!=NULL&&theme.text_size>0&&theme.text[0]>0){
        ptsize=(float)theme.text[0];
    }else{
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,_("No text sizes configured, using 20 pt"));
    }

    if(obtain_font(NULL,ptsize)==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to obtain the embedded default font at %.0f pt"),ptsize);
        return 0;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,_("Loaded embedded default font at %.0f pt"),ptsize);
    return 1;
}

/* ---- 主题读取 ----
   结构见 assets/Default/config.toml：根层是全局设置，每个 [表名] 是一套配色切片。 */

static const char *allow_version[]={"0.0.1"};

/* 复位成内联的内建默认主题（name=NULL 表示没有从配置文件加载） */
static void theme_reset_builtin(void){
    theme.name=NULL;
    theme.version="0.0.1";
    theme.now_slice=&builtin_theme_slice[0];
    theme.fonts=builtin_font_names;
    theme.font_size=1;
    theme.text=builtin_text_size;
    theme.text_size=3;
    theme.item_height=32.0f;
    theme.button_animation_time=200;
    theme.switch_animation_time=120;
    theme.button_height=32.0f;
    theme.switch_width=64.0f;
    theme.switch_icon_height=0.0f;
    theme.rect_round_corner=10.0f;
    theme.slices=builtin_theme_slice;
    theme.slice_size=2;
    theme.icon_path=NULL;
    theme.icons=NULL;
    theme.icon_size=0;
}

/* 统一的主题初始化：字体 + icon_default_rules。
 * 调用前 renderer 必须已经创建；icon_rules 会先清空再按默认规则建立。 */
int theme_init(void){
    theme_free_loaded();
    theme_reset_builtin();
    release_icon_rules();
    icon_index_max=0;

    for(size_t i=0;i<sizeof(icon_default_rules)/sizeof(icon_default_rules[0]);i++){
        if(add_icon_rule(icon_default_rules[i].name,(long)icon_default_rules[i].index)<0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_init: failed to add default icon rule %s"),
                         icon_default_rules[i].name);
            return 0;
        }
    }

    if(!theme_load_default_font()){
        return 0;
    }

    if(!theme_create_builtin_lost()){
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    _("theme_init: built-in lost icon is unavailable"));
    }
    return 1;
}

/* 释放主题里 malloc 出来的数组。
 * name/version/slice.name 指向 TOML 文档里的字符串，由 toml_unload 统一释放。
 * fonts 里的字体族名是 theme_set 复制出来的，需要逐个释放。 */
static void theme_free_arrays(theme_t *th){
    if(th->slices!=NULL){
        for(size_t i=0;i<th->slice_size;i++){
            free(th->slices[i].color);
            free(th->slices[i].text_color);
        }
        free(th->slices);
        th->slices=NULL;
    }
    if(th->fonts!=NULL&&th->fonts!=builtin_font_names){
        for(size_t i=0;i<th->font_size;i++){
            free(th->fonts[i]);
        }
        free(th->fonts);
        th->fonts=NULL;
    }
    if(th->icons!=NULL){
        for(size_t i=0;i<th->icon_size;i++){
            if(th->icons[i]!=NULL){
                SDL_DestroyTexture(th->icons[i]);
            }
        }
        free(th->icons);
        th->icons=NULL;
    }
    free(th->icon_path);
    th->icon_path=NULL;
    th->icon_size=0;

    free(th->text);
    th->text=NULL;
    th->slice_size=0;
    th->font_size=0;
    th->text_size=0;
}

/* 释放 theme_set 加载出来的部分，然后回到内联的内建默认主题。
 * name==NULL 就是内联主题的标记：没有加载数据，也不需要释放。 */
static void theme_free_loaded(void){
    if(theme.name==NULL){
        return;
    }
    theme_free_arrays(&theme);
    theme_reset_builtin();
}

/* 从 TOML 数值（整数或浮点）里取一个 double */
static int theme_read_number(toml_type_t *tp,double *out){
    if(tp==NULL){
        return 0;
    }
    if(*tp==TOML_INTEGER){
        *out=(double)((toml_integer_t *)tp)->v;
        return 1;
    }
    if(*tp==TOML_FLOAT){
        *out=((toml_float_t *)tp)->v;
        return 1;
    }
    return 0;
}

/* 颜色数组：每个元素都是 0xRRGGBBAA 范围内的整数 */
static unsigned *theme_read_colors(toml_type_t *tp,const char *slice,const char *key,size_t *out_size){
    *out_size=0;
    if(tp==NULL||*tp!=TOML_ARRAY){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: theme %s key %s must be an array"),slice,key);
        return NULL;
    }
    toml_array_t *arr=(toml_array_t *)tp;
    if(arr->size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: theme %s key %s must not be empty"),slice,key);
        return NULL;
    }

    unsigned *colors=malloc(arr->size*sizeof(*colors));
    if(colors==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: failed to allocate memory for %s.%s"),slice,key);
        return NULL;
    }

    for(size_t i=0;i<arr->size;i++){
        toml_type_t *item=arr->items[i];
        if(item==NULL||*item!=TOML_INTEGER){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: %s.%s[%zu] must be an integer"),slice,key,i);
            free(colors);
            return NULL;
        }
        long v=((toml_integer_t *)item)->v;
        if(v<0||v>0xFFFFFFFFL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: %s.%s[%zu] is outside 0x00000000..0xFFFFFFFF"),slice,key,i);
            free(colors);
            return NULL;
        }
        colors[i]=(unsigned)v;
    }

    *out_size=arr->size;
    return colors;
}

/* 单个颜色：0xRRGGBBAA 范围内的整数 */
static int theme_read_color(toml_type_t *tp,const char *slice,const char *key,unsigned *out){
    if(tp==NULL||*tp!=TOML_INTEGER){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s.%s must be an integer"),slice,key);
        return 0;
    }
    long v=((toml_integer_t *)tp)->v;
    if(v<0||v>0xFFFFFFFFL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s.%s is outside 0x00000000..0xFFFFFFFF"),slice,key);
        return 0;
    }
    *out=(unsigned)v;
    return 1;
}

/* 字号数组：元素是正数，写整数或浮点都可以 */
static unsigned *theme_read_text_sizes(toml_type_t *tp,size_t *out_size){
    *out_size=0;
    if(tp==NULL||*tp!=TOML_ARRAY){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: text_size must be an array"));
        return NULL;
    }
    toml_array_t *arr=(toml_array_t *)tp;
    if(arr->size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: text_size must not be empty"));
        return NULL;
    }

    unsigned *sizes=malloc(arr->size*sizeof(*sizes));
    if(sizes==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate memory for text_size"));
        return NULL;
    }

    for(size_t i=0;i<arr->size;i++){
        double v=0.0;
        if(!theme_read_number(arr->items[i],&v)){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: text_size[%zu] must be a number"),i);
            free(sizes);
            return NULL;
        }
        if(!isfinite(v)||v<1.0||v>1000.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: text_size[%zu] must be between 1 and 1000 pt"),i);
            free(sizes);
            return NULL;
        }
        sizes[i]=(unsigned)(v+0.5);
    }

    *out_size=arr->size;
    return sizes;
}



void theme_set(const char *path,const char *file){
    char *f=confc_find_path(path,file);
    if(f==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: cannot find %s under %s"),file?file:"(null)",path?path:"(null)");
        return ;
    }

    /* 配置目录用于解析默认 icon_path（<配置目录>/icon） */
    char *config_dir=theme_dirname(f);
    toml_t t=toml_load(f);
    free(f);
    if(config_dir==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate config directory"));
        return ;
    }
    if(t.size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to parse %s"),file);
        free(config_dir);
        return ;
    }

    /* 新配置先完整读到 tmp，任何一项不合格都不碰正在使用的 theme/theme_toml。
       tmp.fonts 先保持 NULL，等 fonts 解析阶段再分配，避免失败时误释放当前主题的数组。 */
    theme_t tmp={0};
    tmp.item_height=32.0f;
    tmp.button_animation_time=200;
    tmp.switch_animation_time=120;
    tmp.button_height=32.0f;
    tmp.switch_width=64.0f;
    tmp.switch_icon_height=0.0f;
    tmp.rect_round_corner=10.0f;

    toml_type_t *tp=NULL;

    /* 1. 版本必须认识 */
    tp=toml_lookup(&t,"",0,"version");
    if(tp==NULL||*tp!=TOML_STRING){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s is missing a string version"),file);
        goto fail;
    }
    const char *version=((toml_string_t *)tp)->str;
    int supported=0;
    for(size_t i=0;i<sizeof(allow_version)/sizeof(allow_version[0]);i++){
        if(strcmp(allow_version[i],version)==0){
            supported=1;
            break;
        }
    }
    if(!supported){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: unsupported configuration version %s"),version);
        goto fail;
    }
    tmp.version=(char *)version;

    /* 2. name 必选：只有内联主题（name==NULL）才没有名字 */
    tp=toml_lookup(&t,"",0,"name");
    if(tp==NULL||*tp!=TOML_STRING){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s is missing a string name"),file);
        goto fail;
    }
    tmp.name=((toml_string_t *)tp)->str;

    /* 3. 图标目录：没写时使用 <config.toml 所在目录>/icon */
    char *icon_path=NULL;
    tp=toml_lookup(&t,"",0,"icon_path");
    if(tp!=NULL){
        if(*tp!=TOML_STRING||((toml_string_t *)tp)->str[0]=='\0'){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: icon_path must be a non-empty string"));
            goto fail;
        }
        const char *value=((toml_string_t *)tp)->str;
        if(value[0]=='/'){
            icon_path=theme_dup_string(value);
        }else{
            icon_path=theme_join_path(config_dir,value);
        }
    }else{
        icon_path=theme_join_path(config_dir,"icon");
    }
    free(config_dir);
    config_dir=NULL;
    if(icon_path==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate icon path"));
        goto fail;
    }
    tmp.icon_path=icon_path;

    /* 4. 字号：没写就沿用内建的三级字号 */
    tp=toml_lookup(&t,"",0,"text_size");
    if(tp!=NULL){
        tmp.text=theme_read_text_sizes(tp,&tmp.text_size);
        if(tmp.text==NULL){
            goto fail;
        }
    }else{
        tmp.text=malloc(sizeof(builtin_text_size));
        if(tmp.text==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate default text sizes"));
            goto fail;
        }
        memcpy(tmp.text,builtin_text_size,sizeof(builtin_text_size));
        tmp.text_size=sizeof(builtin_text_size)/sizeof(builtin_text_size[0]);
    }

    /* 5. 字体族：写了就记下族名并通过 obtain_font 获取；没写就沿用当前字体 */
    tp=toml_lookup(&t,"",0,"fonts");
    if(tp!=NULL){
        if(*tp!=TOML_ARRAY){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: fonts must be an array"));
            goto fail;
        }
        toml_array_t *arr=(toml_array_t *)tp;
        if(arr->size==0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: fonts must not be empty"));
            goto fail;
        }

        tmp.fonts=calloc(arr->size,sizeof(*tmp.fonts));
        if(tmp.fonts==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate font array"));
            goto fail;
        }
        tmp.font_size=arr->size;

        for(size_t i=0;i<arr->size;i++){
            toml_type_t *item=arr->items[i];
            if(item==NULL||*item!=TOML_STRING||((toml_string_t *)item)->str[0]=='\0'){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("theme_set: fonts[%zu] must be a non-empty string"),i);
                goto fail;
            }
            const char *family=((toml_string_t *)item)->str;
            tmp.fonts[i]=theme_dup_string(family);
            if(tmp.fonts[i]==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("theme_set: failed to copy font name %s"),family);
                goto fail;
            }
            /* 统一从 obtain_font 入口取字体：命中缓存直接复用，找不到时回退内嵌字体 */
            if(obtain_font(family,(float)tmp.text[0])==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("theme_set: failed to obtain font %s"),family);
                goto fail;
            }
        }
    }else{
        /* 没写 fonts：复制当前字体族名，避免旧主题释放后留下悬空指针 */
        tmp.font_size=theme.font_size>0?theme.font_size:1;
        tmp.fonts=calloc(tmp.font_size,sizeof(*tmp.fonts));
        if(tmp.fonts==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate font array"));
            goto fail;
        }
        for(size_t i=0;i<tmp.font_size;i++){
            const char *family=(theme.fonts!=NULL&&i<theme.font_size)?theme.fonts[i]:NULL;
            if(family!=NULL){
                tmp.fonts[i]=theme_dup_string(family);
                if(tmp.fonts[i]==NULL){
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 _("theme_set: failed to copy font name %s"),family);
                    goto fail;
                }
            }
        }
    }

    /* 6. 尺寸参数 */
    tp=toml_lookup(&t,"",0,"item_height");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: item_height must be positive"));
            goto fail;
        }
        tmp.item_height=(float)v;
    }
    tp=toml_lookup(&t,"",0,"button_height");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: button_height must be positive"));
            goto fail;
        }
        tmp.button_height=(float)v;
    }
    tp=toml_lookup(&t,"",0,"button_animation_time");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: button_animation_time must be positive"));
            goto fail;
        }
        tmp.button_animation_time=(unsigned long)v;
    }
    tp=toml_lookup(&t,"",0,"switch_animation_time");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: switch_animation_time must be positive"));
            goto fail;
        }
        tmp.switch_animation_time=(unsigned long)v;
    }
    if(tmp.button_height>tmp.item_height){
        tmp.button_height=tmp.item_height;
    }
    tp=toml_lookup(&t,"",0,"switch_width");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: switch_width must be positive"));
            goto fail;
        }
        tmp.switch_width=(float)v;
    }
    tp=toml_lookup(&t,"",0,"switch_icon_height");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: switch_icon_height must be non-negative"));
            goto fail;
        }
        tmp.switch_icon_height=(float)v;
    }
    tp=toml_lookup(&t,"",0,"rect_round_corner");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: rect_round_corner must be non-negative"));
            goto fail;
        }
        tmp.rect_round_corner=(float)v;
    }

    /* 7. 根下每个非空名字的表都是一套配色切片 */
    size_t slice_count=0;
    for(size_t i=0;i<t.size;i++){
        toml_pair_t *pair=&t.pairs[i];
        if(pair->key==NULL||pair->key[0]=='\0'){
            continue;   /* "" 里放的是全局设置 */
        }
        if(pair->value!=NULL&&*pair->value==TOML_TABLE){
            slice_count++;
        }
    }
    if(slice_count==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s contains no theme slices"),file);
        goto fail;
    }

    tmp.slices=calloc(slice_count,sizeof(*tmp.slices));
    if(tmp.slices==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate theme slices"));
        goto fail;
    }
    tmp.slice_size=slice_count;

    /* 先全部清零，失败时 theme_free_arrays 可以安全地按 slice_size 回收 */
    size_t si=0;
    for(size_t i=0;i<t.size;i++){
        toml_pair_t *pair=&t.pairs[i];
        if(pair->key==NULL||pair->key[0]=='\0'){
            continue;
        }
        if(pair->value==NULL||*pair->value!=TOML_TABLE){
            continue;
        }

        theme_slice_t *slice=&tmp.slices[si++];
        slice->name=pair->key;

        slice->color=theme_read_colors(toml_lookup_from_table((toml_table_t *)pair->value,"color"),
                                       slice->name,"color",&slice->color_size);
        if(slice->color==NULL){
            goto fail;
        }
        slice->text_color=theme_read_colors(toml_lookup_from_table((toml_table_t *)pair->value,"text_color"),
                                            slice->name,"text_color",&slice->text_color_size);
        if(slice->text_color==NULL){
            goto fail;
        }

        /* 未配置时回退到调色板的次要色 */
        slice->divider_color=(slice->color_size>(size_t)COLOR_SECONDARY)?slice->color[COLOR_SECONDARY]:0x7F7F7FFF;
        tp=toml_lookup_from_table((toml_table_t *)pair->value,"divider_color");
        if(tp!=NULL&&!theme_read_color(tp,slice->name,"divider_color",&slice->divider_color)){
            goto fail;
        }
    }

    /* 8. default 必须指向已经读进来的某套切片；没写就用第一套 */
    tmp.now_slice=&tmp.slices[0];
    tp=toml_lookup(&t,"",0,"default");
    if(tp!=NULL){
        if(*tp!=TOML_STRING){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: default must be a string"));
            goto fail;
        }
        const char *default_name=((toml_string_t *)tp)->str;
        int found=0;
        for(size_t i=0;i<tmp.slice_size;i++){
            if(strcmp(tmp.slices[i].name,default_name)==0){
                tmp.now_slice=&tmp.slices[i];
                found=1;
                break;
            }
        }
        if(!found){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: default theme %s does not exist"),default_name);
            goto fail;
        }
    }

    /* 给每个图标下标预留纹理槽，theme_load_icons() 再按规则逐个加载 */
    size_t icon_count=icon_index_max>0?icon_index_max:1;
    tmp.icons=calloc(icon_count,sizeof(*tmp.icons));
    if(tmp.icons==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate icon slots"));
        goto fail;
    }
    tmp.icon_size=icon_count;

    /* 到这里 tmp 已经完整且有效，才释放旧配置并覆盖：
       旧数组先回收，再卸掉旧 TOML（tmp 里的字符串属于新 TOML，最后才接管）。
       字体本身归缓存所有，不在这里关闭。 */
    theme_free_loaded();
    toml_unload(&theme_toml);
    theme_toml=t;
    theme=tmp;
    theme_load_icons();

    /* 主题已经加载成功，这时才按环境 locale 启用 gettext */
    sdlgui_i18n_init();
    theme_select_system();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                _("theme_set: loaded theme %s from %s (%zu slices, %zu fonts)"),
                theme.name,file,theme.slice_size,theme.font_size);
    return ;

fail:
    free(config_dir);
    config_dir=NULL;
    theme_free_arrays(&tmp);
    toml_unload(&t);
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                 _("theme_set: %s failed validation; keeping the current theme"),file);
}



int theme_select_slice(const char *name){
    if(name==NULL){
        return 0;
    }
    for(size_t i=0;i<theme.slice_size;i++){
        if(theme.slices[i].name!=NULL&&strcmp(theme.slices[i].name,name)==0){
            theme.now_slice=&theme.slices[i];
            return 1;
        }
    }
    return 0;
}

int theme_select_system(void){
    SDL_SystemTheme system_theme=SDL_GetSystemTheme();
    if(system_theme==SDL_SYSTEM_THEME_DARK){
        return theme_select_slice("Dark");
    }
    if(system_theme==SDL_SYSTEM_THEME_LIGHT){
        return theme_select_slice("Bright");
    }
    return 0;
}

/* 主题数组可能比枚举短（旧配置只写了一个 text_color），越界时回退到最后一项 */
static inline size_t theme_index(size_t index,size_t count){
    if(count==0){
        return 0;
    }
    return index<count?index:count-1;
}

unsigned get_color(theme_color_t index){
    theme_slice_t *now=theme.now_slice;
    return now->color[theme_index((size_t)index,now->color_size)];
}
unsigned get_text_color(theme_text_t index){
    theme_slice_t *now=theme.now_slice;
    return now->text_color[theme_index((size_t)index,now->text_color_size)];
}
unsigned get_divider_color(void){
    return theme.now_slice->divider_color;
}
const char *get_font(theme_text_t index){
    return theme.fonts[theme_index((size_t)index,theme.font_size)];
}
float get_text_size(theme_text_t index){
    return theme.text[theme_index((size_t)index,theme.text_size)];
}

/* lost 图标是占位图，统一乘上当前文字色，深色/浅色主题都能看清。
 * 主题自带的 lost.png 也走这里；其他图标保持原色。 */
static void theme_tint_lost(SDL_Texture *texture){
    if(texture==NULL){
        return;
    }
    unsigned color=get_text_color(TEXT_CONTEXT);
    SDL_SetTextureColorMod(texture,
                           (Uint8)((color>>24)&0xFF),
                           (Uint8)((color>>16)&0xFF),
                           (Uint8)((color>>8)&0xFF));
    SDL_SetTextureAlphaMod(texture,(Uint8)(color&0xFF));
}

SDL_Texture *get_icon(theme_icon_t index){
    size_t slot=(size_t)index;
    if(theme.name!=NULL&&theme.icons!=NULL&&slot<theme.icon_size&&theme.icons[slot]!=NULL){
        if(index==ICON_LOST){
            theme_tint_lost(theme.icons[slot]);
        }
        return theme.icons[slot];
    }

    /* 旋钮只从主题加载，不提供内置或 lost 回退 */
    if(index==ICON_KNOB){
        return NULL;
    }

    /* 外部主题可能自己带了 lost.png，缺失的图标先回退到它 */
    if(theme.name!=NULL&&theme.icons!=NULL&&(size_t)ICON_LOST<theme.icon_size&&
       theme.icons[ICON_LOST]!=NULL){
        theme_tint_lost(theme.icons[ICON_LOST]);
        return theme.icons[ICON_LOST];
    }

    /* 内置主题，或外部主题缺少该图标：回退到编译期内嵌的问号 */
    theme_tint_lost(builtin_lost_texture);
    return builtin_lost_texture;
}

const theme_t *get_theme(void){
    return &theme;
}
const theme_slice_t *get_theme_slice(void){
    return theme.now_slice;
}


void theme_quit(void){
    theme_free_loaded();
    release_icon_rules();
    if(builtin_lost_texture!=NULL){
        SDL_DestroyTexture(builtin_lost_texture);
        builtin_lost_texture=NULL;
    }
    toml_unload(&theme_toml);
    release_font_cache();
}


static TTF_Font *open_font_file(const char *name,const char *path,int index,float ptsize){
    if(path==NULL){
        return NULL;
    }

    SDL_PropertiesID props=SDL_CreateProperties();
    if(props==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to create font properties: %s"),SDL_GetError());
        return NULL;
    }

    TTF_Font *font=NULL;
    if(SDL_SetStringProperty(props,TTF_PROP_FONT_CREATE_FILENAME_STRING,path)&&
       SDL_SetFloatProperty(props,TTF_PROP_FONT_CREATE_SIZE_FLOAT,ptsize)&&
       SDL_SetNumberProperty(props,TTF_PROP_FONT_CREATE_FACE_NUMBER,(Sint64)index)){
        font=TTF_OpenFontWithProperties(props);
    }else{
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to set font properties: %s"),SDL_GetError());
    }
    SDL_DestroyProperties(props);

    if(font==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to open font %s (index %d): %s"),path,index,SDL_GetError());
    }else{
        TTF_SetFontHinting(font,TTF_HINTING_MONO);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,_("create_font: %s -> %s (index %d, %.0f px)"),name,path,index,ptsize);
    }
    return font;
}

/* 通过平台字体后端解析字体名，返回新字体以及解析出的路径/index（路径由调用方释放）。
 * ptsize 是物理像素尺寸（调用方已经乘过 UI 缩放）。 */
static TTF_Font *create_font(const char *name,float ptsize,char **path_out,int *index_out){
    if(path_out!=NULL){
        *path_out=NULL;
    }
    if(index_out!=NULL){
        *index_out=0;
    }
    if(name==NULL||name[0]=='\0'){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: font name must not be empty"));
        return NULL;
    }
    if(!TTF_WasInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: TTF is not initialized"));
        return NULL;
    }

    /* 把 "字体族[:style=...]" 解析成具体字体文件：macOS 走 CoreText，其他平台走 fontconfig */
    int index=0;
    char *path=sdlgui_font_lookup(name,&index);
    if(path==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: could not find font %s"),name);
        return NULL;
    }

    TTF_Font *font=open_font_file(name,path,index,ptsize);
    if(font==NULL){
        free(path);
        return NULL;
    }

    if(path_out!=NULL){
        *path_out=path;
    }else{
        free(path);
    }
    if(index_out!=NULL){
        *index_out=index;
    }
    return font;
}

typedef struct {
    char *name;    /* NULL 表示内嵌字体 */
    float ptsize;  /* 逻辑字号，缓存键 */

    char *path;    /* 平台字体后端解析出的字体文件；内嵌/回退字体为 NULL */
    int index;     /* .ttc/.otc 的 face index */
    TTF_Font *font;
}font_pair;

static darray_t(font_pair) font_cache;

/* NULL 字体名排在最前，其余按字符串比较 */
static int font_cache_name_cmp(const char *a,const char *b){
    if(a==NULL){
        return b==NULL?0:-1;
    }
    if(b==NULL){
        return 1;
    }
    return strcmp(a,b);
}

static int font_pair_cmp(const font_pair *a,const font_pair *b){
    int name_cmp=font_cache_name_cmp(a->name,b->name);
    if(name_cmp!=0){
        return name_cmp;
    }
    if(a->ptsize<b->ptsize){
        return -1;
    }
    if(a->ptsize>b->ptsize){
        return 1;
    }
    return 0;
}

/* 缓存按 (name, ptsize) 升序排列，返回第一个不小于 key 的位置 */
static size_t font_cache_lower_bound(const font_pair *key){
    size_t lo=0;
    size_t hi=font_cache.size;
    while(lo<hi){
        size_t mid=lo+(hi-lo)/2;
        if(font_pair_cmp(&font_cache.arr[mid],key)<0){
            lo=mid+1;
        }else{
            hi=mid;
        }
    }
    return lo;
}

/* 精确查找 (name, ptsize) */
static font_pair *font_cache_find(const char *name,float ptsize){
    font_pair key={.name=(char *)name,.ptsize=ptsize};
    size_t i=font_cache_lower_bound(&key);
    if(i<font_cache.size&&font_pair_cmp(&font_cache.arr[i],&key)==0){
        return &font_cache.arr[i];
    }
    return NULL;
}

/* 查找同名且已经记录文件路径的项，用于换字号时跳过字体查找 */
static font_pair *font_cache_find_named(const char *name){
    font_pair key={.name=(char *)name,.ptsize=0.0f};
    size_t i=font_cache_lower_bound(&key);
    while(i<font_cache.size&&font_cache_name_cmp(font_cache.arr[i].name,name)==0){
        if(font_cache.arr[i].path!=NULL){
            return &font_cache.arr[i];
        }
        i++;
    }
    return NULL;
}

/* 二分插入，保持缓存按 (name, ptsize) 有序 */
static int font_cache_insert(font_pair *entry){
    font_pair key={.name=entry->name,.ptsize=entry->ptsize};
    size_t pos=font_cache_lower_bound(&key);
    return da_add_loc(sizeof(*entry),&font_cache,entry,pos);
}

/* 关闭缓存里的所有字体；必须在 TTF_Quit() 之前调用 */
static void release_font_cache(void){
    for(size_t i=0;i<font_cache.size;i++){
        if(font_cache.arr[i].font!=NULL){
            TTF_CloseFont(font_cache.arr[i].font);
        }
        free(font_cache.arr[i].name);
        free(font_cache.arr[i].path);
    }
    da_clear(&font_cache);
}

TTF_Font *obtain_font(const char *name,float ptsize){
    if(name!=NULL&&name[0]=='\0'){
        name=NULL;
    }
    if(!isfinite(ptsize)||ptsize<=0.0f){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: invalid point size %.1f"),(double)ptsize);
        return NULL;
    }

    font_pair *hit=font_cache_find(name,ptsize);
    if(hit!=NULL){
        return hit->font;
    }

    /* 缓存键用逻辑字号；实际栅格化按 Retina 缩放放大 */
    float pixel_size=ptsize*sdlgui_get_scale();

    TTF_Font *font=NULL;
    char *path=NULL;
    int index=0;

    /* 同名不同字号：已有文件路径就直接按路径打开，不再查字体后端 */
    font_pair *named=(name!=NULL)?font_cache_find_named(name):NULL;
    if(named!=NULL){
        font=open_font_file(name,named->path,named->index,pixel_size);
        if(font!=NULL){
            path=theme_dup_string(named->path);
            index=named->index;
            if(path==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: failed to copy font path"));
                TTF_CloseFont(font);
                return NULL;
            }
        }
    }

    /* 没有记录过路径时才走字体后端 */
    if(font==NULL&&name!=NULL&&named==NULL){
        font=create_font(name,pixel_size,&path,&index);
    }

    if(font==NULL){
        font=create_embedded_font(pixel_size);
        if(font==NULL){
            free(path);
            return NULL;
        }
        if(name!=NULL){
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        _("obtain_font: font %s not found; using the embedded font"),name);
        }
        free(path);
        path=NULL;
        index=0;
    }

    char *name_copy=theme_dup_string(name);
    if(name!=NULL&&name_copy==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: failed to copy font name"));
        free(path);
        TTF_CloseFont(font);
        return NULL;
    }

    font_pair entry={
        .name=name_copy,
        .ptsize=ptsize,
        .path=path,
        .index=index,
        .font=font
    };
    if(font_cache_insert(&entry)!=0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: failed to grow the font cache"));
        free(entry.name);
        free(entry.path);
        TTF_CloseFont(entry.font);
        return NULL;
    }
    return font;
}


/* ---- 图标 ---- */

static void release_icon_rules(void){
    for(size_t i=0;i<icon_rules.size;i++){
        free(icon_rules.arr[i].name);
    }
    da_clear(&icon_rules);
    icon_index_max=0;
}

/* 内置 lost 图标的 RGBA 数据 -> surface；数据是静态的，surface 不拥有像素 */
static SDL_Surface *theme_builtin_lost_surface(void){
    if(pacman_lost_icon_w<=0||pacman_lost_icon_h<=0){
        return NULL;
    }
    size_t expected=(size_t)pacman_lost_icon_w*(size_t)pacman_lost_icon_h*4u;
    if(pacman_lost_icon_len<expected){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme: built-in lost icon has %zu bytes, expected %zu"),
                     pacman_lost_icon_len,expected);
        return NULL;
    }
    return SDL_CreateSurfaceFrom(pacman_lost_icon_w,pacman_lost_icon_h,
                                 SDL_PIXELFORMAT_RGBA32,(void *)pacman_lost_icon,
                                 pacman_lost_icon_w*4);
}

static SDL_Texture *theme_texture_from_surface(SDL_Surface *surface){
    if(surface==NULL){
        return NULL;
    }

    SDL_Renderer *renderer=get_renderer();
    if(renderer==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("load_icon: renderer is not available"));
        SDL_DestroySurface(surface);
        return NULL;
    }

    SDL_Texture *texture=SDL_CreateTextureFromSurface(renderer,surface);
    SDL_DestroySurface(surface);
    if(texture==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("load_icon: SDL_CreateTextureFromSurface failed: %s"),SDL_GetError());
        return NULL;
    }
    SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
    return texture;
}

SDL_Texture *load_icon(const char *path,const char *name){
    char *filename=theme_icon_filename(name);
    if(filename==NULL){
        return NULL;
    }

    SDL_Surface *surface=NULL;
    if(path==NULL){
        /* 内置主题没有图标目录，只有编译期嵌入的 lost */
        if(SDL_strcasecmp(filename,"lost.png")==0){
            surface=theme_builtin_lost_surface();
        }else{
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                         _("load_icon: no built-in icon named %s"),filename);
        }
        free(filename);
    }else{
        char *file=theme_join_path(path,filename);
        free(filename);
        if(file!=NULL){
            surface=SDL_LoadSurface(file);
            if(surface==NULL){
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                             _("load_icon: cannot load %s: %s"),file,SDL_GetError());
            }
            free(file);
        }
    }

    return theme_texture_from_surface(surface);
}

static int theme_create_builtin_lost(void){
    if(builtin_lost_texture!=NULL){
        return 1;
    }
    builtin_lost_texture=load_icon(NULL,"lost");
    if(builtin_lost_texture==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_init: failed to create the built-in lost icon"));
        return 0;
    }
    return 1;
}

/* 主题加载后新增规则时，按需扩大纹理槽数组 */
static int theme_ensure_icon_slots(void){
    if(theme.name==NULL){
        return 1;   /* 内置主题直接用 builtin_lost_texture */
    }
    if(icon_index_max<=theme.icon_size){
        return 1;
    }

    size_t count=icon_index_max;
    SDL_Texture **grown=realloc(theme.icons,count*sizeof(*grown));
    if(grown==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme: failed to grow icon slots"));
        return 0;
    }
    for(size_t i=theme.icon_size;i<count;i++){
        grown[i]=NULL;
    }
    theme.icons=grown;
    theme.icon_size=count;
    return 1;
}

/* 按规则名加载还没有纹理的槽；同名别名共享同一个 slot */
static void theme_load_icons(void){
    if(theme.name==NULL||theme.icon_path==NULL||theme.icons==NULL){
        return;
    }

    SDL_PathInfo info;
    if(!SDL_GetPathInfo(theme.icon_path,&info)||info.type!=SDL_PATHTYPE_DIRECTORY){
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    _("theme: icon directory %s not found; using built-in icons"),
                    theme.icon_path);
        return;
    }

    for(size_t i=0;i<icon_rules.size;i++){
        size_t slot=(size_t)icon_rules.arr[i].index;
        if(slot>=theme.icon_size||theme.icons[slot]!=NULL){
            continue;
        }
        theme.icons[slot]=load_icon(theme.icon_path,icon_rules.arr[i].name);
    }
}

static size_t icon_rule_lower_bound(const char *name){
    size_t low=0;
    size_t high=icon_rules.size;
    while(low<high){
        size_t mid=low+(high-low)/2;
        if(strcmp(icon_rules.arr[mid].name,name)<0){
            low=mid+1;
        }else{
            high=mid;
        }
    }
    return low;
}

/* 规则名必须能安全地当作标识符/文件名：
 * 只允许 A-Z a-z 0-9 和 '_'，且首字符不能是数字。 */
static int icon_rule_name_valid(const char *name){
    if(name==NULL||name[0]=='\0'){
        return 0;
    }

    unsigned char first=(unsigned char)name[0];
    if(!((first>='A'&&first<='Z')||(first>='a'&&first<='z')||first=='_')){
        return 0;
    }

    for(const char *p=name;*p!='\0';p++){
        unsigned char c=(unsigned char)*p;
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')){
            return 0;
        }
    }
    return 1;
}

long add_icon_rule(const char *name,long index){
    if(!icon_rule_name_valid(name)){
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    _("add_icon_rule: invalid icon name \"%s\""),
                    name!=NULL?name:"(null)");
        return -1;
    }

    size_t pos=icon_rule_lower_bound(name);
    if(pos<icon_rules.size&&strcmp(icon_rules.arr[pos].name,name)==0){
        return -1;   /* 同名规则已经存在 */
    }

    size_t old_max=icon_index_max;
    long assigned=index;
    if(assigned<0){
        if(icon_index_max>=(size_t)LONG_MAX){
            return -1;
        }
        assigned=(long)icon_index_max++;
    }else if((size_t)assigned>=icon_index_max){
        icon_index_max=(size_t)assigned+1;
    }

    char *copy=theme_dup_string(name);
    if(copy==NULL){
        icon_index_max=old_max;
        return -1;
    }

    icon_pair_t pair={.name=copy,.index=(theme_icon_t)assigned};
    if(da_add_loc(sizeof(pair),&icon_rules,&pair,pos)!=0){
        free(copy);
        icon_index_max=old_max;
        return -1;
    }

    if(theme.name!=NULL&&theme_ensure_icon_slots()){
        theme_load_icons();
    }
    return assigned;
}

int del_icon_rule(const char *name,theme_icon_t index){
    if(name==NULL){
        return 1;
    }

    size_t pos=icon_rule_lower_bound(name);
    if(pos>=icon_rules.size||strcmp(icon_rules.arr[pos].name,name)!=0||
       icon_rules.arr[pos].index!=index){
        return 1;
    }

    theme_icon_t removed=icon_rules.arr[pos].index;
    free(icon_rules.arr[pos].name);
    da_del(sizeof(icon_pair_t),&icon_rules,pos);

    int used=0;
    for(size_t i=0;i<icon_rules.size;i++){
        if(icon_rules.arr[i].index==removed){
            used=1;
            break;
        }
    }
    if(!used&&theme.name!=NULL&&theme.icons!=NULL&&(size_t)removed<theme.icon_size){
        if(theme.icons[(size_t)removed]!=NULL){
            SDL_DestroyTexture(theme.icons[(size_t)removed]);
            theme.icons[(size_t)removed]=NULL;
        }
    }
    return 0;
}

unsigned long find_icon(const char *name){
    if(!icon_rule_name_valid(name)){
        return ICON_LOST;
    }
    size_t pos=icon_rule_lower_bound(name);
    if(pos<icon_rules.size&&strcmp(icon_rules.arr[pos].name,name)==0){
        return icon_rules.arr[pos].index;
    }
    return ICON_LOST;
}
