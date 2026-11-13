#ifndef PACMAN_H
#define PACMAN_H

#include <stddef.h>
#include "sdlgui.h"

typedef enum{
    PKG_STAT_NULL,
    PKG_STAT_INSTALLED,
    PKG_STAT_INSTALLING,
    PKG_STAT_UPDATING,
    PKG_STAT_UNINSTALLING
}pkg_stat_t;

typedef struct {
    char *from;  // 标记来自pacman/aur或其他管理器
    char *name;
    char *meta_data;  // 用于标记来自flatpak等存在该条目的管理器

    char *description;
    draft_texture_t *icon; // 封面图，如果有的话
    char *license;

    char *submitter;
    char **maintainers;
    char **contributors;

    char *version;
    unsigned long epoch;

    double popularity;

    char *detail;

    pkg_stat_t stat;
    double progress;
    unsigned long size; // 占用大小，未知时填(unsigned long)-1

    void *data; // 每个包管理器自有数据

}package_t;


void pkg_stat_update(package_t *pkg);

typedef struct {
    char *name; // 管理器名
    package_t *pkgs; // 管理的包
    size_t pkg_cnt;

    int (*init)(void);
    int (*init_settings)(void *tab);
    int (*refresh)(void); // 用于更新进度等
    int (*update)(void); // 更新源（对应pacman -Sy）
    int (*update_all)(void); // 更新所有包（对应pacman -Syu）
    void (*quit)(void);

    int (*find_package_from_repo)(const char *key,package_t *pkgs,unsigned int cnt);
    int (*install_package)(package_t *pkg);
    int (*update_package)(package_t *pkg);
    int (*uninstall_package)(package_t *pkg);
}pacman_t;

void pacman_register(pacman_t *m);

#endif