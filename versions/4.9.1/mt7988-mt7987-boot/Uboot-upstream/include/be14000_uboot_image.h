#ifndef __BE14000_UBOOT_IMAGE_H__
#define __BE14000_UBOOT_IMAGE_H__

#ifndef __ST7789_LZ4_BLOB_DEF
#define __ST7789_LZ4_BLOB_DEF
struct st7789_lz4_blob {
	const unsigned char *data;
	const unsigned char *end;
};
#endif

extern struct st7789_lz4_blob be14000_map_booting;
extern struct st7789_lz4_blob be14000_map_proc[7];

#endif /* __BE14000_UBOOT_IMAGE_H__ */
