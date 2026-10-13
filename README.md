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

## Remote node mode

The Cardputer can also be the keyboard and screen of another MeshCore node
that runs stock **companion** firmware (a Heltec, T-Echo, RAK...), over
Bluetooth, the way the phone app is. Settings > Mesh node > remote switches
to it (the device restarts); its own LoRa radio then sleeps and need not
even be attached. Remote node (in the main menu) finds nodes nearby and
connects with the node's Bluetooth PIN (stock default 123456); the device
reconnects by itself after that.

Everything works against the remote node: chats, contacts, channels,
heard adverts, repeater and room admin, trace and ping, radio settings,
diagnostics and the packet log (from the node's raw receive log). History is
kept on the Cardputer, per node. Backup & restore saves the node's settings,
channels, contacts and identity (if the node allows export) to the SD card
and puts them back on the node. The Cardputer's GPS can set the node's clock
and advert position. Remote node also restarts or erases the node.

The Bluetooth build only: the USB-only build has no remote mode.

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
  - Chats: conversations, unread first by recency. The selected chat's last message scrolls by in full (Settings > Chat preview: "short" scrolls only its first 44 characters).
  - Contacts.
  - Nodes: repeaters, rooms and sensors.
  - Channels.
- **Conversation:**
  - Each message shows its delivery state: sending, sent, delivered or failed, with retries and repeat counts.
  - Incoming messages show hops and SNR.
  - A "New" line marks where the messages that arrived since you last opened the chat begin, and the chat opens scrolled to it.
  - You can compose replies inline, in a word-wrapped box.
  - History: the newest 512 messages stay on flash. With an SD card, older
    ones move to `/meshcore-adv-ui/history/` on the card instead of being
    lost. Up at the top of a conversation loads older messages, and Down at
    the bottom comes back.
  - "Clear history" on a contact or channel deletes its messages, including
    the card's copy.
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
- **Settings:** node name, theme, brightness, screen timeout, text size, chat preview, key filter, sound, new-message alert, auto-add, ping bot, Bluetooth and PIN, factory reset.
- **New-message alert:** a 2-second burst of colours on the RGB LED with the screen left off (the default, easiest on the battery), the screen turning on, both, or neither. The chime follows the Sound setting either way. Enter on the setting previews the colours. The LED runs on the backlight's power, so the screen glows briefly while it flashes.
- **Ping bot** (local node mode, off by default):
  - A direct message that is just `ping` or `test` gets an automatic reply with the path it took, for example `pong: 2 hops a1>Hilltop, SNR 6.5`.
  - Relays are named when their hash matches exactly one repeater or room contact.
  - A DM that came by a direct route has no path left when it arrives, so the reply shows our stored route to the sender, reversed and marked `(stored route)`.
  - Each contact gets at most one answer every 10 seconds.
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
pixi run simnet-test         # UI on real MeshCore nodes in a simulated mesh
pixi run ci                  # everything, including size budgets
```

Flash with `pio run -e cardputer_adv -t upload`. `pixi run package` writes
release images for esptool, web flashers, M5Burner and M5Launcher to `dist/`.
See `docs/FLASHING.md` for how to install them, and `docs/DEVICE-TESTING.md`
for the first-flash checklist.

| Build | What | Flash | Static RAM |
|---|---|---|---|
| `cardputer_adv` | UI + Bluetooth phone companion + remote node mode (default) | ~0.94 MB | ~113 KB |
| `cardputer_adv_usb` | UI + USB companion, no Bluetooth | ~0.69 MB | ~101 KB |

The stock MeshCore companion builds are about 1.25 MB with Bluetooth and
0.64 MB without. The keyboard UI and its diagnostics add about 45 KB. Our
Bluetooth build uses NimBLE instead of stock's Bluedroid, which makes it
about 0.3 MB smaller than stock even with the UI and remote mode. The phone apps see the same
service; a phone that was paired with a Bluedroid build has to forget the
device and pair again once.

### Simulators

There are two simulators. Both run the real UI on a virtual clock, and key
presses go through the same Cardputer keymap code as the device.

- **`sim`** uses a scripted mock mesh. It gives deterministic screenshots of
  every screen and theme. Scenarios are in `test/scenarios/*.sim`.
- **`simnet`** runs stock MeshCore: our node, scripted companion nodes, and
  unmodified MeshCore repeaters, on a simulated LoRa medium with real airtime
  and per-link SNR. It covers routing, ACKs, retries, admin, trace, channels,
  backup and restore, and reboots. Scenarios with expectations are in
  `test/simnet/*.sim`. With `--remote`, the UI drives our node as a remote
  companion over a simulated Bluetooth link (it paces and drops frames as
  stock's BLE interface does); scenarios marked `# both-modes` run both ways.

The script commands are listed in `boards/sim/main.cpp`,
`boards/simnet/main.cpp` and `boards/sim/runner.cpp`.

## Layout

```
src/ui/        UI: canvas, fonts, themes, widgets, screens (no Arduino/MeshCore)
src/model/     MeshModel: the UI's only view of the mesh
src/app/       MeshCore adapter (local node), radio presets, backup
src/app/remote/ remote node: companion protocol client and its MeshModel
src/hal/       display / keyboard interfaces
boards/<name>/ device drivers and MeshCore target for a board
boards/sim/    mock-data simulator and the shared script runner
boards/simnet/ MeshCore on the host: Arduino shim, LoRa medium, nodes, repeaters
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
