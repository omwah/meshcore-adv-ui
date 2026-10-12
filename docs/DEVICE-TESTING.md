# Device bring-up checklist (Cardputer ADV + Cap LoRa-1262)

Everything below was built and tested in the simulators. These items depend on
hardware facts that the simulators can't check. Verify them in this order on
first flash. Keep serial logs and photos of real devices **out of git**: they
can show MAC addresses, node keys and names.

## Flash

```sh
pixi run build                                  # both firmware images
pio run -e cardputer_adv -t upload              # USB-C; hold G0 while plugging in if needed
pio device monitor -e cardputer_adv             # 115200 baud
```

## 1. Boot, display, keyboard

Serial bring-up commands (Bluetooth build): `stat`, `bl <0-255>|gpio`,
`blf <hz> <duty>`, `fill <rrggbb>`, `init`, `shot` (dumps the UI frame) and
`key <row> <col>`. See `boards/cardputer_adv/diag.h`.

- [ ] **Splash.** It shows "MeshCore keyboard edition" right way up, with correct colours. Check that Midnight's header is dark blue, not yellow. If colours are inverted, toggle INVON (0x21) in `st7789_display.cpp`. If the picture is mirrored or offset, check MADCTL 0x60 and the gap (40, 53).
- [x] **Backlight.** It needs a low PWM rate: at 5 kHz the driver stays dark, while 256 Hz works (found on hardware).
- [ ] **Brightness and timeout.** Brightness follows Settings > Brightness, and the backlight turns off after the screen timeout. Any key wakes it without doing anything else.
- [ ] **Keyboard map.** Every key types its printed legend in a message. `; , . /` move in menus. Opt opens the menu. Holding `` ` `` goes Home.
- [ ] **No missed or doubled presses.** With Settings > Key filter at 60 ms, fast Up/Down presses must neither drop nor double. If they do, try other values (10–500 ms).

## 2. Radio (the riskiest assumptions)

- [x] **Radio starts.** Serial shows no `halt: radio init failed`. Serial command `radio` shows the live settings and counters.
- [ ] **RF switch enabled.** The Cap's antenna switch is powered by PI4IOE5V6408 (I2C 0x43) P0 high: registers 0x05 (output), 0x03 (direction) and 0x07 (high-Z) are set by `cardputer_board.cpp`. Without it, transmit works only at very short range. Check this with a second node a room away.
- [ ] **TCXO voltage.** It is set to 3.3 V (`SX126X_DIO3_TCXO_VOLTAGE`). If the radio fails to start or is deaf, try 1.8.
- [ ] **TX current limit.** It is 140 mA. The other open Cardputer port uses 240 mA for this Cap; only raise it after measuring.
- [x] **Receive (SoCal preset).** Packets and adverts from a stock node are received.
- [ ] **Transmit.** Another node hears our adverts and DMs (serial `radio`: tx count).
- [ ] **Interop.** Exchange adverts and a DM with a stock MeshCore node or the phone app on the same preset (default "USA", 910.525 MHz / 62.5 kHz / SF7 / CR5).

## 3. SD card (shares the LoRa SPI bus)

- [x] **Backup works.** "Back up now" succeeded on hardware.
- [ ] **Card detection.** Insert a FAT32 card. Backup & restore shows "SD card ready".
- [ ] **No radio disruption.** "Back up now" while messages arrive must not disturb the radio. RadioLib and SD each drive their own CS (5 and 12).
- [ ] **History archive.** With a card in, after more than 512 messages in all, scrolling up past the oldest flash message loads older ones from `/meshcore-adv-ui/history/`. "Clear history" removes that thread's file. (Covered in simnet with a 40-record ring.)
- [ ] **Fresh-install restore.** Erase flash (`pio run -t erase`), reflash, and accept the restore offer. Check that the same node key, contacts and channels come back.

## 4. GPS (ATGM336H)

- [ ] **Fix.** Turn GPS on. Within a few minutes outdoors the GPS screen shows satellites and a fix. The module's TX goes to GPIO15 and its RX to GPIO13, at 115200 baud. If nothing arrives, swap `PIN_GPS_TX/RX`.
- [ ] **Boot clock sync.** "Set clock from GPS at boot" sets the clock and then turns the receiver off again, when GPS itself is off.

## 5. Bluetooth (default build)

- [x] **Pairing and key import.** Pairing works, and so does the app's private key import (after enabling ENABLE_PRIVATE_KEY_IMPORT/EXPORT as stock does).
- [ ] **Phone app.** The MeshCore phone app pairs using the PIN from Settings. Messages sent from the phone appear in the device's history too, and the reverse.
- [ ] **Bluetooth toggle.** Settings > Bluetooth off stops advertising.

## 6. Sound

- [ ] **Chime.** Serial `beep` (or a new message with Settings > Sound on) plays two rising notes. Is the volume and tone OK? The codec is ES8311 at 0x18; I2S pins 41/43/42, as in M5Unified.
- [ ] **No chime when muted.** No chime for messages in muted channels, or with Sound off.

## 7. Power and memory

- [ ] **Battery drain.** With the idle sleep in the main loop: note the voltage on Diagnostics > System, run an hour on battery with the screen timing out, and note it again. Compare with an older build if possible.
- [ ] **Lower CPU clock.** Build with `-D ESP32_CPU_FREQ=80` (or 160): check that Bluetooth with the phone, radio receive and the UI still work, then repeat the drain test.

- [x] **Battery reading.** GPIO10 through a 2:1 divider, measured as esp-vim does. With the power switch off on USB it reads 4.14 V (full). With the switch on it read 3.48 V after an evening on USB: the battery itself, which appears not to charge fully while the device runs.
- [ ] **Charging.** Find out whether the Cardputer ADV charges with the power switch on, or only with it off.
- [ ] **Heap.** After an hour with Bluetooth connected, the heap minimum on Diagnostics > System stays above about 30 KB.
- [ ] **Launcher.** If you use bmorcelli's Launcher: the app runs from it, and after a re-install the restore offer appears.
