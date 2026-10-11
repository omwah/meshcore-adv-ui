#include "cardputer_board.h"

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

uint16_t CardputerBoard::getBattMilliVolts() {
  // GPIO10 through a 2:1 divider.
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(PIN_VBAT_READ);
  return (uint16_t)(mv / 8 * 2);
}
