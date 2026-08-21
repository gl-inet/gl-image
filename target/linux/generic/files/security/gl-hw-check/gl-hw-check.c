#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/workqueue.h>

static struct {
    unsigned long start_time;
    struct delayed_work check_work;
} gl_wait_data;

static bool is_hw_info_module_loaded(void)
{
    struct file *filp = filp_open("/proc/gl-hw-info", O_RDONLY, 0);
    if (IS_ERR(filp)) {
        return false;
    }
    filp_close(filp, NULL);
    return true;
}

static int create_hw_check_script(void)
{
    const char *content =
        "#!/bin/sh\n"
        "simple=$1\n"
        "vaild_mac() {\n"
        "    local mac=\"$1\"\n"
        "    [ \"${#mac}\" -eq 17 ] || exit 1\n"
        "    [ \"$(echo \"$mac\" | tr -d -c ':' | wc -c)\" -eq 5 ] || exit 1\n"
        "    for i in $(seq 6); do\n"
        "        byte=$(echo \"$mac\" | cut -d: -f$i)\n"
        "        [ \"${#byte}\" -eq 2 ] || exit 1\n"
        "        [ -z \"$(echo $byte | tr -d '0-9-a-f')\" ] || exit 1\n"
        "    done\n"
        "}\n"
        "[ -f /proc/gl-hw-info/device_mac ] || exit 1\n"
        "[ -f /proc/gl-hw-info/device_ddns ] || exit 1\n"
        "mac1=$(cat /proc/gl-hw-info/device_mac)\n"
        "vaild_mac \"$mac1\"\n"
        "ddns=$(cat /proc/gl-hw-info/device_ddns)\n"
        "mac1=$(echo \"$mac1\" | tr -d ':')\n"
        "[ \"${mac1:7}\" = \"${ddns:2}\" ] || exit 1\n"
        "[ $simple -eq 1 ] && exit 0\n"
        "[ -f /proc/gl-hw-info/device_cert ] || exit 1\n"
        "mac2=$(openssl x509 -in /proc/gl-hw-info/device_cert  -noout -subject 2>/dev/null | cut -d, -f5 | cut -d' ' -f4)\n"
        "[ \"$mac1\" = \"$mac2\" ]\n";
    struct file *filp;
    loff_t pos = 0;
    int ret;

    filp = filp_open("/tmp/gl-hw-check", O_CREAT | O_WRONLY | O_TRUNC, 0744);
    if (IS_ERR(filp))
        return -PTR_ERR(filp);

    ret = kernel_write(filp, content, strlen(content), &pos);
    if (ret < 0) {
        filp_close(filp, NULL);
        return ret;
    }

    filp_close(filp, NULL);
    return 0;
}

static int run_cmd(const char *cmd)
{
	char **argv;
	static char *envp[] = {
		"HOME=/",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
		NULL
	};
	int ret;
	argv = argv_split(GFP_KERNEL, cmd, NULL);
	if (argv) {
		ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
		argv_free(argv);
	} else {
		ret = -ENOMEM;
	}

	return ret;
}

static void timer_expires(struct timer_list *t)
{
}

static void do_panic(void)
{
    struct timer_list timer;
    timer_setup(&timer, timer_expires, 0);
    mod_timer(&timer, jiffies +  60 * 60 * HZ);
}

static void gl_hw_check_work_handler(struct work_struct *work)
{
    int ret = 0;

    if (!is_hw_info_module_loaded()) {
        if (time_after(jiffies, gl_wait_data.start_time + 60 * HZ)) {
            do_panic();
            return;
        }

        schedule_delayed_work(&gl_wait_data.check_work, 5 * HZ);
        return;
    }

    msleep(1000);

    ret = create_hw_check_script();
    if (ret < 0)
        goto done;

    ret = run_cmd("/bin/sh /tmp/gl-hw-check 1");

done:
    run_cmd("/bin/rm -f /tmp/gl-hw-check");

    if (ret)
        do_panic();
}

static int __init gl_xx_init(void)
{
    INIT_DELAYED_WORK(&gl_wait_data.check_work, gl_hw_check_work_handler);
    gl_wait_data.start_time = jiffies;

    schedule_delayed_work(&gl_wait_data.check_work, 10 * HZ);

    return 0;
}

static void __exit gl_xx_exit(void)
{
    cancel_delayed_work_sync(&gl_wait_data.check_work);
}

module_init(gl_xx_init);
module_exit(gl_xx_exit);

MODULE_AUTHOR("jianhui zhao <jianhui.zhao@gl-inet.com>");
MODULE_LICENSE("GPL");
