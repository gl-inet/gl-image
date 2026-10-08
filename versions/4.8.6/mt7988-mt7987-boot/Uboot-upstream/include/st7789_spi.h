#ifndef __ST7789_SPI_H__
#define __ST7789_SPI_H__

/*
 * Legacy fallback when no "gl,st7789" device tree node is present
 * (GL-BE10000-style wiring: SPI0 + these GPIOs).
 */
#define GPIO_LEDK			12
#define GPIO_RESET			10
#define GPID_DC				22
#define GPIO_DC_CMD                     0
#define GPIO_DC_DAT                     1
/*
 * Reset key: gpio-keys /reset and gl,st7789 reset-button-gpios must use the same
 * pin and GPIO_ACTIVE_* (U-Boot reads via BUTTON uclass "reset"). GL-BE10000: pio 4
 * active-low; GL-BE14000 eMMC: pio 13 active-low. Fallback below matches BE10000 only.
 */
#define GPIO_RESET_BTN_FALLBACK		4
#define GL_RESET_BUTTON_IS_PRESS_RAW	0

void st7789v_map_display(unsigned char *map);
int st7789v_init_display(void);
void check_button_is_press(void);

#endif /* __ST7789_SPI_H__ */

