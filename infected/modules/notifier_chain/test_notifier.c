#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

BLOCKING_NOTIFIER_HEAD(test_notifier_list);

enum {
    TEST_REBOOT,
    TEST_POWEROFF,
    TEST_HALT,
};

int test_reboot_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
    if (action == TEST_REBOOT) {
        printk("%s called\n", __func__);
    }
    return 0;
}

int test_poweroff_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
    if (action == TEST_POWEROFF) {
        printk("%s called\n", __func__);
    }
    return 0;
}

int test_halt_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
    if (action == TEST_HALT) {
        printk("%s called\n", __func__);
    }
    return 0;
}

static struct notifier_block test_reboot_nb = {
    .notifier_call = test_reboot_notifier,
};

static struct notifier_block test_poweroff_nb = {
    .notifier_call = test_poweroff_notifier,
};

static struct notifier_block test_halt_nb = {
    .notifier_call = test_halt_notifier,
};

static int __init test_enter(void)
{
    blocking_notifier_chain_register(&test_notifier_list, &test_reboot_nb);
    blocking_notifier_chain_register(&test_notifier_list, &test_poweroff_nb);
    blocking_notifier_chain_register(&test_notifier_list, &test_halt_nb);

    blocking_notifier_call_chain(&test_notifier_list, TEST_REBOOT, NULL);
    return 0;
}

static void __exit test_exit(void)
{

}

module_init(test_enter)
module_exit(test_exit)
// 必须要有，否则会提示 Unknown symbol blocking_notifier_chain_register
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chumoath");