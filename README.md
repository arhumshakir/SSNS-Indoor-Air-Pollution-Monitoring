# SSNS — Indoor Air Pollution Monitoring System

Edge-to-gateway indoor air-quality monitor for the **Smart Sensor Network
Systems** course (Frankfurt UAS). An **nRF52840 DK** reads CO2/PM/TVOC/temp/
light, filters the data, and BLE-broadcasts it; an **nRF52840 Dongle** scans
the broadcasts and streams JSON to a host, which shows a live dashboard.

```
[Sensors]──I2C/ADC──[nRF52840 DK]══BLE advertise══>[nRF52840 Dongle]──USB──>[PC: parser.py / dashboard.py]
```

## Repository layout
```
edge_node/     Zephyr app for the nRF52840 DK (sensors + BLE broadcaster)
  src/         main.c, per-sensor drivers, EMA filter, payload format
  boards/      devicetree overlay (I2C pins + ADC channel)
  prj.conf, CMakeLists.txt
gateway/       Zephyr app for the nRF52840 Dongle (BLE observer + USB JSON)
  src/, boards/, prj.conf, CMakeLists.txt
backend/       Host Python: parser.py (serial→CSV) and dashboard.py (live web)
docs/          WIRING.md, wiring_diagram.svg
```

---

## ⏱ Do this in order (vertical slice first!)

Do **not** wire all six sensors and flash everything at once. Bring it up as a
thin vertical slice, then widen. Order that minimises pain:

**Phase 0 — Toolchain (≈30–45 min)**
1. Install **nRF Connect for VS Code** + the **nRF Connect SDK** (toolchain
   manager). Pick one SDK version (v2.6.x or newer is fine) and stick to it.
2. Open VS Code → nRF Connect → *Create a new application* → *Add existing* →
   point at `edge_node/`. Board target: `nrf52840dk/nrf52840`.
3. Build → Flash a stock **Blinky** first to prove the toolchain + DK work.

**Phase 1 — One sensor on serial (≈45 min)**
4. In `edge_node/src/main.c` set `ENABLE_SCD41 1` and the other four
   `ENABLE_* 0`.
5. Wire **only** the SCD41 (3.3V, GND, SDA→P0.26, SCL→P0.27) + 4.7kΩ pull-ups.
6. Build + flash. Open the **RTT/serial terminal** (nRF Terminal in VS Code).
   You should see a JSON line with a real `co2` value every 5 s.
   - Nothing? Check: common ground, pull-ups, SDA/SCL not swapped, address 0x62.

**Phase 2 — BLE link (≈30 min)**
7. The edge node is already advertising. Install **nRF Connect for Mobile** on
   your phone, scan, and confirm you see `IAQ-Node` with manufacturer data.

**Phase 3 — Gateway + PC (≈45 min)**
8. Build `gateway/` for board target `nrf52840dongle/nrf52840`.
   Flash the dongle (press its reset button to enter bootloader, then use
   *nRF Connect Programmer* or `nrfutil`).
9. Plug the dongle into the PC. It appears as a serial port (COMx / ttyACMx).
10. `cd backend && pip install -r requirements.txt`
    `python parser.py --port COM7`  → JSON lines from the air should print.

**Phase 4 — Add the rest, then the dashboard (remaining time)**
11. Switch `ENABLE_MCP9808`, then `ENABLE_LDR`, then `ENABLE_CCS811`, then
    `ENABLE_SPS30` to `1` **one at a time**, wiring each and re-flashing.
    Confirm each new field shows up before adding the next.
12. `python dashboard.py --port COM7` → open <http://localhost:8000>.

### No-hardware fallback (so you can always present)
`python dashboard.py --demo` runs the full dashboard on synthetic data — use it
to rehearse and as a backup if the bench setup fights you tomorrow.

---

## Build commands (CLI alternative to the VS Code buttons)
```bash
# Edge node
cd edge_node
west build -b nrf52840dk/nrf52840 -p
west flash

# Gateway
cd ../gateway
west build -b nrf52840dongle/nrf52840 -p
# flash the dongle with nRF Connect Programmer (DFU) or:
# nrfutil pkg generate --hw-version 52 --sd-req 0x00 --application build/zephyr/zephyr.hex --application-version 1 dfu.zip
# nrfutil dfu usb-serial -pkg dfu.zip -p COMx
```
> Board target strings use the new hwmodelv2 form (`nrf52840dk/nrf52840`). On
> older SDKs use `nrf52840dk_nrf52840` / `nrf52840dongle_nrf52840`.

## Data format
See `edge_node/src/payload.h` — 18-byte BLE manufacturer payload, little-endian,
company ID `0xFFFF`, CRC-8 over the data bytes. The gateway prints e.g.:
```json
{"co2":842,"temp":22.40,"rh":47.10,"pm25":12.3,"tvoc":142,"eco2":910,"lux":450,"rssi":-63,"pkt":89}
```

## Honest status
This is a **reference scaffold**, written carefully but **not compiled or tested
on hardware**. Expect to debug at the bench — that's normal for sensor bring-up.
The driver command sequences follow each sensor's datasheet; the CCS811 needs
~20 min run-in before its numbers are meaningful, and the SCD41 emits its first
sample ~5 s after start.

## FTDI / Waveshare FT232 note
The dongle streams JSON over its **own native USB**, so the FTDI bridge is **not
required** for the main path. Use the FT232 only if you instead take UART pins
out of a board (TX→RX, RX→TX, GND→GND) and read that COM port with the same
`parser.py`.

## Team ownership (from the proposal)
- Embedded core / EMA — Arhum Bin Shakir
- Sensor drivers / wiring — Muhammad Izhan Alam Khan
- BLE + gateway — Haseeb Ahmed
- Backend + dashboard — Touqeer Khalid
- Product owner — Syed Sameem Manzoor Rizvi
- Scrum master / QA — Muhammad Hussain Rizwan
