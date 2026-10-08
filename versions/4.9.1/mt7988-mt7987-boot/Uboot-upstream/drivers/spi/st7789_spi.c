#include <common.h>
#include <button.h>
#include <command.h>
#include <linux/delay.h>
#include <dm/device.h>
#include <dm/uclass-internal.h>
#include <dm/uclass.h>
#include <spi.h>
#include <spi-mem.h>
#include <dm.h>
#include <malloc.h>
#include <st7789_spi.h>
#include <dm/ofnode.h>
#include <asm/gpio.h>
#include <u-boot/lz4.h>
#include <be10000_uboot_image.h>
#include <be14000_uboot_image.h>

static struct st7789_lz4_blob *st7789_map_proc;
static struct st7789_lz4_blob *st7789_map_booting;
static unsigned char st7789_img_buf[153600];

static void st7789_select_image_data(void)
{
#ifdef CONFIG_DEFAULT_DEVICE_TREE
    if (!strcmp(CONFIG_DEFAULT_DEVICE_TREE, "gl-be14000-emmc") ||
        !strcmp(CONFIG_DEFAULT_DEVICE_TREE, "gl-ba7200be-spim-nand")) {
        st7789_map_proc = be14000_map_proc;
        st7789_map_booting = &be14000_map_booting;
        return;
    }
#endif
    /* Default image set: GL-BE10000 */
    st7789_map_proc = be10000_map_proc;
    st7789_map_booting = &be10000_map_booting;
}

static const unsigned char *st7789_get_image_data(struct st7789_lz4_blob *blob)
{
    size_t out_len = sizeof(st7789_img_buf);
    size_t in_len;
    int ret;

    if (!blob || !blob->data || !blob->end || blob->end <= blob->data)
        return NULL;

    in_len = blob->end - blob->data;

    ret = ulz4fn(blob->data, in_len, st7789_img_buf, &out_len);
    if (ret || out_len != sizeof(st7789_img_buf)) {
        printf("ST7789V: LZ4 decode failed ret=%d out=%lu in=%lu\n",
               ret, (unsigned long)out_len, (unsigned long)in_len);
        return NULL;
    }

    return st7789_img_buf;
}

struct spi_slave *st7789v_spi;

static struct gpio_desc st7789_dc;
static struct gpio_desc st7789_rst;
static struct gpio_desc st7789_bl;
static struct udevice *st7789_rst_btn_dev;
static struct gpio_desc st7789_rst_btn;
static bool st7789_rst_btn_use_button;
static bool st7789_rst_btn_use_gpio_dt;
static bool st7789_rst_btn_setup_done;
static bool st7789_use_dt;
static unsigned int st7789_bus;
static unsigned int st7789_cs;
static u32 st7789_hz = 52000000;
static char st7789_spi_name[32] = "generic_0:0";

static void st7789_dc_set(int val)
{
    if (st7789_use_dt)
        dm_gpio_set_value(&st7789_dc, val);
    else
        gpio_set_value(GPID_DC, val);
}

static void st7789_free_dt_gpios(void)
{
    if (!st7789_use_dt)
        return;
    dm_gpio_free(NULL, &st7789_bl);
    dm_gpio_free(NULL, &st7789_rst);
    dm_gpio_free(NULL, &st7789_dc);
    st7789_use_dt = false;
}

static int st7789_setup_from_dt(void)
{
    ofnode panel = ofnode_by_compatible(ofnode_null(), "gl,st7789");
    struct udevice *bus;
    u32 reg = 0;
    u32 hz = 52000000;
    int ret;

    if (!ofnode_valid(panel))
        return -ENODEV;

    ofnode bus_node = ofnode_get_parent(panel);

    if (!ofnode_valid(bus_node))
        return -EINVAL;

    ret = device_get_global_by_ofnode(bus_node, &bus);
    if (ret)
        return ret;

    ret = ofnode_read_u32(panel, "reg", &reg);
    if (ret)
        reg = 0;

    hz = ofnode_read_u32_default(panel, "spi-max-frequency", 52000000);

    st7789_bus = dev_seq(bus);
    st7789_cs = reg;
    st7789_hz = hz;
    snprintf(st7789_spi_name, sizeof(st7789_spi_name), "generic_%u:%u",
         st7789_bus, st7789_cs);

    ret = gpio_request_by_name_nodev(panel, "dc-gpios", 0, &st7789_dc,
                     GPIOD_IS_OUT);
    if (ret)
        return ret;
    ret = gpio_request_by_name_nodev(panel, "reset-gpios", 0, &st7789_rst,
                       GPIOD_IS_OUT);
    if (ret)
        goto err_dc;
    ret = gpio_request_by_name_nodev(panel, "backlight-gpios", 0,
                     &st7789_bl, GPIOD_IS_OUT);
    if (ret)
        goto err_rst;

    st7789_use_dt = true;
    return 0;
err_rst:
    dm_gpio_free(NULL, &st7789_rst);
err_dc:
    dm_gpio_free(NULL, &st7789_dc);
    return ret;
}

static void st7789_setup_reset_button_once(void)
{
    ofnode panel;
    int ret;

    if (st7789_rst_btn_setup_done)
        return;

    /*
     * gpio-keys already claims the reset GPIO via button_gpio.
     * Second gpio_request on the same line fails; use BUTTON uclass.
     */
#if IS_ENABLED(CONFIG_BUTTON)
    ret = button_get_by_label("reset", &st7789_rst_btn_dev);
    if (!ret) {
        st7789_rst_btn_use_button = true;
        st7789_rst_btn_setup_done = true;
        return;
    }
#endif

    panel = ofnode_by_compatible(ofnode_null(), "gl,st7789");
    if (ofnode_valid(panel)) {
        ret = gpio_request_by_name_nodev(panel, "reset-button-gpios", 0,
                         &st7789_rst_btn, GPIOD_IS_IN);
        if (!ret) {
            st7789_rst_btn_use_gpio_dt = true;
            st7789_rst_btn_setup_done = true;
            return;
        }
    }

    st7789_rst_btn_setup_done = true;
}

static bool st7789_reset_button_pressed(void)
{
    int v;

    if (st7789_rst_btn_use_button) {
        v = button_get_state(st7789_rst_btn_dev);

        if (v < 0)
            return false;
        return v == BUTTON_ON;
    }

    if (st7789_rst_btn_use_gpio_dt)
        return dm_gpio_get_value(&st7789_rst_btn) == 1;

    v = gpio_get_value(GPIO_RESET_BTN_FALLBACK);
    return v == GL_RESET_BUTTON_IS_PRESS_RAW;
}

static bool st7789_reset_button_stable_pressed(void)
{
    if (!st7789_reset_button_pressed())
        return false;
    mdelay(10);
    return st7789_reset_button_pressed();
}

/**
 * 安全的 spi-mem 执行函数
 */
static int st7789v_spi_exec_op(struct spi_slave *slave, struct spi_mem_op *op)
{
    int ret;

    if (!slave) {
        printf("ST7789V: SPI device not initialized\n");
        return -ENODEV;
    }

    ret = spi_mem_exec_op(slave, op);
    if (ret) {
        printf("ST7789V: SPI OP failed: cmd=0x%02x, ret=%d\n", 
               op->cmd.opcode, ret);
    }

    return ret;
}

static int st7789v_spi_write_cmd_u8(struct spi_slave *slave, u8 cmd)
{
    struct spi_mem_op op;
    int ret;

    op = (struct spi_mem_op) {
        .cmd = {
            .opcode = cmd,
            .buswidth = 1,
            .nbytes = 1,
        },
    };

    st7789_dc_set(0);
    ret = st7789v_spi_exec_op(slave, &op);

    return ret;
}

static int st7789v_spi_write_data_u8(struct spi_slave *slave, u8 data)
{
    struct spi_mem_op op;

    /* DC 已经在命令写完后设置为数据模式 */
    op = (struct spi_mem_op) {
        .cmd = {
            .opcode = data,  /* 数据模式不使用命令字节 */
            .buswidth = 1,
            .nbytes = 1,
        },
    };

    st7789_dc_set(1);

    return st7789v_spi_exec_op(slave, &op);
}

static int st7789v_spi_write_data(struct spi_slave *slave, const u8 *data, int len)
{
    struct spi_mem_op op;
    int ret;

    if (!data || len <= 0)
        return -EINVAL;

    /* DC 保持数据模式 */
    op = (struct spi_mem_op) {
        .cmd = {
            .opcode = data[0],
            .buswidth = 1,
            .nbytes = 1,
        },
        .data = {
            .dir = SPI_MEM_DATA_OUT,
            .buswidth = 1,
            .nbytes = len-1,
            .buf.out = data+1,
        },
    };

    st7789_dc_set(1);
    ret = st7789v_spi_exec_op(slave, &op);
    return ret;
}

static void st7789v_spi_write_u8_array(struct spi_slave *slave, const u8 *buff, int size)
{
    int i;

    if (!buff || size <= 0)
        return;

    st7789v_spi_write_cmd_u8(slave, buff[0]);

    for (i = 1; i < size; i++) {
        if (buff[i] == 0xff) {
            break;
        }
        st7789v_spi_write_data_u8(slave, buff[i]);
    }
}

void st7789v_map_display(unsigned char *map)
{
    if (!map)
        return;

    st7789v_spi_write_cmd_u8(st7789v_spi, 0x2a);
    st7789v_spi_write_data_u8(st7789v_spi, 0x00);
    st7789v_spi_write_data_u8(st7789v_spi, 0x00);
    st7789v_spi_write_data_u8(st7789v_spi, 0x00);
    st7789v_spi_write_data_u8(st7789v_spi, 0xef);

    st7789v_spi_write_cmd_u8(st7789v_spi, 0x2b);
    st7789v_spi_write_data_u8(st7789v_spi, 0x00);
    st7789v_spi_write_data_u8(st7789v_spi, 0x00);
    st7789v_spi_write_data_u8(st7789v_spi, 0x01);
    st7789v_spi_write_data_u8(st7789v_spi, 0x3f);

    /* 写入显示数据 */
    st7789v_spi_write_cmd_u8(st7789v_spi, 0x2c);
    st7789v_spi_write_data(st7789v_spi, map, 65535);
    st7789v_spi_write_data(st7789v_spi, map + 65535, 65535);
    st7789v_spi_write_data(st7789v_spi, map + 131070, 22530);
//    st7789v_spi_write_data(st7789v_spi, map, 153600);
}

int st7789v_init_display(void)
{
    int i, ret;
    struct udevice *dev;

    if (!st7789_map_proc || !st7789_map_booting)
        st7789_select_image_data();
    static unsigned char init_display[][16] = {
    {0x36, 0x00, 0xff},
    {0x3a, 0x05, 0xff},
    {0xb2, 0x05, 0x05, 0x00, 0x33, 0x33, 0xff},
    {0xb7, 0x35, 0xff},
    {0xbb, 0x21, 0xff},
    {0xc0, 0x2c, 0xff},
    {0xc2, 0x01, 0xff},
    {0xc3, 0x0b, 0xff},
    {0xc4, 0x20, 0xff},
    {0xc6, 0x0a, 0xff},
    {0xd0, 0xa7, 0xa1, 0xff},
    {0xd0, 0xa4, 0xa1, 0xff},
    {0x35, 0x00, 0xff},
    {0xd6, 0xa1, 0xff},
    {0xe0, 0xd0, 0x04, 0x08, 0x0a, 0x09, 0x05, 0x2d, 0x43, 0x49, 0x09, 0x16, 0x15, 0x26, 0x2b, 0xff},
    {0xe1, 0xd0, 0x03, 0x09, 0x0a, 0x0a, 0x06, 0x2e, 0x44, 0x40, 0x3e, 0x15, 0x15, 0x26, 0x2a, 0xff},
    {0x21, 0xff},
    };

    if (st7789_setup_from_dt()) {
        st7789_bus = 0;
        st7789_cs = 0;
        st7789_hz = 52000000;
        strlcpy(st7789_spi_name, "generic_0:0", sizeof(st7789_spi_name));
    }

    ret = _spi_get_bus_and_cs(st7789_bus, st7789_cs, st7789_hz, SPI_MODE_0,
                  "spi_generic_drv", st7789_spi_name, &dev, &st7789v_spi);
    if (!st7789v_spi) {
        debug("%s: Failed to set up slave\n", __func__);
        st7789_free_dt_gpios();
        return -1;
    }

    ret = spi_claim_bus(st7789v_spi);
    if (ret) {
        debug("%s: Failed to claim SPI bus: %d\n", __func__, ret);
        goto err_claim_bus;
    }

    if (st7789_use_dt) {
        dm_gpio_set_value(&st7789_rst, 1);
        st7789_dc_set(1);
        dm_gpio_set_value(&st7789_rst, 0);
        mdelay(10);
        dm_gpio_set_value(&st7789_rst, 1);
        mdelay(10);
    } else {
        gpio_request(GPIO_RESET, "GPIO_RESET");
        gpio_direction_output(GPIO_RESET, 0x1);

        gpio_request(GPID_DC, "GPID_DC");
        gpio_direction_output(GPID_DC, 0x1);

        gpio_set_value(GPIO_RESET, 0);
        mdelay(10);
        gpio_set_value(GPIO_RESET, 1);
        mdelay(10);
    }

    st7789v_spi_write_cmd_u8(st7789v_spi, 0x11);
    mdelay(10);

    for(i=0; i<ARRAY_SIZE(init_display); i++) {
        st7789v_spi_write_u8_array(st7789v_spi, init_display[i], ARRAY_SIZE(init_display[i]));
    }
    mdelay(10);
    st7789v_spi_write_cmd_u8(st7789v_spi, 0x29);
    
    mdelay(50);
    {
        const unsigned char *img = st7789_get_image_data(st7789_map_booting);
        if (img)
            st7789v_map_display((unsigned char *)img);
    }

    mdelay(50);
    if (st7789_use_dt) {
        dm_gpio_set_value(&st7789_bl, 1);
    } else {
        gpio_request(GPIO_LEDK, "GPIO_LEDK");
        gpio_direction_output(GPIO_LEDK, 0x1);
    }
    printf("st7789va init display\n");

    return 0;

err_claim_bus:
    st7789_free_dt_gpios();
    spi_release_bus(st7789v_spi);
    spi_free_slave(st7789v_spi);
    return -1;
}

void check_button_is_press(void)
{
    int counter = 0;

    if (!st7789_map_proc || !st7789_map_booting)
        st7789_select_image_data();

    st7789_setup_reset_button_once();

    if (!st7789_rst_btn_use_button && !st7789_rst_btn_use_gpio_dt) {
        gpio_request(GPIO_RESET_BTN_FALLBACK, "GPIO_RESET_BTN");
        gpio_direction_input(GPIO_RESET_BTN_FALLBACK);
    }

    while (st7789_reset_button_stable_pressed()) {
        if (counter == 0)
            printf("Reset button is pressed for: %2d ", counter);

        if (5 - counter >= 0) {
            const unsigned char *img = st7789_get_image_data(&st7789_map_proc[5 - counter]);
            if (img)
                st7789v_map_display((unsigned char *)img);
        }

        mdelay(1000);
        counter++;

        printf("\b\b\b%2d ", counter);

        if (counter >= 5) {
            {
                const unsigned char *img = st7789_get_image_data(&st7789_map_proc[0]);
                if (img)
                    st7789v_map_display((unsigned char *)img);
            }
            mdelay(1000);
            {
                const unsigned char *img = st7789_get_image_data(&st7789_map_proc[6]);
                if (img)
                    st7789v_map_display((unsigned char *)img);
            }

            run_command("httpd", 0);
            break;
        }
    }

    {
        const unsigned char *img = st7789_get_image_data(st7789_map_booting);
        if (img)
            st7789v_map_display((unsigned char *)img);
    }

    if (counter != 0)
        printf("\n");
}

