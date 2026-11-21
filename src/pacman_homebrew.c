/*
 * Copyright (C) 2026 Zhang Cw and pacman-gui contributors
 *
 * This file is part of pacman-gui.
 *
 * pacman-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pacman-gui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with pacman-gui.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "pacman_homebrew.h"

static pacman_t homebrew_backend;
static pacman_t *homebrew_self(void);

static char *homebrew_strdup(const char *text){
    if(text==NULL){
        return NULL;
    }
    size_t length=strlen(text)+1;
    char *copy=malloc(length);
    if(copy!=NULL){
        memcpy(copy,text,length);
    }
    return copy;
}

/* 读取一个 brew 条目并交给全局注册表，字段所有权同时转交 pkg_add()。 */
static int homebrew_append(const char *name,const char *version,const char *meta){
    package_t pkg;
    memset(&pkg,0,sizeof(pkg));

    pkg.from=homebrew_self();
    pkg.meta_data=(char *)meta;
    pkg.name=homebrew_strdup(name);
    pkg.version=homebrew_strdup(version);
    pkg.stat=PKG_STAT_INSTALLED;
    pkg.size=(unsigned long)-1;

    if(pkg.name==NULL||pkg.version==NULL){
        free(pkg.name);
        free(pkg.version);
        return 0;
    }

    pkg_add(&pkg);
    return 1;
}

static const char *homebrew_binary(void){
    const char *prefix=getenv("HOMEBREW_PREFIX");
    static char path[PATH_MAX];

    if(prefix!=NULL&&prefix[0]!='\0'){
        snprintf(path,sizeof(path),"%s/bin/brew",prefix);
        if(access(path,X_OK)==0){
            return path;
        }
    }

    static const char *candidates[]={
        "/opt/homebrew/bin/brew",
        "/usr/local/bin/brew",
        "/home/linuxbrew/.linuxbrew/bin/brew"
    };
    for(size_t i=0;i<sizeof(candidates)/sizeof(candidates[0]);i++){
        if(access(candidates[i],X_OK)==0){
            return candidates[i];
        }
    }

    const char *search=getenv("PATH");
    if(search!=NULL){
        while(*search!='\0'){
            const char *end=strchr(search,':');
            size_t length=end!=NULL?(size_t)(end-search):strlen(search);
            if(length>0&&length+6<sizeof(path)){
                snprintf(path,sizeof(path),"%.*s/brew",(int)length,search);
                if(access(path,X_OK)==0){
                    return path;
                }
            }
            if(end==NULL){
                break;
            }
            search=end+1;
        }
    }

    return NULL;
}

int pacman_homebrew_available(void){
    return homebrew_binary()!=NULL;
}

/* 读取 `brew list --versions --formula/--cask` 的文本输出。
 * 每行格式：name version [version ...]，取最后一个版本。 */
static int homebrew_list(const char *flag,const char *meta){
    const char *binary=homebrew_binary();
    if(binary==NULL){
        return 0;
    }

    char command[PATH_MAX+64];
    snprintf(command,sizeof(command),"\"%s\" list --versions %s",binary,flag);

    FILE *pipe=popen(command,"r");
    if(pipe==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "homebrew: failed to run %s",command);
        return 0;
    }

    char line[1024];
    int ok=1;
    while(fgets(line,sizeof(line),pipe)!=NULL){
        size_t length=strlen(line);
        while(length>0&&(line[length-1]=='\n'||line[length-1]=='\r')){
            line[--length]='\0';
        }
        if(line[0]=='\0'){
            continue;
        }

        char *name=line;
        while(*name==' '||*name=='\t'){
            name++;
        }
        char *cursor=name;
        while(*cursor!='\0'&&*cursor!=' '&&*cursor!='\t'){
            cursor++;
        }
        if(*cursor!='\0'){
            *cursor++='\0';
        }
        while(*cursor==' '||*cursor=='\t'){
            cursor++;
        }

        char *end=cursor+strlen(cursor);
        while(end>cursor&&(end[-1]==' '||end[-1]=='\t')){
            *--end='\0';
        }

        const char *version=cursor;
        char *last=cursor;
        for(char *p=cursor;*p!='\0';){
            while(*p==' '||*p=='\t'){
                p++;
            }
            if(*p=='\0'){
                break;
            }
            last=p;
            while(*p!='\0'&&*p!=' '&&*p!='\t'){
                p++;
            }
        }
        version=last;

        if(name[0]!='\0'&&!homebrew_append(name,version,meta)){
            ok=0;
            break;
        }
    }

    int status=pclose(pipe);
    if(status!=0){
        ok=0;
    }
    return ok;
}

static int homebrew_init(void){
    return pacman_homebrew_available()?1:0;
}

static void homebrew_list_installed(void){
    if(!pacman_homebrew_available()){
        return;
    }
    homebrew_list("--formula","formula");
    homebrew_list("--cask","cask");
}

/* 只保留能被 shell 安全引用的搜索字符：brew search 的关键词只会是包名，
 * 过滤掉空格、引号、分号等，避免用户输入被拼进命令。 */
static void homebrew_keyword_filter(const char *keyword,char *out,size_t size){
    if(out==NULL||size==0){
        return;
    }
    out[0]='\0';
    if(keyword==NULL){
        return;
    }

    size_t used=0;
    for(size_t i=0;keyword[i]!='\0'&&used+1<size;i++){
        unsigned char c=(unsigned char)keyword[i];
        int ok=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
               c=='@'||c=='.'||c=='_'||c=='+'||c=='-'||c=='/';
        if(ok){
            out[used++]=(char)c;
        }
    }
    out[used]='\0';
}

/* 读取 `brew search <flag> <keyword>`：每行一个包名，跳过 "==> Formulae"
 * 这类表头和空行。结果通过 pkg_available_add() 进入 packages 栏目。 */
static int homebrew_search_flag(const char *flag,const char *meta,const char *keyword){
    const char *binary=homebrew_binary();
    if(binary==NULL){
        return 0;
    }

    char command[PATH_MAX+128];
    snprintf(command,sizeof(command),
        "HOMEBREW_NO_AUTO_UPDATE=1 \"%s\" search %s \"%s\"",binary,flag,keyword);

    FILE *pipe=popen(command,"r");
    if(pipe==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "homebrew: failed to run %s",command);
        return 0;
    }

    char line[1024];
    while(fgets(line,sizeof(line),pipe)!=NULL){
        size_t length=strlen(line);
        while(length>0&&(line[length-1]=='\n'||line[length-1]=='\r')){
            line[--length]='\0';
        }

        char *name=line;
        while(*name==' '||*name=='\t'){
            name++;
        }
        if(name[0]=='\0'||name[0]=='='||name[0]=='>'){
            continue;
        }

        /* brew search 的名字是单 token；出现空白说明是提示信息，跳过 */
        char *cursor=name;
        while(*cursor!='\0'&&*cursor!=' '&&*cursor!='\t'){
            cursor++;
        }
        if(*cursor!='\0'){
            continue;
        }

        package_t pkg;
        memset(&pkg,0,sizeof(pkg));
        pkg.from=homebrew_self();
        pkg.meta_data=(char *)meta;
        pkg.name=homebrew_strdup(name);
        pkg.version=homebrew_strdup("unknown");
        pkg.stat=PKG_STAT_NULL;
        pkg.size=(unsigned long)-1;
        if(pkg.name==NULL||pkg.version==NULL){
            free(pkg.name);
            free(pkg.version);
            continue;
        }
        pkg_available_add(&pkg);
    }

    int status=pclose(pipe);
    return status==0?1:0;
}

static void homebrew_search(const char *keyword){
    if(!pacman_homebrew_available()){
        return;
    }

    char filtered[128];
    homebrew_keyword_filter(keyword,filtered,sizeof(filtered));
    if(filtered[0]=='\0'){
        return;
    }

    homebrew_search_flag("--formula","formula",filtered);
    homebrew_search_flag("--cask","cask",filtered);
}

static int homebrew_refresh(void){
    if(!pacman_homebrew_available()){
        return 0;
    }
    pkg_del_from(homebrew_self());
    homebrew_list_installed();
    return 1;
}

static pacman_t *homebrew_self(void){
    return &homebrew_backend;
}

static pacman_t homebrew_backend={
    .name="homebrew",
    .official=0,
    .init=homebrew_init,
    .refresh=homebrew_refresh,
    .list_installed=homebrew_list_installed,
    .search=homebrew_search
};

pacman_t *pacman_homebrew(void){
    return &homebrew_backend;
}
