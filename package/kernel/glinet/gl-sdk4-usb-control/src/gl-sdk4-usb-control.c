/*
 * GL-usb-control - Control the enabling and disabling of USB.
 *
 * Copyright (C) 2026 Chengyang.li <chengyang.li@gl-inet.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define GL_USB_CONTROL_DRV_NAME "gl-usb-control_v1.0"
#define MOD_NAME "usb-control: "
#define DEFAULT_SYSFS_PATH "/sys/class/gpio/usb_power/value"
#define MAX_PATH_LEN 256

struct usb_control_data {
    struct device *dev;
    struct gpio_desc *control_gpio;
    int irq;
    int power_default;
    struct work_struct gpio_work;
    int gpio_value;
    char usb_power_path[MAX_PATH_LEN];
};


static void write_gpio_value_work(struct work_struct *work)
{
    struct usb_control_data *data = container_of(work, struct usb_control_data, gpio_work);
    struct file *fp;
    mm_segment_t old_fs;
    char buffer[4];
    int ret;

    snprintf(buffer, sizeof(buffer), "%d\n", data->gpio_value);

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fp = filp_open(data->usb_power_path, O_WRONLY, 0);
    if (IS_ERR(fp)) {
        pr_err(MOD_NAME "Failed to open sysfs file %s\n", data->usb_power_path);
        set_fs(old_fs);
        return;
    }

    ret = kernel_write(fp, buffer, strlen(buffer), &fp->f_pos);
    if (ret != strlen(buffer)) {
        pr_err(MOD_NAME "Failed to set usb_power GPIO, ret=%d\n", ret);
    } else {
        pr_info(MOD_NAME "set usb_power GPIO to %d\n", data->gpio_value);
    }

    filp_close(fp, NULL);

    set_fs(old_fs);
}


static irqreturn_t usb_control_irq_handler(int irq, void *dev_id)
{
    struct usb_control_data *data = dev_id;
    int control_state;

    control_state = gpiod_get_value(data->control_gpio);

    if (control_state) {
        data->gpio_value = 1;
        schedule_work(&data->gpio_work);
    } else {
        data->gpio_value = 0;
        schedule_work(&data->gpio_work);
    }

    return IRQ_HANDLED;
}

static int write_gpio_sysfs(struct usb_control_data *data, int value)
{
    struct file *fp;
    mm_segment_t old_fs;
    char buffer[4];
    int ret;

    snprintf(buffer, sizeof(buffer), "%d\n", value);

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fp = filp_open(data->usb_power_path, O_WRONLY, 0);
    if (IS_ERR(fp)) {
        pr_err(MOD_NAME "Failed to open sysfs file %s\n", data->usb_power_path);
        set_fs(old_fs);
        return PTR_ERR(fp);
    }

    ret = kernel_write(fp, buffer, strlen(buffer), &fp->f_pos);
    if (ret != strlen(buffer)) {
        pr_err(MOD_NAME "Failed to write %d to sysfs, ret=%d\n", value, ret);
        filp_close(fp, NULL);
        set_fs(old_fs);
        return ret;
    }

    filp_close(fp, NULL);
    set_fs(old_fs);

    return 0;
}

static int check_sysfs_file_exists(const char *path)
{
    struct file *fp;
    mm_segment_t old_fs;
    int ret = 0;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fp = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        ret = -ENOENT;
    } else {
        filp_close(fp, NULL);
        ret = 0;
    }

    set_fs(old_fs);
    return ret;
}

static int usb_control_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *node = dev->of_node;
    struct usb_control_data *data;
    const char *sysfs_path;
    int ret;

    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->dev = dev;

    ret = of_property_read_string(node, "usb-power-path", &sysfs_path);
    if (ret) {
        strncpy(data->usb_power_path, DEFAULT_SYSFS_PATH, MAX_PATH_LEN - 1);
        data->usb_power_path[MAX_PATH_LEN - 1] = '\0';
    } else {
        strncpy(data->usb_power_path, sysfs_path, MAX_PATH_LEN - 1);
        data->usb_power_path[MAX_PATH_LEN - 1] = '\0';
        //pr_info(MOD_NAME "Using sysfs path from DTS: %s\n", data->usb_power_path);
    }

    ret = check_sysfs_file_exists(data->usb_power_path);
    if (ret) {
        pr_err(MOD_NAME "sysfs file %s does not exist or cannot be accessed. Ensure gpio-export driver is loaded.\n", data->usb_power_path);
        return -ENODEV;
    }

    INIT_WORK(&data->gpio_work, write_gpio_value_work);

    data->control_gpio = devm_gpiod_get_index(dev, "control", 0, GPIOD_IN);
    if (IS_ERR(data->control_gpio)) {
        pr_err(MOD_NAME "Failed to get control GPIO\n");
        return PTR_ERR(data->control_gpio);
    }

    /*
    ret = write_gpio_sysfs(data, 0);
    if (ret) {
        pr_err(MOD_NAME "Failed to initialize GPIO to 0 via sysfs\n");
        return ret;
    } else {
        pr_info(MOD_NAME "Initialized usb power GPIO to 0\n");
    }
    */

    ret = of_property_read_u32(node, "power-gpio-default-output", &data->power_default);
    if (ret)
        data->power_default = 1;

    data->irq = gpiod_to_irq(data->control_gpio);
    if (data->irq < 0) {
        pr_err(MOD_NAME "Failed to get IRQ number\n");
        return data->irq;
    }

    ret = devm_request_irq(dev, data->irq, usb_control_irq_handler,
                          IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                          "usb-control", data);
    if (ret) {
        pr_err(MOD_NAME "Failed to request IRQ\n");
        return ret;
    }

    platform_set_drvdata(pdev, data);

    pr_info(MOD_NAME "USB control driver loaded successfully\n");

    return 0;
}

static int usb_control_remove(struct platform_device *pdev)
{
    struct usb_control_data *data = platform_get_drvdata(pdev);
    int ret;

    cancel_work_sync(&data->gpio_work);

    ret = write_gpio_sysfs(data, data->power_default);
    if (ret) {
        pr_err(MOD_NAME "Failed to set GPIO to default via sysfs\n");
    } else {
        pr_info(MOD_NAME "usb_power GPIO set to default: %d\n", data->power_default);
    }

    return 0;
}

/*
static void usb_control_shutdown(struct platform_device *pdev)
{
    struct usb_control_data *data = platform_get_drvdata(pdev);
    int ret;

    ret = write_gpio_sysfs(data, data->power_default);
    if (ret) {
        pr_err(MOD_NAME "Shutdown: Failed to set GPIO to default\n");
    } else {
        pr_info(MOD_NAME "Shutdown: Power GPIO set to default: %d\n", data->power_default);
    }
}
*/

static const struct of_device_id usb_control_of_match[] = {
    { .compatible = "usb-control" },
    { },
};

MODULE_DEVICE_TABLE(of, usb_control_of_match);

static struct platform_driver usb_control_driver = {
    .driver = {
        .name = GL_USB_CONTROL_DRV_NAME,
        .of_match_table = usb_control_of_match,
    },
    .probe = usb_control_probe,
    .remove = usb_control_remove,

};

module_platform_driver(usb_control_driver);

MODULE_AUTHOR("Chengyang.li <chengyang.li@gl-inet.com>");
MODULE_DESCRIPTION("GL.iNet USB Control Driver");
MODULE_LICENSE("GPL");
