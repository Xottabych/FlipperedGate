#include "cc1101_ext.h"

/* OOK 650 baud async config (matches FuriHalSubGhzPresetOok650Async) */
static const uint8_t cc1101_ook_regs[][2] = {
    {CC1101_IOCFG0,   0x0D}, /* GD0: assert on carrier sense / RSSI above threshold */
    {CC1101_FIFOTHR,  0x47},
    {CC1101_PKTCTRL0, 0x32}, /* infinite packet length, async serial mode */
    {CC1101_FSCTRL1,  0x06},
    {CC1101_MDMCFG4,  0xF7}, /* OOK, 4.8 kBaud */
    {CC1101_MDMCFG3,  0x83},
    {CC1101_MDMCFG2,  0x38}, /* OOK, no sync word detection */
    {CC1101_MDMCFG1,  0x23},
    {CC1101_MDMCFG0,  0x4B},
    {CC1101_MCSM0,    0x18},
    {CC1101_FOCCFG,   0x16},
    {CC1101_AGCCTRL2, 0x07},
    {CC1101_AGCCTRL1, 0x00},
    {CC1101_AGCCTRL0, 0x91},
    {CC1101_FREND1,   0x56},
    {CC1101_FREND0,   0x11},
    {CC1101_FSCAL3,   0xE9},
    {CC1101_FSCAL2,   0x2A},
    {CC1101_FSCAL1,   0x00},
    {CC1101_FSCAL0,   0x1F},
    {CC1101_TEST2,    0x81},
    {CC1101_TEST1,    0x35},
    {CC1101_TEST0,    0x09},
    {0xFF, 0xFF}, /* sentinel */
};

/* 2-FSK ~4.8 kBaud, 47 kHz deviation */
static const uint8_t cc1101_fsk_regs[][2] = {
    {CC1101_IOCFG0,   0x0D},
    {CC1101_FIFOTHR,  0x47},
    {CC1101_PKTCTRL0, 0x32},
    {CC1101_FSCTRL1,  0x06},
    {CC1101_MDMCFG4,  0xCA},
    {CC1101_MDMCFG3,  0x83},
    {CC1101_MDMCFG2,  0x00}, /* 2-FSK, no sync word */
    {CC1101_MDMCFG1,  0x22},
    {CC1101_MDMCFG0,  0xF8},
    {CC1101_DEVIATN,  0x47}, /* ~47 kHz deviation */
    {CC1101_MCSM0,    0x18},
    {CC1101_FOCCFG,   0x1D},
    {CC1101_AGCCTRL2, 0x03},
    {CC1101_AGCCTRL1, 0x40},
    {CC1101_AGCCTRL0, 0x91},
    {CC1101_FREND1,   0x56},
    {CC1101_FREND0,   0x10},
    {CC1101_FSCAL3,   0xE9},
    {CC1101_FSCAL2,   0x2A},
    {CC1101_FSCAL1,   0x00},
    {CC1101_FSCAL0,   0x1F},
    {CC1101_TEST2,    0x81},
    {CC1101_TEST1,    0x35},
    {CC1101_TEST0,    0x09},
    {0xFF, 0xFF}, /* sentinel */
};

/* ── SPI bit-bang primitives ─────────────────────────────────────────────── */

static inline void cs_low(AppState* app) {
    furi_hal_gpio_write(app->pin_cs, false);
}

static inline void cs_high(AppState* app) {
    furi_hal_gpio_write(app->pin_cs, true);
}

static void spi_write_byte(AppState* app, uint8_t byte) {
    for(int i = 7; i >= 0; i--) {
        furi_hal_gpio_write(app->pin_mosi, (byte >> i) & 1);
        furi_delay_us(1);
        furi_hal_gpio_write(app->pin_sck, true);
        furi_delay_us(1);
        furi_hal_gpio_write(app->pin_sck, false);
    }
}

static uint8_t spi_read_byte(AppState* app) {
    uint8_t byte = 0;
    for(int i = 7; i >= 0; i--) {
        furi_hal_gpio_write(app->pin_sck, true);
        furi_delay_us(1);
        if(furi_hal_gpio_read(app->pin_miso)) {
            byte |= (1u << i);
        }
        furi_hal_gpio_write(app->pin_sck, false);
        furi_delay_us(1);
    }
    return byte;
}

/* Wait for MISO to go low (CC1101 ready signal after CS asserted) */
static void spi_wait_miso_low(AppState* app) {
    uint32_t timeout = 1000;
    while(furi_hal_gpio_read(app->pin_miso) && timeout--) {
        furi_delay_us(1);
    }
}

/* ── CC1101 register access ──────────────────────────────────────────────── */

void cc1101_write_reg(AppState* app, uint8_t addr, uint8_t val) {
    cs_low(app);
    spi_wait_miso_low(app);
    spi_write_byte(app, addr & 0x3F); /* write, single byte */
    spi_write_byte(app, val);
    cs_high(app);
}

uint8_t cc1101_read_reg(AppState* app, uint8_t addr) {
    cs_low(app);
    spi_wait_miso_low(app);
    spi_write_byte(app, (addr & 0x3F) | 0x80); /* read, single byte */
    uint8_t val = spi_read_byte(app);
    cs_high(app);
    return val;
}

/* Read status register (requires burst bit) */
static uint8_t cc1101_read_status(AppState* app, uint8_t addr) {
    cs_low(app);
    spi_wait_miso_low(app);
    spi_write_byte(app, (addr & 0x3F) | 0xC0); /* read + burst (status) */
    uint8_t val = spi_read_byte(app);
    cs_high(app);
    return val;
}

void cc1101_strobe(AppState* app, uint8_t cmd) {
    cs_low(app);
    spi_wait_miso_low(app);
    spi_write_byte(app, cmd & 0x3F);
    cs_high(app);
}

/* ── High-level CC1101 API ───────────────────────────────────────────────── */

bool cc1101_ext_init(AppState* app) {
    /* Enable 5V on GPIO Pin 1 so the external module works on battery */
    furi_hal_power_enable_otg();
    furi_delay_ms(15); /* allow voltage to stabilise */

    /* Configure GPIO directions */
    furi_hal_gpio_init(app->pin_sck,  GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(app->pin_mosi, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(app->pin_cs,   GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(app->pin_miso, GpioModeInput,          GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(app->pin_gd0,  GpioModeInput,          GpioPullNo, GpioSpeedVeryHigh);

    /* Idle state: CS high, SCK low */
    cs_high(app);
    furi_hal_gpio_write(app->pin_sck, false);
    furi_delay_ms(1);

    /* Power-on reset sequence per CC1101 datasheet §10.1 */
    cs_low(app);
    furi_delay_us(10);
    cs_high(app);
    furi_delay_us(40);
    cc1101_strobe(app, CC1101_SRES);
    furi_delay_ms(2);

    /* Verify chip identity */
    uint8_t partnum = cc1101_read_status(app, CC1101_PARTNUM);
    uint8_t version = cc1101_read_status(app, CC1101_VERSION);

    FURI_LOG_I("CC1101", "PARTNUM=0x%02X VERSION=0x%02X", partnum, version);

    if(partnum != CC1101_PARTNUM_EXPECTED || version != CC1101_VERSION_EXPECTED) {
        FURI_LOG_W("CC1101", "Unexpected chip id — no external CC1101?");
        return false;
    }

    /* Write modulation-appropriate register configuration */
    cc1101_apply_modulation(app);

    FURI_LOG_I("CC1101", "External CC1101 initialised OK");
    return true;
}

void cc1101_apply_modulation(AppState* app) {
    const uint8_t(*regs)[2] =
        (app->modulation == ModFSK) ? cc1101_fsk_regs : cc1101_ook_regs;
    cc1101_strobe(app, CC1101_SIDLE);
    furi_delay_us(100);
    for(size_t i = 0; regs[i][0] != 0xFF; i++) {
        cc1101_write_reg(app, regs[i][0], regs[i][1]);
    }
}

void cc1101_ext_deinit(AppState* app) {
    cc1101_strobe(app, CC1101_SIDLE);
    furi_delay_us(100);

    /* Restore GPIO pins to safe default (analog/high-Z) */
    furi_hal_gpio_init(app->pin_sck,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(app->pin_mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(app->pin_cs,   GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(app->pin_miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(app->pin_gd0,  GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    /* Cut 5V supply to external module */
    furi_hal_power_disable_otg();
}

void cc1101_set_frequency(AppState* app, float freq_mhz) {
    /* FREQ = freq_hz / (f_xtal / 2^16) = freq_hz * 65536 / 26000000 */
    uint32_t freq_word =
        (uint32_t)((freq_mhz * 1000000.0f / (float)CC1101_XTAL_HZ) * 65536.0f);

    cc1101_strobe(app, CC1101_SIDLE);
    furi_delay_us(100);

    cc1101_write_reg(app, CC1101_FREQ2, (freq_word >> 16) & 0xFF);
    cc1101_write_reg(app, CC1101_FREQ1, (freq_word >> 8) & 0xFF);
    cc1101_write_reg(app, CC1101_FREQ0,  freq_word & 0xFF);

    /* Calibrate PLL after frequency change */
    cc1101_strobe(app, CC1101_SCAL);
    furi_delay_ms(1);
}

int8_t cc1101_get_rssi(AppState* app) {
    uint8_t raw = cc1101_read_status(app, CC1101_RSSI_REG);
    int16_t rssi;
    if(raw >= 128) {
        rssi = ((int16_t)raw - 256) / 2 - 74;
    } else {
        rssi = (int16_t)raw / 2 - 74;
    }
    return (int8_t)rssi;
}

void cc1101_enter_rx(AppState* app) {
    cc1101_strobe(app, CC1101_SRX);
    furi_delay_us(200);
}

void cc1101_idle(AppState* app) {
    cc1101_strobe(app, CC1101_SIDLE);
    furi_delay_us(100);
}
