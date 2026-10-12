#include "cardputer_board.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>

// PI4IOE5V6408 IO expander on the Cap LoRa-1262 (I2C 0x43). M5Stack: "Enable
// the RF antenna switch: set P0 of the PI4IOE IO expansion chip to high".
// Registers from the PI4IOE5V6408 datasheet.
static constexpr uint8_t IOE_ADDR = 0x43;
static constexpr uint8_t IOE_DIR = 0x03;        // 1 = output
static constexpr uint8_t IOE_OUT = 0x05;        // output level
static constexpr uint8_t IOE_HIGH_Z = 0x07;     // 1 = high impedance (reset: all)

static bool ioeRead(uint8_t reg, uint8_t& v) {
  Wire.beginTransmission(IOE_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(IOE_ADDR, (uint8_t)1) != 1) return false;
  v = Wire.read();
  return true;
}

static bool ioeWrite(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(IOE_ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

static bool ioeSetBits(uint8_t reg, uint8_t set, uint8_t clear) {
  uint8_t v;
  return ioeRead(reg, v) && ioeWrite(reg, (uint8_t)((v | set) & ~clear));
}

void CardputerBoard::begin() {
  ESP32Board::begin();   // Wire on PIN_BOARD_SDA/SCL (the internal bus, 8/9)
  Wire.setClock(400000);

  // P0 high, push-pull output: the cap's RF switch is powered.
  _capFound = ioeSetBits(IOE_OUT, 0x01, 0) && ioeSetBits(IOE_DIR, 0x01, 0) && ioeSetBits(IOE_HIGH_Z, 0, 0x01);
}

// Battery on GPIO10 (ADC1 channel 9) through a 2:1 divider, measured the way
// esp-vim's esp_power.c does it (Apache-2.0): 12 dB attenuation, Espressif's
// calibration, the mean of 8 readings. Ported to the ESP-IDF 4.4 ADC API that
// this Arduino core has.
uint16_t CardputerBoard::getBattMilliVolts() {
  static esp_adc_cal_characteristics_t chars;
  static bool ready = false;
  if (!ready) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_9, ADC_ATTEN_DB_11);   // "DB_11" is the 12 dB setting
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &chars);
    ready = true;
  }
  // The voltage moves with the load (radio, backlight) and the ADC is noisy:
  // a new reading at most every 2 s, smoothed (each one moves the result a
  // quarter of the way), so the percentage doesn't jump about. A large step
  // (charger plugged in or out) is taken at once.
  static uint32_t lastAt = 0;
  static uint32_t smooth = 0;   // mV * 4
  uint32_t now = millis();
  if (smooth && now - lastAt < 2000) return (uint16_t)(smooth / 4);
  lastAt = now;
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_9), &chars);
  uint32_t mv = sum / 8 * 2;
  if (!smooth || mv * 4 > smooth + 4 * 150 || mv * 4 + 4 * 150 < smooth) smooth = mv * 4;
  else smooth = (smooth * 3 + mv * 4) / 4;
  return (uint16_t)(smooth / 4);
}
