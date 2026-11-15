#ifndef PACMAN_HOMEBREW_H
#define PACMAN_HOMEBREW_H

#include "pacman.h"

/* Homebrew 后端。Makefile 检测到 brew 时定义 HAVE_HOMEBREW 并编译
 * pacman_homebrew.c；没有宏时 pacman.c 不会引用这里的接口。 */
pacman_t *pacman_homebrew(void);
int pacman_homebrew_available(void);

#endif
