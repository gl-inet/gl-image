#include <command.h>
#include <button.h>
#include <linux/delay.h>
#include <dm/ofnode.h>

int led_control(const char *cmd, const char *name, const char *arg)
{
    const char *led = ofnode_conf_read_str(name);
    char buf[128];


    if (!led)
        return -1;

    if(strncmp(cmd, "ledblink", 8) == 0) {
        sprintf(buf, "%s %s %s %s", "led", led, "blink", arg);
    } else {
        sprintf(buf, "%s %s %s", cmd, led, arg);
    }
    run_command(buf, 0);

    return 0;
}

static void gpio_power_clr(void)
{
    ofnode node = ofnode_path("/config");
    char cmd[128];
    const u32 *val;
    int size, i;

    if (!ofnode_valid(node))
        return;

    val = ofnode_read_prop(node, "gpio_power_clr", &size);
    if (!val)
        return;

    for (i = 0; i < size / 4; i++) {
        sprintf(cmd, "gpio clear %u", fdt32_to_cpu(val[i]));
        run_command(cmd, 0);
    }
}

static void gpio_pull_up(void)
{
    ofnode node = ofnode_path("/config");
    char cmd[128];
    const u32 *val;
    int size, i;

    if (!ofnode_valid(node))
        return;

    val = ofnode_read_prop(node, "gpio_pull_up", &size);
    if (!val)
        return;

    for (i = 0; i < size / 4; i++) {
        sprintf(cmd, "gpio set %u", fdt32_to_cpu(val[i]));
        run_command(cmd, 0);
    }
}

static void led_action_post(void *arg)
{
    led_control("ledblink", "blink_led", "0");
    led_control("led", "blink_led", "on");
}

static int do_glbtn(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    const char *button_label = "reset";
    int ret, counter = 0;
    struct udevice *dev;
    ulong ts;

    gpio_power_clr();
    gpio_pull_up();

    ret = button_get_by_label(button_label, &dev);
    if (ret) {
        printf("Button '%s' not found (err=%d)\n", button_label, ret);
        return CMD_RET_FAILURE;
    }

    if (!button_get_state(dev)) {
        return CMD_RET_SUCCESS;

    }

    printf("RESET button is pressed for: %2d second(s)", counter++);

    led_control("led", "blink_led", "on");
    ts = get_timer(0);

    while (button_get_state(dev) && counter < 6) {
        if (get_timer(ts) < 1000) {
            if (get_timer(ts) == 500) {
                led_control("led", "blink_led", "toggle");
            }
            continue;
        }

        led_control("led", "blink_led", "toggle");
        ts = get_timer(0);

        printf("\b\b\b\b\b\b\b\b\b\b\b\b%2d second(s)", counter++);
    }

    printf("\n");

    led_control("led", "blink_led", "off");

    if (counter == 6) {
        led_control("led", "system_led", "on");
        run_command("httpd", 0);
    } else {
        led_control("led", "blink_led", "on");
    }

    return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
    glbtn, 1, 0, do_glbtn,
    "GL-iNet button check",
    ""
);
