#ifndef PACMAN_ALPM_H
#define PACMAN_ALPM_H

#include "pacman.h"

/* libalpm 后端。Makefile 检测到 libalpm 时定义 HAVE_LIBALPM 并编译
 * pacman_alpm.c；没有宏时 pacman.c 不会引用这里的接口。 */
pacman_t *pacman_alpm(void);
int pacman_alpm_available(void);

#endif
