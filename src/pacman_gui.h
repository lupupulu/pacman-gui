#ifndef PACMAN_GUI_H
#define PACMAN_GUI_H

#include "sdlgui.h"

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

    int installed;
    unsigned long size; // 占用大小，未知时填(unsigned long)-1

}package_t;

layout_rect_t *create_layout_package(package_t *package);

#endif