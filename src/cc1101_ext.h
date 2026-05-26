#pragma once

#include "../subghz_gate_scanner.h"

/* CC1101 Configuration Registers */
#define CC1101_IOCFG2    0x00
#define CC1101_IOCFG1    0x01
#define CC1101_IOCFG0    0x02
#define CC1101_FIFOTHR   0x03
#define CC1101_SYNC1     0x04
#define CC1101_SYNC0     0x05
#define CC1101_PKTLEN    0x06
#define CC1101_PKTCTRL1  0x07
#define CC1101_PKTCTRL0  0x08
#define CC1101_ADDR      0x09
#define CC1101_CHANNR    0x0A
#define CC1101_FSCTRL1   0x0B
#define CC1101_FSCTRL0   0x0C
#define CC1101_FREQ2     0x0D
#define CC1101_FREQ1     0x0E
#define CC1101_FREQ0     0x0F
#define CC1101_MDMCFG4   0x10
#define CC1101_MDMCFG3   0x11
#define CC1101_MDMCFG2   0x12
#define CC1101_MDMCFG1   0x13
#define CC1101_MDMCFG0   0x14
#define CC1101_DEVIATN   0x15
#define CC1101_MCSM2     0x16
#define CC1101_MCSM1     0x17
#define CC1101_MCSM0     0x18
#define CC1101_FOCCFG    0x19
#define CC1101_BSCFG     0x1A
#define CC1101_AGCCTRL2  0x1B
#define CC1101_AGCCTRL1  0x1C
#define CC1101_AGCCTRL0  0x1D
#define CC1101_WOREVT1   0x1E
#define CC1101_WOREVT0   0x1F
#define CC1101_WORCTRL   0x20
#define CC1101_FREND1    0x21
#define CC1101_FREND0    0x22
#define CC1101_FSCAL3    0x23
#define CC1101_FSCAL2    0x24
#define CC1101_FSCAL1    0x25
#define CC1101_FSCAL0    0x26
#define CC1101_RCCTRL1   0x27
#define CC1101_RCCTRL0   0x28
#define CC1101_FSTEST    0x29
#define CC1101_PTEST     0x2A
#define CC1101_AGCTEST   0x2B
#define CC1101_TEST2     0x2C
#define CC1101_TEST1     0x2D
#define CC1101_TEST0     0x2E

/* CC1101 Status Registers (burst read required: addr | 0xC0) */
#define CC1101_PARTNUM   0x30
#define CC1101_VERSION   0x31
#define CC1101_FREQEST   0x32
#define CC1101_LQI       0x33
#define CC1101_RSSI_REG  0x34
#define CC1101_MARCSTATE 0x35
#define CC1101_WORTIME1  0x36
#define CC1101_WORTIME0  0x37
#define CC1101_PKTSTATUS 0x38
#define CC1101_VCO_VC_DAC 0x39
#define CC1101_TXBYTES   0x3A
#define CC1101_RXBYTES   0x3B

/* CC1101 Command Strobes */
#define CC1101_SRES      0x30
#define CC1101_SFSTXON   0x31
#define CC1101_SXOFF     0x32
#define CC1101_SCAL      0x33
#define CC1101_SRX       0x34
#define CC1101_STX       0x35
#define CC1101_SIDLE     0x36
#define CC1101_SWOR      0x38
#define CC1101_SPWD      0x39
#define CC1101_SFRX      0x3A
#define CC1101_SFTX      0x3B
#define CC1101_SWORRST   0x3C
#define CC1101_SNOP      0x3D

/* Expected identification values */
#define CC1101_PARTNUM_EXPECTED  0x00
#define CC1101_VERSION_EXPECTED  0x14

/* XTAL frequency in Hz */
#define CC1101_XTAL_HZ   26000000UL

bool    cc1101_ext_init(AppState* app);
void    cc1101_ext_deinit(AppState* app);
void    cc1101_set_frequency(AppState* app, float freq_mhz);
int8_t  cc1101_get_rssi(AppState* app);
void    cc1101_strobe(AppState* app, uint8_t cmd);
void    cc1101_write_reg(AppState* app, uint8_t addr, uint8_t val);
uint8_t cc1101_read_reg(AppState* app, uint8_t addr);
void    cc1101_enter_rx(AppState* app);
void    cc1101_idle(AppState* app);
void    cc1101_apply_modulation(AppState* app);
