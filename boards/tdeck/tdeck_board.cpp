#include "tdeck_board.h"
#include <SPI.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

extern SPIClass loraSpi;

void TDeckBoard::begin() {
  // GPIO10 powers the keyboard's controller, the touch panel, the display
  // and the radio: on before anything probes them, and a moment to wake.
  pinMode(PIN_PERF_POWERON, OUTPUT);
  digitalWrite(PIN_PERF_POWERON, HIGH);
  // Everything on the shared SPI bus deselected before any of it talks.
  for (int cs : {P_LORA_NSS, PIN_TFT_CS, PIN_SD_CS}) {
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
  }
  pinMode(P_LORA_MISO, INPUT_PULLUP);
  loraSpi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
  delay(200);
  ESP32Board::begin();   // Wire on PIN_BOARD_SDA/SCL (18/8)
  // The keyboard's ESP32-C3 answers at 100 kHz; the touch panel shares the bus.
  Wire.setClock(100000);
}

// Battery on GPIO4 (ADC1 channel 3) through a 2:1 divider, measured as on the
// Cardputer (after esp-vim's esp_power.c, Apache-2.0): 12 dB attenuation,
// Espressif's calibration, the mean of 8 readings, smoothed.
uint16_t TDeckBoard::getBattMilliVolts() {
  static esp_adc_cal_characteristics_t chars;
  static bool ready = false;
  if (!ready) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);   // "DB_11" is the 12 dB setting
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &chars);
    ready = true;
  }
  static uint32_t lastAt = 0;
  static uint32_t smooth = 0;   // mV * 4
  uint32_t now = millis();
  if (smooth && now - lastAt < 2000) return (uint16_t)(smooth / 4);
  lastAt = now;
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_3), &chars);
  uint32_t mv = sum / 8 * 2;
  if (!smooth || mv * 4 > smooth + 4 * 150 || mv * 4 + 4 * 150 < smooth) smooth = mv * 4;
  else smooth = (smooth * 3 + mv * 4) / 4;
  return (uint16_t)(smooth / 4);
}
