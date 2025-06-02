#include "virt_fan.h"

int _fan_read(struct fan_data *data, u32 command, int nr, u8 *rbuf, int rlen)
{
    int ret;
    char tx_buffer[BUFFER_SIZE];
    struct i2c_client *client = data->client;
    struct i2c_msg msgs[2];
    // addr => 0: write  1: read
    tx_buffer[0] = (client->addr << 1);
    tx_buffer[1] = FAN_REG_SEND;
    tx_buffer[2] = 0x04;
    tx_buffer[3] = (command >> 16) & 0xFF;
    tx_buffer[4] = (command >> 8) & 0xFF;
    tx_buffer[5] = command & 0xFF;
    tx_buffer[6] = nr;

    // calculate pec
    tx_buffer[FAN_SEND_PEC_SIZE] = i2c_smbus_pec(0, (u8 *)tx_buffer, FAN_SEND_PEC_SIZE);

    // send command
    ret = i2c_master_send(client, &tx_buffer[1], FAN_SEND_COMMAND_SIZE);
    if (ret != FAN_SEND_COMMAND_SIZE) {
        printk ("%s: transfer data failed\n", __func__);
        ret = -EIO;
        goto end;
    }

    udelay(100);

    tx_buffer[1] = FAN_REG_RECV;
    tx_buffer[2] = (client->addr << 1) | 1;
    msgs[0].addr = client->addr;
    msgs[0].flags = client->flags;
    msgs[0].len = 1;
    msgs[0].buf = (u8 *)&tx_buffer[1];

    msgs[1].addr = client->addr;
    msgs[1].flags = client->flags | I2C_M_RD;
    msgs[1].len = rlen + FAN_RECV_CONTROL_AND_PEC_SIZE;
    msgs[1].buf = (u8 *)&tx_buffer[FAN_RECV_CONTROL_NUM_OFFSET];

    ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
    if (ret != 2) {
        ret = -EIO;
        printk ("%s: i2c read failed\n", __func__);
        goto end;
    } else {
        ret = 0;
    }

    // verify val len
    if ( tx_buffer[FAN_RECV_CONTROL_NUM_OFFSET] != rlen + 1) {
        ret = -EIO;
        printk ("%s: recv len verify failed\n", __func__);
        goto end;
    }

    // verify pec
    if (i2c_smbus_pec(0, (u8 *)tx_buffer, rlen + FAN_RECV_PEC_EXCEPT_CONTENT_SIZE) != 0) {
        ret = -EIO;
        printk ("%s: recv data verify failed\n", __func__);
        goto end;
    }

    // only copy result; rpm/pwm
    memcpy(rbuf, &tx_buffer[FAN_RECV_CONTENT_OFFSET], rlen);

end:
    return ret;
}

int _fan_write(struct fan_data *data, u32 command, int nr, const u8 *wbuf, int wlen)
{
    int ret;
    char tx_buffer[BUFFER_SIZE];
    struct i2c_client *client = data->client;

    tx_buffer[0] = (client->addr << 1);
    tx_buffer[1] = FAN_REG_SET_PWM;
    tx_buffer[2] = 0x05;
    tx_buffer[3] = (command >> 16) & 0xFF;
    tx_buffer[4] = (command >> 8) & 0xFF;
    tx_buffer[5] = command & 0xFF;
    tx_buffer[6] = nr;

    memcpy(&tx_buffer[FAN_SEND_PEC_SIZE], wbuf, wlen);

    // calculate pec
    tx_buffer[FAN_SEND_PEC_SIZE + wlen] = i2c_smbus_pec(0, (u8 *)tx_buffer, FAN_SEND_PEC_SIZE + wlen);

    // send command
    ret = i2c_master_send(client, &tx_buffer[1], FAN_SEND_COMMAND_SIZE + wlen);
    if (ret != FAN_SEND_COMMAND_SIZE + wlen) {
        printk ("%s: transfer data failed\n", __func__);
        ret = -EIO;
    }

    return ret;
}