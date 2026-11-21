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

#ifndef PACMAN_ALPM_H
#define PACMAN_ALPM_H

#include "pacman.h"

/* libalpm 后端。Makefile 检测到 libalpm 时定义 HAVE_LIBALPM 并编译
 * pacman_alpm.c；没有宏时 pacman.c 不会引用这里的接口。 */
pacman_t *pacman_alpm(void);
int pacman_alpm_available(void);

#endif
