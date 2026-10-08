/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Pointer-sized assembly macros for ARM.
 * Used by shared code (e.g. failsafe/fsdata/embed.S) that emits pointer data.
 */

#ifndef __ASM_ASM_H
#define __ASM_ASM_H

#if defined(CONFIG_ARM64) && !defined(__ILP32__)
#define PTR		.quad
#define PTRSIZE		8
#define PTRLOG		3
#else
#define PTR		.word
#define PTRSIZE		4
#define PTRLOG		2
#endif

#endif /* __ASM_ASM_H */
