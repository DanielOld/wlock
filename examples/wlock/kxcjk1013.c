#include "twi_master.h"
#include "nrf_delay.h"
#include "kxcjk1013.h"

#if 0
void kxcjk1013_read(uint8_t *buf, uint8_t len )
{
    buf[0] = KXCJK1013_REG_CNT_XYZ_ADDR;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 1, TWI_DONT_ISSUE_STOP);
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR | TWI_READ_BIT, buf, len, TWI_ISSUE_STOP);
}

void kxcjk1013_standby_mode(void)
{
    uint8_t buf[2];
    // enter stand by mode
    buf[0] = KXCJK1013_REG_CTRL1_ADDR;
    buf[1] = KXCJK1013_REG_CTRL1_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);
}

void kxcjk1013_operating_mode(void)
{
    uint8_t buf[2];

    //set output data rate (the reset value)
    buf[0] = KXCJK1013_REG_DATA_CTRL_ADDR;
    buf[1] = KXCJK1013_REG_DATA_CTRL_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);


    buf[0] = KXCJK1013_REG_CTRL1_ADDR;
    buf[1] = 0x80 | KXCJK1013_REG_CTRL1_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);
}

void kxcjk1013_reset(void)
{
    uint8_t buf[2];

    buf[0] = KXCJK1013_REG_CTRL2_ADDR;
    buf[1] = 0x80;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);
    nrf_delay_ms(100);
}

#endif

void kxcjk1013_motion_mode(void)
{
    uint8_t buf[2];

    //enable motion detection
    buf[0] = KXCJK1013_REG_CTRL1_ADDR;
    buf[1] = KXCJK1013_REG_CTRL1_VALUE | KXCJK1013_BIT_MOTION_DETECT_ENABLE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

    //set the output data rate for motion detection
    buf[0] = KXCJK1013_REG_CTRL2_ADDR;
    buf[1] = KXCJK1013_REG_CTRL2_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

    //set the interrup pin
    buf[0] = KXCJK1013_REG_INT_CTRL_REG1_ADDR;
    buf[1] = KXCJK1013_REG_INT_CTRL_REG1_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

    //select motion detection axis
    buf[0] = KXCJK1013_REG_INT_CTRL_REG2_ADDR;
    buf[1] = KXCJK1013_REG_INT_CTRL_REG2_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

    //set output data rate
    buf[0] = KXCJK1013_REG_DATA_CTRL_ADDR;
    buf[1] = KXCJK1013_REG_DATA_CTRL_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

    //set wakeup threshold
    buf[0] = KXCJK1013_REG_WAKEUP_THRESHOLD_ADDR;
    buf[1] = KXCJK1013_REG_WAKEUP_THRESHOLD_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

    //enter operation mode
    buf[0] = KXCJK1013_REG_CTRL1_ADDR;
    buf[1] = 0x80 | KXCJK1013_REG_CTRL1_VALUE | KXCJK1013_BIT_MOTION_DETECT_ENABLE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);
}


void kxcjk1013_interrupt_release(void)
{
    uint8_t buf[2];

    buf[0] = KXCJK1013_REG_INT_REL_ADDR;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 1, TWI_DONT_ISSUE_STOP);
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR | TWI_READ_BIT, buf, 1, TWI_ISSUE_STOP);
}

bool kxcjk1013_init(void)
{

    uint8_t buf[2];

    //nrf_delay_ms(10);
    //kxcjk1013_reset();
	
    //who am I
    buf[0] = KXCJK1013_REG_WHO_ADDR;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 1, TWI_DONT_ISSUE_STOP);
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR | TWI_READ_BIT, buf, 1, TWI_ISSUE_STOP);
    if (buf[0] != KXCJK1013_REG_WHO_VALUE)
    {
        return false;
    }

    // enter stand by mode
    //set res and range
    buf[0] = KXCJK1013_REG_CTRL1_ADDR;
    buf[1] = KXCJK1013_REG_CTRL1_VALUE;
    twi_master_transfer(KXCJK1013_I2C_SLAVE_ADDR, buf, 2, TWI_ISSUE_STOP);

   kxcjk1013_interrupt_release();
	kxcjk1013_motion_mode();

    return true;
}


