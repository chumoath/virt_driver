#include "virt_fan.h"

#define FAN_MAX_RPM  5000

int _fan_read(struct fan_data *data, u32 command, int nr, u8 *rbuf, int rlen)
{
    switch (command) {
        case FAN_CMD_GET_PWM:
            mutex_lock(&data->pwm_buf_lock);
            *rbuf = data->pwm[nr-1];
            mutex_unlock(&data->pwm_buf_lock);
            break;
        case FAN_CMD_GET_RPM:
            mutex_lock(&data->pwm_buf_lock);
            *(u16 *)rbuf = (unsigned long)data->pwm[nr - 1] * FAN_MAX_RPM / 255;
            mutex_unlock(&data->pwm_buf_lock);
            break;
        default:
            break;
    }
    return 0;
}

int _fan_write(struct fan_data *data, u32 command, int nr, const u8 *wbuf, int wlen)
{
    return 0;
}