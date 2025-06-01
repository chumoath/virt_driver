#include <linux/init.h>
#include <linux/i2c.h>

#define FAN_FAIL_MAX_RETRY 50

#define FAN_CMD_SET_PWM 0x180010
#define FAN_CMD_GET_PWM 0x180011
#define FAN_CMD_GET_RPM 0x18000D

#define FAN_REG_SEND     0x20
#define FAN_REG_RECV     0x21
#define FAN_REG_SET_PWM  0x22

#define FAN_SEND_COMMAND_SIZE              7
#define FAN_SEND_PEC_SIZE                  7
#define FAN_RECV_CONTROL_AND_PEC_SIZE      3
#define FAN_RECV_PEC_EXCEPT_CONTENT_SIZE   6
#define FAN_RECV_CONTROL_NUM_OFFSET        3
#define FAN_RECV_CONTENT_OFFSET            5

#define BUFFER_SIZE                        64
#define FAN_MAX_NUM                        16
#define FAN_ITEM_NUM                       3

struct fan_data;

struct pwm_work_struct {
    struct fan_data *data;
    struct work_struct _work;
};

struct fan_data {
    struct i2c_client *client;
    struct timer_list pwm_timer;
    struct workqueue_struct *pwm_workqueue;
    struct mutex mcu_lock;
    struct mutex pwm_buf_lock;
    u8 pwm[FAN_MAX_NUM];
    u8 pwm_enabled[FAN_MAX_NUM];
    struct attribute_group attr_group;
    const struct attribute_group *groups[2];
    struct attribute **attrs;
};

int _fan_read(struct fan_data *data, u32 command, int nr, char *rbuf, int rlen);
int _fan_write(struct fan_data *data, u32 command, int nr, const char *wbuf, int wlen);