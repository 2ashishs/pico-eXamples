#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

 // device has default bus address of 0x76
#define BMP280_I2C_ADDR _u(0x76)
#define BMP280_I2C_SDA_PIN    4
#define BMP280_I2C_SCL_PIN    5
#define BMP280_I2C_BAUDRATE    100*1000 //100KhZ

// hardware registers
#define REG_CONFIG _u(0xF5)
#define REG_CTRL_MEAS _u(0xF4)
#define REG_RESET _u(0xE0)

#define REG_TEMP_XLSB _u(0xFC)
#define REG_TEMP_LSB _u(0xFB)
#define REG_TEMP_MSB _u(0xFA)

// calibration registers
#define REG_DIG_T1_LSB _u(0x88)
#define REG_DIG_T1_MSB _u(0x89)
#define REG_DIG_T2_LSB _u(0x8A)
#define REG_DIG_T2_MSB _u(0x8B)
#define REG_DIG_T3_LSB _u(0x8C)
#define REG_DIG_T3_MSB _u(0x8D)

// number of calibration registers to be read
#define NUM_CALIB_PARAMS 6

struct BMP280_calib_param {
    // temperature params
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
};


void BMP280_reset() {
    // reset the device with the power-on-reset procedure
    uint8_t buf[2] = { REG_RESET, 0xB6 };
    i2c_write_blocking(i2c0, BMP280_I2C_ADDR, buf, 2, false);
}


// intermediate function that calculates the fine resolution temperature
// used for both pressure and temperature conversions
int32_t BMP280_convert(int32_t temp, struct BMP280_calib_param* params) {
    // use the 32-bit fixed point compensation implementation given in the
    // datasheet

    int32_t var1, var2;
    var1 = ((((temp >> 3) - ((int32_t)params->dig_t1 << 1))) * ((int32_t)params->dig_t2)) >> 11;
    var2 = (((((temp >> 4) - ((int32_t)params->dig_t1)) * ((temp >> 4) - ((int32_t)params->dig_t1))) >> 12) * ((int32_t)params->dig_t3)) >> 14;
    return var1 + var2;
}


int32_t BMP280_convert_temp(int32_t temp, struct BMP280_calib_param* params) {
    // uses the BMP280 calibration parameters to compensate the temperature value read from its registers
    int32_t t_fine = BMP280_convert(temp, params);
    return (t_fine * 5 + 128) >> 8;
}


void BMP280_read_raw(int32_t* temp) {
    // BMP280 data registers are auto-incrementing and we have 3 temperature
    // registers, so we start at 0xFA and read 3 bytes to 0xFC
    // note: normal mode does not require further ctrl_meas and config register writes
    uint8_t buf[3];
    uint8_t reg = REG_TEMP_MSB;
    i2c_write_blocking(i2c0, BMP280_I2C_ADDR, &reg, 1, true);  // true to keep master control of bus
    i2c_read_blocking(i2c0, BMP280_I2C_ADDR, buf, 3, false);  // false - finished with bus
    // store the 20 bit read in a 32 bit signed integer for conversion
    *temp = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
}


void BMP280_get_calib_params(struct BMP280_calib_param* params) {
    // raw temp and pressure values need to be calibrated according to
    // parameters generated during the manufacturing of the sensor
    // there are 3 temperature params, and 9 pressure params, each with a LSB
    // and MSB register, so we read from 24 registers

    uint8_t buf[NUM_CALIB_PARAMS] = { 0 };
    uint8_t reg = REG_DIG_T1_LSB;
    i2c_write_blocking(i2c0, BMP280_I2C_ADDR, &reg, 1, true);  // true to keep master control of bus
    // read in one go as register addresses auto-increment
    i2c_read_blocking(i2c0, BMP280_I2C_ADDR, buf, NUM_CALIB_PARAMS, false);  // false, we're done reading

    // store these in a struct for later use
    params->dig_t1 = (uint16_t)(buf[1] << 8) | buf[0];
    params->dig_t2 = (int16_t)(buf[3] << 8) | buf[2];
    params->dig_t3 = (int16_t)(buf[5] << 8) | buf[4];
}


void BMP280_init() {
    // use the "handheld device dynamic" optimal setting (see datasheet)
    uint8_t buf[2];

    // 500ms sampling time, x16 filter
    const uint8_t reg_config_val = ((0x04 << 5) | (0x05 << 2)) & 0xFC;

    // send register number followed by its corresponding value
    buf[0] = REG_CONFIG;
    buf[1] = reg_config_val;
    i2c_write_blocking(i2c0, BMP280_I2C_ADDR, buf, 2, false);

    // osrs_t x1, osrs_p x4, normal mode operation
    const uint8_t reg_ctrl_meas_val = (0x01 << 5) | (0x03 << 2) | (0x03);
    buf[0] = REG_CTRL_MEAS;
    buf[1] = reg_ctrl_meas_val;
    i2c_write_blocking(i2c0, BMP280_I2C_ADDR, buf, 2, false);
}


void BMP280_init_i2c() {
    gpio_init(BMP280_I2C_SDA_PIN);
    gpio_set_function(BMP280_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BMP280_I2C_SDA_PIN);

    gpio_init(BMP280_I2C_SCL_PIN);
    gpio_set_function(BMP280_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BMP280_I2C_SCL_PIN);

    i2c_init(i2c0, BMP280_I2C_BAUDRATE);
}


int main() {
    stdio_init_all();
    //Code here
    printf("Hello BMP280!! Initializing..\n\n");
    BMP280_init_i2c();
    BMP280_init();
    // retrieve fixed compensation params
    struct BMP280_calib_param params;
    BMP280_get_calib_params(&params);

    int32_t raw_temperature;
    sleep_ms(250); // sleep so that data polling and register update don't collide

measurement_poll:
    BMP280_read_raw(&raw_temperature);
    //printf("\nRaw Temp: %d\nRaw Pressure: %d\n", raw_temperature, raw_pressure);
    int32_t temperature = BMP280_convert_temp(raw_temperature, &params);
    printf("Temp. = %.2f C\r", temperature / 100.f);

    sleep_ms(1000);
    goto measurement_poll;

    return 0;
}
