#include "sdlgui_fontlookup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SDLGUI_FONT_BACKEND_CORETEXT

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <stdint.h>
#include <strings.h>

/* ---- 读取 OpenType/TrueType 的 name 表，按 PostScript 名找 face 序号 ----
 * CoreText 只告诉我们字体文件，不告诉我们 .ttc 里的第几个 face；
 * SDL3_ttf 打开 .ttc 时需要 face 序号，所以这里自己读 name 表。 */

static uint16_t font_read_be16(const unsigned char *p){
    return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);
}

static uint32_t font_read_be32(const unsigned char *p){
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}

static int font_read_at(FILE *fp,long offset,void *buffer,size_t size){
    return fseek(fp,offset,SEEK_SET)==0&&fread(buffer,1,size,fp)==size;
}

/* 把 name 表里的字符串转成 C 字符串；只接受 ASCII 的 PostScript 名 */
static int font_copy_name(const unsigned char *raw,unsigned length,int wide,char *out,size_t out_size){
    size_t n=0;
    if(wide){
        for(unsigned i=0;i+1<length;i+=2){
            unsigned ch=((unsigned)raw[i]<<8)|raw[i+1];
            if(ch==0){
                continue;
            }
            if(ch>0x7F||n+1>=out_size){
                return 0;
            }
            out[n++]=(char)ch;
        }
    }else{
        for(unsigned i=0;i<length;i++){
            if(raw[i]>0x7F||n+1>=out_size){
                return 0;
            }
            out[n++]=(char)raw[i];
        }
    }
    out[n]='\0';
    return n>0;
}

static int font_sfnt_ps_name(FILE *fp,long base,char *out,size_t out_size){
    unsigned char header[12];
    if(!font_read_at(fp,base,header,sizeof(header))){
        return 0;
    }

    unsigned tables=font_read_be16(header+4);
    if(tables==0||tables>512){
        return 0;
    }

    long name_offset=-1;
    for(unsigned i=0;i<tables;i++){
        unsigned char record[16];
        if(!font_read_at(fp,base+12+(long)i*16,record,sizeof(record))){
            return 0;
        }
        if(memcmp(record,"name",4)==0){
            name_offset=(long)font_read_be32(record+8);
            break;
        }
    }
    if(name_offset<0){
        return 0;
    }

    unsigned char table[6];
    if(!font_read_at(fp,name_offset,table,sizeof(table))){
        return 0;
    }
    unsigned count=font_read_be16(table+2);
    unsigned storage=font_read_be16(table+4);
    if(count==0||count>1024){
        return 0;
    }

    unsigned best_rank=0;
    long best_offset=0;
    unsigned best_length=0;

    for(unsigned i=0;i<count;i++){
        unsigned char record[12];
        if(!font_read_at(fp,name_offset+6+(long)i*12,record,sizeof(record))){
            return 0;
        }
        unsigned platform=font_read_be16(record);
        unsigned name_id=font_read_be16(record+6);
        unsigned length=font_read_be16(record+8);
        unsigned offset=font_read_be16(record+10);
        if(name_id!=6||length==0||length>512){
            continue;
        }

        /* Windows/Unicode 的 UTF-16BE 优先，Mac Roman 次之 */
        unsigned rank=(platform==3||platform==0)?2:(platform==1?1:0);
        if(rank>best_rank){
            best_rank=rank;
            best_offset=name_offset+(long)storage+offset;
            best_length=length;
        }
    }
    if(best_rank==0){
        return 0;
    }

    unsigned char raw[512];
    if(!font_read_at(fp,best_offset,raw,best_length)){
        return 0;
    }
    return font_copy_name(raw,best_length,best_rank==2,out,out_size);
}

/* 不是字体集合时返回 0；找不到匹配的 PostScript 名时也返回 0 */
static int font_face_index(const char *path,const char *ps_name){
    if(path==NULL||ps_name==NULL||ps_name[0]=='\0'){
        return 0;
    }

    FILE *fp=fopen(path,"rb");
    if(fp==NULL){
        return 0;
    }

    int index=0;
    unsigned char header[12];
    if(font_read_at(fp,0,header,sizeof(header))&&memcmp(header,"ttcf",4)==0){
        uint32_t count=font_read_be32(header+8);
        if(count>256){
            count=256;
        }
        for(uint32_t i=0;i<count;i++){
            unsigned char offset[4];
            if(!font_read_at(fp,12+(long)i*4,offset,sizeof(offset))){
                break;
            }
            char name[256];
            if(font_sfnt_ps_name(fp,(long)font_read_be32(offset),name,sizeof(name))&&strcmp(name,ps_name)==0){
                index=(int)i;
                break;
            }
        }
    }

    fclose(fp);
    return index;
}

/* ---- CoreText 解析 ---- */

static CFStringRef font_cfstring(const char *text){
    if(text==NULL){
        return NULL;
    }
    return CFStringCreateWithCString(kCFAllocatorDefault,text,kCFStringEncodingUTF8);
}

static char *font_cfstring_copy(CFStringRef string){
    if(string==NULL){
        return NULL;
    }
    CFIndex max=CFStringGetMaximumSizeForEncoding(CFStringGetLength(string),kCFStringEncodingUTF8)+1;
    if(max<=1){
        return NULL;
    }
    char *buffer=malloc((size_t)max);
    if(buffer==NULL){
        return NULL;
    }
    if(!CFStringGetCString(string,buffer,max,kCFStringEncodingUTF8)){
        free(buffer);
        return NULL;
    }
    return buffer;
}

static char *font_trim(char *text){
    while(*text==' '||*text=='\t'){
        text++;
    }
    char *end=text+strlen(text);
    while(end>text&&(end[-1]==' '||end[-1]=='\t')){
        *--end='\0';
    }
    return text;
}

static int font_contains_case(const char *text,const char *word){
    size_t length=strlen(word);
    for(;*text!='\0';text++){
        if(strncasecmp(text,word,length)==0){
            return 1;
        }
    }
    return 0;
}

/* 通用族名映射到 macOS 的系统字体；返回 NULL 表示使用 ui_type */
static const char *font_generic_family(const char *family,CTFontUIFontType *ui_type){
    if(strcasecmp(family,"monospace")==0||strcasecmp(family,"mono")==0||
       strcasecmp(family,"fixed")==0||strcasecmp(family,"fixed-pitch")==0){
        *ui_type=kCTFontUIFontUserFixedPitch;
        return NULL;
    }
    if(strcasecmp(family,"sans-serif")==0||strcasecmp(family,"sans")==0||
       strcasecmp(family,"system-ui")==0||strcasecmp(family,"ui")==0){
        *ui_type=kCTFontUIFontSystem;
        return NULL;
    }
    if(strcasecmp(family,"serif")==0){
        return "Times New Roman";
    }
    if(strcasecmp(family,"cursive")==0){
        return "Apple Chancery";
    }
    if(strcasecmp(family,"fantasy")==0){
        return "Papyrus";
    }
    return family;
}

static CTFontRef font_apply_traits(CTFontRef font,const char *style){
    if(font==NULL||style==NULL||style[0]=='\0'){
        return font;
    }

    CTFontSymbolicTraits traits=0;
    if(font_contains_case(style,"bold")){
        traits|=kCTFontBoldTrait;
    }
    if(font_contains_case(style,"italic")||font_contains_case(style,"oblique")){
        traits|=kCTFontItalicTrait;
    }
    if(traits==0){
        return font;
    }

    CTFontRef styled=CTFontCreateCopyWithSymbolicTraits(font,12.0,NULL,traits,traits);
    if(styled==NULL){
        return font;
    }
    CFRelease(font);
    return styled;
}

static CTFontRef font_create(CFStringRef family,CFStringRef style){
    if(family==NULL){
        return NULL;
    }

    CFMutableDictionaryRef attributes=CFDictionaryCreateMutable(
        kCFAllocatorDefault,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    if(attributes==NULL){
        return NULL;
    }
    CFDictionarySetValue(attributes,kCTFontFamilyNameAttribute,family);
    if(style!=NULL){
        CFDictionarySetValue(attributes,kCTFontStyleNameAttribute,style);
    }

    CTFontDescriptorRef descriptor=CTFontDescriptorCreateWithAttributes(attributes);
    CFRelease(attributes);
    if(descriptor==NULL){
        return NULL;
    }

    CTFontDescriptorRef match=CTFontDescriptorCreateMatchingFontDescriptor(descriptor,NULL);
    CTFontRef font=CTFontCreateWithFontDescriptor(match!=NULL?match:descriptor,12.0,NULL);

    if(match!=NULL){
        CFRelease(match);
    }
    CFRelease(descriptor);
    return font;
}

static CTFontRef font_create_for_spec(const char *family,const char *style){
    CTFontUIFontType ui_type=0;
    const char *mapped=font_generic_family(family,&ui_type);
    CTFontRef font=NULL;

    if(mapped==NULL){
        font=CTFontCreateUIFontForLanguage(ui_type,12.0,NULL);
    }else{
        CFStringRef family_cf=font_cfstring(mapped);
        CFStringRef style_cf=font_cfstring((style!=NULL&&style[0]!='\0')?style:NULL);
        if(family_cf!=NULL){
            if(style_cf!=NULL){
                font=font_create(family_cf,style_cf);
                if(font!=NULL){
                    /* 已经按 style 精确匹配到，不再叠加 traits */
                    CFRelease(style_cf);
                    CFRelease(family_cf);
                    return font;
                }
            }
            font=font_create(family_cf,NULL);
            if(font==NULL){
                /* 未知族名交给 CoreText 做最后的回退 */
                font=CTFontCreateWithName(family_cf,12.0,NULL);
            }
            if(font!=NULL){
                font=font_apply_traits(font,style);
            }
            if(style_cf!=NULL){
                CFRelease(style_cf);
            }
            CFRelease(family_cf);
        }
        return font;
    }

    if(font!=NULL){
        font=font_apply_traits(font,style);
    }
    return font;
}

/* 从字体取文件路径，并补齐 .ttc 的 face 序号 */
static char *font_copy_file(CTFontRef font,int *index){
    if(font==NULL){
        return NULL;
    }

    CTFontDescriptorRef descriptor=CTFontCopyFontDescriptor(font);
    if(descriptor==NULL){
        return NULL;
    }
    CFTypeRef value=CTFontDescriptorCopyAttribute(descriptor,kCTFontURLAttribute);
    CFRelease(descriptor);
    if(value==NULL){
        return NULL;
    }

    char *path=NULL;
    if(CFGetTypeID(value)==CFURLGetTypeID()){
        CFStringRef file_path=CFURLCopyFileSystemPath((CFURLRef)value,kCFURLPOSIXPathStyle);
        if(file_path!=NULL){
            path=font_cfstring_copy(file_path);
            CFRelease(file_path);
        }
    }
    CFRelease(value);
    if(path==NULL){
        return NULL;
    }

    if(index!=NULL){
        *index=0;
        CFStringRef ps_name=CTFontCopyPostScriptName(font);
        if(ps_name!=NULL){
            char *ps=font_cfstring_copy(ps_name);
            if(ps!=NULL){
                *index=font_face_index(path,ps);
                free(ps);
            }
            CFRelease(ps_name);
        }
    }
    return path;
}

char *sdlgui_font_lookup(const char *name,int *index){
    if(index!=NULL){
        *index=0;
    }
    if(name==NULL||name[0]=='\0'){
        return NULL;
    }

    size_t length=strlen(name);
    char *spec=malloc(length+1);
    if(spec==NULL){
        return NULL;
    }
    memcpy(spec,name,length+1);

    /* 只认识 fontconfig 写法里的 :style=...；其余属性忽略 */
    char *style=NULL;
    char *colon=strchr(spec,':');
    if(colon!=NULL){
        *colon='\0';
        char *item=colon+1;
        while(*item!='\0'){
            char *end=strchr(item,':');
            if(end!=NULL){
                *end='\0';
            }
            char *equal=strchr(item,'=');
            if(equal!=NULL){
                *equal='\0';
                if(strcasecmp(font_trim(item),"style")==0){
                    style=font_trim(equal+1);
                }
            }
            if(end==NULL){
                break;
            }
            item=end+1;
        }
    }

    char *family=font_trim(spec);
    char *path=NULL;
    if(family[0]!='\0'){
        CTFontRef font=font_create_for_spec(family,style);
        path=font_copy_file(font,index);
        if(font!=NULL){
            CFRelease(font);
        }
    }

    free(spec);
    return path;
}

#else

#include <fontconfig/fontconfig.h>

char *sdlgui_font_lookup(const char *name,int *index){
    if(index!=NULL){
        *index=0;
    }
    if(name==NULL||name[0]=='\0'){
        return NULL;
    }

    FcPattern *pattern=FcNameParse((const FcChar8 *)name);
    if(pattern==NULL){
        return NULL;
    }
    FcConfigSubstitute(NULL,pattern,FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result=FcResultMatch;
    FcPattern *match=FcFontMatch(NULL,pattern,&result);
    FcPatternDestroy(pattern);
    if(match==NULL){
        return NULL;
    }

    FcChar8 *file=NULL;
    if(FcPatternGetString(match,FC_FILE,0,&file)!=FcResultMatch||file==NULL){
        FcPatternDestroy(match);
        return NULL;
    }

    int face=0;
    FcPatternGetInteger(match,FC_INDEX,0,&face);

    char *path=malloc(strlen((const char *)file)+1);
    if(path!=NULL){
        strcpy(path,(const char *)file);
        if(index!=NULL){
            *index=face;
        }
    }
    FcPatternDestroy(match);
    return path;
}

#endif
