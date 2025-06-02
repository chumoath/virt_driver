#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/slab.h>
#include "virt_fan.h"

static int num_fans = 0;
module_param(num_fans, int, 0644);
MODULE_PARM_DESC(num_fans, "Number of fans to create");

int fan_read(struct fan_data *data, u32 command, int nr, u8 *rbuf, int rlen)
{
    int i;
    int ret;

    mutex_lock(&data->mcu_lock);

    for (i = 0; i < FAN_FAIL_MAX_RETRY; i++) {
        ret = _fan_read(data, command, nr, rbuf, rlen);
        if ( ret == 0 ) break;

        udelay(1000);
    }

    mutex_unlock(&data->mcu_lock);

    return ret;
}

int fan_write(struct fan_data *data, u32 command, int nr, const u8 *wbuf, int wlen)
{
    int i;
    int ret;

    mutex_lock(&data->mcu_lock);

    for (i = 0; i < FAN_FAIL_MAX_RETRY; i++) {
        ret = _fan_write(data, command, nr, wbuf, wlen);
        if ( ret == 0 ) break;

        udelay(1000);
    }

    mutex_unlock(&data->mcu_lock);

    return ret;
}

static ssize_t show_fan(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct fan_data *data = dev_get_drvdata(dev);
    int nr = attr->index;
    u16 fan_rpm;
    int ret;

    ret = fan_read(data, FAN_CMD_GET_RPM, nr, (u8 *)&fan_rpm, sizeof(fan_rpm));
    if ( ret < 0 ) {
        printk ("%s: get fan rpm%d failed\n", __func__, nr);
        return ret;
    }
    printk ("%s: get fan_rpm%d = %d\n", __func__, nr, fan_rpm);

    return sprintf(buf, "%d\n", fan_rpm);
}

static ssize_t show_pwm(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct fan_data *data = dev_get_drvdata(dev);
    int nr = attr->index;
    u8 fan_pwm;
    int ret;

    ret = fan_read(data, FAN_CMD_GET_PWM, nr, &fan_pwm, sizeof(fan_pwm));
    if ( ret < 0 ) {
        printk ("%s: get fan pwm%d failed\n", __func__, nr);
        return ret;
    }
    printk ("%s: get fan_pwm%d = %d\n", __func__, nr, fan_pwm);

    return sprintf(buf, "%d\n", fan_pwm);
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct fan_data *data = dev_get_drvdata(dev);
    int nr = attr->index;
    unsigned long val;
    int err;

    err = kstrtoul(buf, 10 , &val);
    if (err) return err;

    val = clamp_val(val, 0, 255);

    mutex_lock(&data->pwm_buf_lock);
    data->pwm[nr-1] = (u8)val;
    mutex_unlock(&data->pwm_buf_lock);

    printk ("%s: set fan_pwm%d = %d\n", __func__, nr, (u8)val);
    return count;
}

static ssize_t show_pwm_enable(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct fan_data *data = dev_get_drvdata(dev);
    int nr = attr->index;

    return sprintf(buf, "%d\n", data->pwm_enabled[nr-1]);
}

static ssize_t set_pwm_enable(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
    struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
    struct fan_data *data = dev_get_drvdata(dev);
    int nr = attr->index;
    unsigned long val;
    int err;

    err = kstrtoul(buf, 10 , &val);
    if (err) return err;

    if (val < 0 || val > 1) return -EINVAL;

    data->pwm_enabled[nr-1] = (int)val;

    return count;
}

static void fan_pwm_init(struct fan_data *data)
{
    int nr;

    mutex_lock(&data->pwm_buf_lock);

    for (nr = 1; nr <= num_fans; nr++) {
        // default: 30%
        data->pwm[nr-1] = 0x4c;
    }

    mutex_unlock(&data->pwm_buf_lock);
}

static void update_pwm(struct fan_data *data, int nr, u8 val)
{
    int ret;

    val = clamp_val(val, 0, 255);
    ret = fan_write(data, FAN_CMD_SET_PWM, nr, &val, sizeof(val));
    if (ret < 0) {
        printk ("%s: update fan pwm%d failed\n", __func__, nr);
    }
}

static void fan_update_pwm_work_callback(struct work_struct *work)
{
    struct pwm_work_struct *p_work = container_of(work, struct pwm_work_struct, _work);
    struct fan_data *data = p_work->data;
    int nr;

    mutex_lock(&data->pwm_buf_lock);
    for (nr = 1; nr <= num_fans; nr++) {
        update_pwm(data, nr, data->pwm[nr-1]);
    }
    mutex_unlock(&data->pwm_buf_lock);

    kfree(p_work);
}

static void fan_update_pwm_timer_callback(struct timer_list *t)
{
    // sleeping function called from invalid context at _mightsleep
    // can not invoke mutex_lock or sleep inside the timer handler

    struct fan_data *data = from_timer(data, t, pwm_timer);
    struct pwm_work_struct *p_work = (struct pwm_work_struct *) kmalloc(sizeof(struct pwm_work_struct), GFP_ATOMIC);
    p_work->data = data;

    INIT_WORK(&p_work->_work, fan_update_pwm_work_callback);

    queue_work(data->pwm_workqueue, &(p_work->_work));

    mod_timer(&data->pwm_timer, jiffies + msecs_to_jiffies(1000));
}

static int fan_init(struct fan_data *data)
{
    mutex_init(&data->mcu_lock);
    mutex_init(&data->pwm_buf_lock);
    data->pwm_workqueue = create_workqueue(data->client->name);
    if (data->pwm_workqueue == NULL) {
        printk ("%s: cannot create workqueue\n", __func__);
        return -ENOMEM;
    }
    printk ("%s: workqueue %s created\n", __func__, data->client->name);
    fan_pwm_init(data);
    timer_setup(&data->pwm_timer, fan_update_pwm_timer_callback, 0);
    mod_timer(&data->pwm_timer, jiffies + msecs_to_jiffies(1000));
    printk ("%s: fan timer created\n", __func__);
    return 0;
}

#define FAN_ADD_SENSOR_ATTR(_data, _dev, _format_name, _show, _store, _mode, _index, _slot) \
    do { \
        a = devm_kzalloc(_dev, sizeof(*a), GFP_KERNEL); \
        if (a == NULL) return -ENOMEM; \
        sysfs_attr_init(&a->dev_attr.attr); \
        a->dev_attr.attr.name = devm_kasprintf(_dev, GFP_KERNEL, _format_name, _index); \
        if (a->dev_attr.attr.name == NULL) return -ENOMEM; \
        a->dev_attr.show = _show; \
        a->dev_attr.store = _store; \
        a->dev_attr.attr.mode = _mode; \
        a->index = _index; \
        data->attrs[_slot] = &a->dev_attr.attr; \
    } while (0)

/*
 * Usage:
 * insmod virt_fan_drv.ko num_fans=6
 * echo virt_fan 0x30 > /sys/bus/i2c/devices/i2c-1/new_device
 * cat /sys/module/virt_fan_drv/parameters/num_fans
 * echo 6 > /sys/module/virt_fan_drv/parameters/num_fans
*/

//static int fan_probe(struct i2c_client *client, const struct i2c_device_id *id)
static int fan_probe(struct i2c_client *client)
{
    struct device *dev = &client->dev;
    struct fan_data *data;
    struct device *hwmon_dev;
    struct sensor_device_attribute *a;
    int i;

    if (num_fans <= 0 || num_fans > FAN_MAX_NUM) {
        printk ("%s: num_fans is invalid\n", __func__);
        return -EINVAL;
    }

    // no need to free manually
    data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
    if (data == NULL) {
        printk ("%s-%d: devm_kzalloc failed\n", __func__, __LINE__);
        return -ENOMEM;
    }
    printk ("%s: fan_probe is invoked\n", __func__);
    printk ("%s: i2c client name is %s\n", __func__, client->name);

    data->attrs = devm_kcalloc(dev, (num_fans * FAN_ITEM_NUM) + 1, sizeof(*data->attrs), GFP_KERNEL);
    if (data->attrs == NULL) {
        printk ("%s-%d: devm_kcalloc failed\n", __func__, __LINE__);
        return -ENOMEM;
    }

    for (i = 0; i < num_fans; i++) {
        FAN_ADD_SENSOR_ATTR(data, dev, "fan%d_input", show_fan, NULL, 0444, i+1, i * FAN_ITEM_NUM);
        FAN_ADD_SENSOR_ATTR(data, dev, "pwm%d", show_pwm, set_pwm, 0644, i+1, i * FAN_ITEM_NUM + 1);
        FAN_ADD_SENSOR_ATTR(data, dev, "pwm%d_enable", show_pwm_enable, set_pwm_enable, 0644, i+1, i * FAN_ITEM_NUM + 2);
    }

    data->attr_group.attrs = data->attrs;
    data->groups[0] = &data->attr_group;
    data->client = client;
    i2c_set_clientdata(client, data);

    hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name, data, data->groups);
    if (IS_ERR(hwmon_dev)) {
        printk ("%s: create hwmon failed\n", __func__);
        return PTR_ERR(hwmon_dev);
    }
    printk ("%s: create hwmon successfully\n", __func__);
    return fan_init(data);
}

static void fan_remove(struct i2c_client *client)
{
    struct fan_data *data = i2c_get_clientdata(client);
    // make sure timer callback done
    del_timer_sync(&data->pwm_timer);
    flush_workqueue(data->pwm_workqueue);
    destroy_workqueue(data->pwm_workqueue);
}

static const struct i2c_device_id virt_fan_id[] = {
    { "virt_fan", 0 },
    {}
};

static struct i2c_driver fan_driver = {
    .class = I2C_CLASS_HWMON,
    .driver = {
        .name = "virt_fan",
    },
    .probe = fan_probe,
    .remove = fan_remove,
    .id_table = virt_fan_id
};

module_i2c_driver(fan_driver)

MODULE_AUTHOR("chumoath");
MODULE_DESCRIPTION("virt fan driver");
MODULE_LICENSE("GPL");