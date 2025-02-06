#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306_font.h"

/* SSD1306 Registers, Pins & Structs */

#define SSD1306_HEIGHT              32
#define SSD1306_WIDTH               128

#define SSD1306_I2C_ADDR            _u(0x3C)
#define SSD1306_I2C_BAUDRATE             400 * 1000 //400kHz

#define SSD1306_I2C_SDA_PIN         4
#define SSD1306_I2C_SCL_PIN         5

// commands (see datasheet)
#define SSD1306_SET_MEM_MODE        _u(0x20)
#define SSD1306_SET_COL_ADDR        _u(0x21)
#define SSD1306_SET_PAGE_ADDR       _u(0x22)
#define SSD1306_SET_HORIZ_SCROLL    _u(0x26)
#define SSD1306_SET_SCROLL          _u(0x2E)

#define SSD1306_SET_DISP_START_LINE _u(0x40)

#define SSD1306_SET_CONTRAST        _u(0x81)
#define SSD1306_SET_CHARGE_PUMP     _u(0x8D)

#define SSD1306_SET_SEG_REMAP       _u(0xA0)
#define SSD1306_SET_ENTIRE_ON       _u(0xA4)
#define SSD1306_SET_ALL_ON          _u(0xA5)
#define SSD1306_SET_NORM_DISP       _u(0xA6)
#define SSD1306_SET_INV_DISP        _u(0xA7)
#define SSD1306_SET_MUX_RATIO       _u(0xA8)
#define SSD1306_SET_DISP            _u(0xAE)
#define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
#define SSD1306_SET_COM_OUT_DIR_FLIP _u(0xC0)

#define SSD1306_SET_DISP_OFFSET     _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
#define SSD1306_SET_PRECHARGE       _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
#define SSD1306_SET_VCOM_DESEL      _u(0xDB)

#define SSD1306_PAGE_HEIGHT         _u(8)
#define SSD1306_NUM_PAGES           (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUF_LEN             (SSD1306_NUM_PAGES * SSD1306_WIDTH)

#define SSD1306_WRITE_MODE         _u(0xFE)
#define SSD1306_READ_MODE          _u(0xFF)


struct render_area {
    uint8_t start_col;
    uint8_t end_col;
    uint8_t start_page;
    uint8_t end_page;

    int buflen;
};

/* BMP280 Registers, Pins & Structs */

// device has default bus address of 0x76
#define BMP280_I2C_ADDR _u(0x76)
#define BMP280_I2C_SDA_PIN    14
#define BMP280_I2C_SCL_PIN    15
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

/* SSD1306 related functions */

static inline int GetFontIndex(uint8_t ch) {
    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
    }
    else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
    }
    else if (ch == '^') {
        return 37; //degrees
    }
    else if (ch == '=') {
        return 38; //equal to
    }
    else if (ch == ')') {
        return 39; //smile
    }
    else if (ch == '$') {
        return 40; //yen
    }
    else return  0; // Not got that char so space.
}

static void WriteChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch) {
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    // For the moment, only write on Y row boundaries (every 8 vertical pixels)
    y = y/8;

    ch = toupper(ch);
    int idx = GetFontIndex(ch);
    int fb_idx = y * 128 + x;

    for (int i=0;i<8;i++) {
        buf[fb_idx++] = font[idx * 8 + i];
    }
}

static void WriteString(uint8_t *buf, int16_t x, int16_t y, char *str) {
    // Cull out any string off the screen
    if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
        return;

    while (*str) {
        WriteChar(buf, x, y, *str++);
        x+=8;
    }
}

void calc_render_area_buflen(struct render_area *area) {
    // calculate how long the flattened buffer will be for a render area
    area->buflen = (area->end_col - area->start_col + 1) * (area->end_page - area->start_page + 1);
}

void SSD1306_send_cmd(uint8_t cmd) {
    // I2C write process expects a control byte followed by data
    // this "data" can be a command or data to follow up a command
    // Co = 1, D/C = 0 => the driver expects a command
    uint8_t buf[2] = {0x80, cmd};
    i2c_write_blocking(i2c0, SSD1306_I2C_ADDR, buf, 2, false);
}

void SSD1306_send_cmd_list(uint8_t *buf, int num) {
    for (int i=0;i<num;i++)
        SSD1306_send_cmd(buf[i]);
}

void SSD1306_send_buf(uint8_t buf[], int buflen) {
    // in horizontal addressing mode, the column address pointer auto-increments
    // and then wraps around to the next page, so we can send the entire frame
    // buffer in one gooooooo!

    // copy our frame buffer into a new buffer because we need to add the control byte
    // to the beginning

    uint8_t *temp_buf = malloc(buflen + 1);

    temp_buf[0] = 0x40;
    memcpy(temp_buf+1, buf, buflen);

    i2c_write_blocking(i2c0, SSD1306_I2C_ADDR, temp_buf, buflen + 1, false);

    free(temp_buf);
}

void render(uint8_t *buf, struct render_area *area) {
    // update a portion of the display with a render area
    uint8_t cmds[] = {
        SSD1306_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1306_SET_PAGE_ADDR,
        area->start_page,
        area->end_page
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
    SSD1306_send_buf(buf, area->buflen);
}

void SSD1306_init() {
    // Some of these commands are not strictly necessary as the reset
    // process defaults to some of these but they are shown here
    // to demonstrate what the initialization sequence looks like
    // Some configuration values are recommended by the board manufacturer

    uint8_t cmds[] = {
        SSD1306_SET_DISP,               // set display off
        /* memory mapping */
        SSD1306_SET_MEM_MODE,           // set memory address mode 0 = horizontal, 1 = vertical, 2 = page
        0x00,                           // horizontal addressing mode
        /* resolution and layout */
        SSD1306_SET_DISP_START_LINE,    // set display start line to 0
        SSD1306_SET_SEG_REMAP | 0x01,   // set segment re-map, column address 127 is mapped to SEG0
        SSD1306_SET_MUX_RATIO,          // set multiplex ratio
        SSD1306_HEIGHT - 1,             // Display height - 1
        SSD1306_SET_COM_OUT_DIR | 0x08, // set COM (common) output scan direction. Scan from bottom up, COM[N-1] to COM0
        SSD1306_SET_DISP_OFFSET,        // set display offset
        0x00,                           // no offset
        SSD1306_SET_COM_PIN_CFG,        // set COM (common) pins hardware configuration. Board specific magic number.
                                        // 0x02 Works for 128x32, 0x12 Possibly works for 128x64. Other options 0x22, 0x32
#if ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 32))
        0x02,
#elif ((SSD1306_WIDTH == 128) && (SSD1306_HEIGHT == 64))
        0x12,
#else
        0x02,
#endif
        /* timing and driving scheme */
        SSD1306_SET_DISP_CLK_DIV,       // set display clock divide ratio
        0x80,                           // div ratio of 1, standard freq
        SSD1306_SET_PRECHARGE,          // set pre-charge period
        0xF1,                           // Vcc internally generated on our board
        SSD1306_SET_VCOM_DESEL,         // set VCOMH deselect level
        0x30,                           // 0.83xVcc
        /* display */
        SSD1306_SET_CONTRAST,           // set contrast control
        0xFF,
        SSD1306_SET_ENTIRE_ON,          // set entire display on to follow RAM content
        SSD1306_SET_NORM_DISP,           // set normal (not inverted) display
        SSD1306_SET_CHARGE_PUMP,        // set charge pump
        0x14,                           // Vcc internally generated on our board
        SSD1306_SET_SCROLL | 0x00,      // deactivate horizontal scrolling if set. This is necessary as memory writes will corrupt if scrolling was enabled
        SSD1306_SET_DISP | 0x01, // turn display on
    };

    SSD1306_send_cmd_list(cmds, count_of(cmds));
}

/* BMP280 related functions */

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
    i2c_write_blocking(i2c1, BMP280_I2C_ADDR, &reg, 1, true);  // true to keep master control of bus
    i2c_read_blocking(i2c1, BMP280_I2C_ADDR, buf, 3, false);  // false - finished with bus
    // store the 20 bit read in a 32 bit signed integer for conversion
    *temp = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
}

void BMP280_get_calib_params(struct BMP280_calib_param* params) {
    // raw temp values need to be calibrated according to
    // parameters generated during the manufacturing of the sensor
    // there are 3 temperature params, each with a LSB
    // and MSB register, so we read from 6 registers

    uint8_t buf[NUM_CALIB_PARAMS] = { 0 };
    uint8_t reg = REG_DIG_T1_LSB;
    i2c_write_blocking(i2c1, BMP280_I2C_ADDR, &reg, 1, true);  // true to keep master control of bus
    // read in one go as register addresses auto-increment
    i2c_read_blocking(i2c1, BMP280_I2C_ADDR, buf, NUM_CALIB_PARAMS, false);  // false, we're done reading

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
    i2c_write_blocking(i2c1, BMP280_I2C_ADDR, buf, 2, false);

    // osrs_t x1, osrs_p x4, normal mode operation
    const uint8_t reg_ctrl_meas_val = (0x01 << 5) | (0x03 << 2) | (0x03);
    buf[0] = REG_CTRL_MEAS;
    buf[1] = reg_ctrl_meas_val;
    i2c_write_blocking(i2c1, BMP280_I2C_ADDR, buf, 2, false);
}

void init_i2c() {
    // i2c for OLED
    gpio_init(SSD1306_I2C_SDA_PIN);
    gpio_set_function(SSD1306_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_I2C_SDA_PIN);

    gpio_init(SSD1306_I2C_SCL_PIN);
    gpio_set_function(SSD1306_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_I2C_SCL_PIN);

    i2c_init(i2c0, SSD1306_I2C_BAUDRATE);

    // i2c for BMP280
    gpio_init(BMP280_I2C_SDA_PIN);
    gpio_set_function(BMP280_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BMP280_I2C_SDA_PIN);

    gpio_init(BMP280_I2C_SCL_PIN);
    gpio_set_function(BMP280_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BMP280_I2C_SCL_PIN);

    i2c_init(i2c1, BMP280_I2C_BAUDRATE);
}

int main() {
    stdio_init_all();
    //Code here
    init_i2c();
    //BMP280 init
    BMP280_init();
    struct BMP280_calib_param params; // retrieve fixed compensation params
    BMP280_get_calib_params(&params);
    int32_t raw_temperature;
    int32_t temperature;
    char* text_temperature;
    //SSD1306 init
    SSD1306_init();
    // Initialize render area for entire frame
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1306_WIDTH - 1,
        start_page : 0,
        end_page : SSD1306_NUM_PAGES - 1
        };
    // zero the entire display
    uint8_t buf[SSD1306_BUF_LEN];
    memset(buf, 0, SSD1306_BUF_LEN);
    render(buf, &frame_area);

    sleep_ms(250); // sleep so that data polling and register update don't collide

measure_display_loop:
    calc_render_area_buflen(&frame_area);
    BMP280_read_raw(&raw_temperature);
    temperature = BMP280_convert_temp(raw_temperature, &params);
    sprintf(text_temperature, "Temp: %.2f ^C", temperature/100.0f);
    printf("%s :)", text_temperature);
    // Write temperature to display
    WriteString(buf, 0, 0, text_temperature);
    render(buf, &frame_area);
    sleep_ms(1000);
    goto measure_display_loop;

    return 0;
}
