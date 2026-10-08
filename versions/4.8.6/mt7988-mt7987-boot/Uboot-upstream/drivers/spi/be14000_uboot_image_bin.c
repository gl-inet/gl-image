// SPDX-License-Identifier: GPL-2.0+

#include <be14000_uboot_image.h>

extern unsigned char be14000_uboot_gl_inet_lz4_start[];
extern unsigned char be14000_uboot_gl_inet_lz4_end[];
extern unsigned char be14000_uboot_model_lz4_start[];
extern unsigned char be14000_uboot_model_lz4_end[];
extern unsigned char be14000_uboot_ip_lz4_start[];
extern unsigned char be14000_uboot_ip_lz4_end[];
extern unsigned char be14000_uboot_5s_lz4_start[];
extern unsigned char be14000_uboot_5s_lz4_end[];
extern unsigned char be14000_uboot_4s_lz4_start[];
extern unsigned char be14000_uboot_4s_lz4_end[];
extern unsigned char be14000_uboot_3s_lz4_start[];
extern unsigned char be14000_uboot_3s_lz4_end[];
extern unsigned char be14000_uboot_2s_lz4_start[];
extern unsigned char be14000_uboot_2s_lz4_end[];
extern unsigned char be14000_uboot_1s_lz4_start[];
extern unsigned char be14000_uboot_1s_lz4_end[];

#define BLOB(_s, _e) { .data = (_s), .end = (_e) }

struct st7789_lz4_blob be14000_map_booting =
	BLOB(be14000_uboot_gl_inet_lz4_start, be14000_uboot_gl_inet_lz4_end);

struct st7789_lz4_blob be14000_map_proc[7] = {
	BLOB(be14000_uboot_model_lz4_start, be14000_uboot_model_lz4_end),
	BLOB(be14000_uboot_1s_lz4_start, be14000_uboot_1s_lz4_end),
	BLOB(be14000_uboot_2s_lz4_start, be14000_uboot_2s_lz4_end),
	BLOB(be14000_uboot_3s_lz4_start, be14000_uboot_3s_lz4_end),
	BLOB(be14000_uboot_4s_lz4_start, be14000_uboot_4s_lz4_end),
	BLOB(be14000_uboot_5s_lz4_start, be14000_uboot_5s_lz4_end),
	BLOB(be14000_uboot_ip_lz4_start, be14000_uboot_ip_lz4_end),
};

__asm__(
".section .rodata\n"
".balign 4\n"
".global be14000_uboot_gl_inet_lz4_start\n"
"be14000_uboot_gl_inet_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-gl-inet.bin.lz4\"\n"
".global be14000_uboot_gl_inet_lz4_end\n"
"be14000_uboot_gl_inet_lz4_end:\n"
".global be14000_uboot_model_lz4_start\n"
"be14000_uboot_model_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-model.bin.lz4\"\n"
".global be14000_uboot_model_lz4_end\n"
"be14000_uboot_model_lz4_end:\n"
".global be14000_uboot_ip_lz4_start\n"
"be14000_uboot_ip_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-ip.bin.lz4\"\n"
".global be14000_uboot_ip_lz4_end\n"
"be14000_uboot_ip_lz4_end:\n"
".global be14000_uboot_5s_lz4_start\n"
"be14000_uboot_5s_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-5s.bin.lz4\"\n"
".global be14000_uboot_5s_lz4_end\n"
"be14000_uboot_5s_lz4_end:\n"
".global be14000_uboot_4s_lz4_start\n"
"be14000_uboot_4s_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-4s.bin.lz4\"\n"
".global be14000_uboot_4s_lz4_end\n"
"be14000_uboot_4s_lz4_end:\n"
".global be14000_uboot_3s_lz4_start\n"
"be14000_uboot_3s_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-3s.bin.lz4\"\n"
".global be14000_uboot_3s_lz4_end\n"
"be14000_uboot_3s_lz4_end:\n"
".global be14000_uboot_2s_lz4_start\n"
"be14000_uboot_2s_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-2s.bin.lz4\"\n"
".global be14000_uboot_2s_lz4_end\n"
"be14000_uboot_2s_lz4_end:\n"
".global be14000_uboot_1s_lz4_start\n"
"be14000_uboot_1s_lz4_start:\n"
".incbin \"include/uboot-image/be14000/be14000-uboot-1s.bin.lz4\"\n"
".global be14000_uboot_1s_lz4_end\n"
"be14000_uboot_1s_lz4_end:\n"
);
