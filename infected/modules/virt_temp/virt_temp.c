#include "virt_temp.h"

static ssize_t show_temp(struct device *dev, struct device_attribute *devattr, char *buf)
{
    struct temp_data *data = dev_get_drvdata(dev);
    unsigned long val;
    mutex_lock(&data->temp_lock);
    val = data->temp;
    mutex_unlock(&data->temp_lock);
    return sprintf(buf, "%ld\n", val);
}

static ssize_t set_temp(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
    struct temp_data *data = dev_get_drvdata(dev);
    unsigned long val;
    int err;

    err = kstrtoul(buf, 10, &val);
    if (err) return err;
    val = clamp_val(val, 0, 100000);

    mutex_lock(&data->temp_lock);
    data->temp = val;
    mutex_unlock(&data->temp_lock);

    return count;
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IWUSR | S_IRUGO, show_temp, set_temp, 1);

static struct attribute *temp_hwmon_attrs[] = {
    &sensor_dev_attr_temp1_input.dev_attr.attr,
    NULL
};

ATTRIBUTE_GROUPS(temp_hwmon);

//static int temp_probe(struct i2c_client *client, const struct i2c_device_id *id)
static int temp_probe(struct i2c_client *client)
{
    struct device *dev = &client->dev;
    struct temp_data *data;

    data = devm_kzalloc(dev, sizeof(struct temp_data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    printk ("%s: temp_probe is invoked\n", __func__);
    printk ("%s: i2c client name is %s\n", __func__, client->name);

    i2c_set_clientdata(client, data);

    data->client = client;
    data->temp = 25000;
    data->hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name, data, temp_hwmon_groups);

    if (IS_ERR(data->hwmon_dev)) {
        printk ("%s: create hwmon failed\n", __func__);
        return PTR_ERR(data->hwmon_dev);
    }

    return 0;
}

//static int temp_remove(struct i2c_client *client)
static void temp_remove(struct i2c_client *client)
{
}

static const struct i2c_device_id virt_temp_id[] = {
    { "virt_temp", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, virt_temp_id);

static struct i2c_driver temp_driver = {
        .class = I2C_CLASS_HWMON,
        .driver = {
                .name = "virt_temp",
        },
        .probe = temp_probe,
        .remove = temp_remove,
        .id_table = virt_temp_id
};

module_i2c_driver(temp_driver)

MODULE_AUTHOR("chumoath");
MODULE_DESCRIPTION("virt temp driver.");
MODULE_LICENSE("GPL");