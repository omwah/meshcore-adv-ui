# meshcore-adv-ui

A keyboard-native user interface for [MeshCore](https://github.com/meshcore-dev/MeshCore)
LoRa mesh radios. The first target is the M5Stack Cardputer ADV with the Cap
LoRa-1262 module. The UI is kept separate from the hardware, so other devices
can be added as new board directories.

- **Stock MeshCore inside.** MeshCore is a git submodule pinned to a release.
  Changes we need live as small patches in `patches/meshcore/`, so moving to a
  new MeshCore release means bumping the submodule and refreshing the patches.
- **Small.** The UI draws into a 16 KB 4-bit framebuffer with Spleen bitmap
  fonts (about 15 KB of flash). Only rows that changed are sent to the panel.
  Nothing on the UI side uses the heap.
- **Colour themes.** Every colour is a *role* (header, selection, unread,
  channel, repeater...). A theme is a 16-entry palette, so switching is
  instant. Themes: Midnight, Amber, Phosphor, Contrast, Daylight, Ocean.
- **Predictable keys:**
  - One press moves one step.
  - Enter never auto-repeats.
  - Lists stop at their ends instead of wrapping.
  - Pending keys are discarded when the screen changes.
  - The key filter (debounce) is adjustable in Settings.

## Keys (Cardputer)

| Key | In lists and menus | While typing |
|---|---|---|
| `;` `.` | up / down | type `;` `.` (Fn + key moves) |
| `,` `/` | previous / next tab, change value | type `,` `/` (Fn + key moves cursor) |
| Enter | open / select | send |
| `` ` `` (Esc) | back (on Home: clear search) | stop typing |
| hold Esc | go Home | go Home |
| Opt | main menu | main menu |
| Tab | details / actions for the selection | |
| letters | find (on Home), start writing (in a chat) | type |

## Screens

- **Home** has four tabs:
  - Chats: conversations, unread first by recency.
  - Contacts.
  - Nodes: repeaters, rooms and sensors.
  - Channels.
- **Conversation:**
  - Each message shows its delivery state: sending, sent, delivered or failed, with retries and repeat counts.
  - Incoming messages show hops and SNR.
  - You can compose replies inline.
- **Contact details:**
  - Path, hops, last heard, SNR/RSSI and position.
  - Actions: favourite, reset path, trace route, ping, remove.
- **Repeater / room admin:** login, status, neighbours, telemetry, clock sync, advert, CLI command.
- **Channels:** join a `#hashtag` channel, join or create a private one, mute, show the key, leave.
- **Radio:**
  - Community presets, including USA and USA - Southern California.
  - Custom frequency, bandwidth, SF, CR and TX power.
  - Repeat and RX boost.
- **Diagnostics:**
  - Radio page: noise floor, RSSI/SNR, duty cycle, packet counters, airtime, queues.
  - System page: heap, battery, uptime, capacity.
- **Packet log:** every received packet with its type, route, hops, SNR, RSSI, length and duplicate flag.
- **Heard adverts** and **Discover repeaters**.
- **GPS:**
  - The receiver is off by default.
  - Sharing your position in adverts and setting the clock at boot are separate options, also off by default.
- **Settings:** node name, theme, brightness, screen timeout, text size, key filter, sound, auto-add, Bluetooth and PIN, factory reset.
- **Backup & restore to SD:**
  - Automatic backup can be turned on or off.
  - Manual backup and restore, plus restore of the previous backup.
  - On a fresh install, the device offers to restore.

## Building

The host tools are managed by [pixi](https://pixi.sh):

```sh
git submodule update --init
pixi install
scripts/install-hooks.sh     # pre-commit check that keeps personal data out of git

pixi run test                # host unit tests
pixi run shots               # run the simulator scenarios -> shots/index.html
pixi run build               # Cardputer ADV firmware (BLE and USB-only builds)
```

### Simulator

`.pio/build/sim/program` runs the real UI with a scripted mock mesh and a
virtual clock. Key presses go through the same Cardputer keymap code as the
device. The scenarios are in `test/scenarios/*.sim`; see
`boards/sim/main.cpp` for the script commands.

## Layout

```
src/ui/        UI: canvas, fonts, themes, widgets, screens (no Arduino/MeshCore)
src/model/     MeshModel: the UI's only view of the mesh
src/app/       MeshCore adapter, radio presets, backup
src/hal/       display / keyboard interfaces
boards/<name>/ device drivers and MeshCore target for a board
boards/sim/    host simulator
external/      MeshCore submodule (never edited; see patches/)
```

## Credits

- The UI design is inspired by
  [anton-vinogradov/meshtastic-adv](https://github.com/anton-vinogradov/meshtastic-adv).
- The keyboard mapping and font tooling are adapted from esp-vim (Apache-2.0).
- The fonts are [Spleen](https://github.com/fcambus/spleen) by Frederic Cambus (BSD-2-Clause).

See `NOTICE` for details.

## License

MIT. See `LICENSE`.
