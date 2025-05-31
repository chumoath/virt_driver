#include <linux/module.h>
#include <linux/init.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/i2c.h>

struct temp_data;

struct temp_data {
    struct i2c_client *client;
    struct device *hwmon_dev;
    struct mutex temp_lock;
    const struct attribute_group *group;
    unsigned long temp;
};