# Wiring — pin by pin

All I2C sensors share one bus (SDA + SCL). Power the **SPS30 from 5V** and
everything else from **3.3V**. **Every ground must be tied together** (DK GND +
MB102 GND + every sensor GND). See `wiring_diagram.svg` for the picture.

nRF52840 DK default pins (changeable in `edge_node/boards/nrf52840dk_nrf52840.overlay`):

| Function | nRF52840 DK pin |
|----------|-----------------|
| I2C SDA  | **P0.26** |
| I2C SCL  | **P0.27** |
| LDR analog in | **P0.02 (AIN0)** |
| 3.3V out | **VDD** |
| Ground   | **GND** |

## Per-sensor connections

### SCD41 — CO2 / temp / humidity (I2C 0x62)
| SCD41 pin | Connect to |
|-----------|-----------|
| VIN/VDD | 3.3V |
| GND | GND |
| SDA | P0.26 (SDA bus) |
| SCL | P0.27 (SCL bus) |

### CCS811 — TVOC / eCO2 (I2C 0x5A)
| CCS811 pin | Connect to |
|-----------|-----------|
| VCC | 3.3V |
| GND | GND |
| SDA | P0.26 (SDA bus) |
| SCL | P0.27 (SCL bus) |
| **WAKE / nWAKE** | **GND** (required, or I2C stays silent) |
| ADDR | GND → address 0x5A |

### MCP9808 — temperature (I2C 0x18)
| MCP9808 pin | Connect to |
|-----------|-----------|
| VDD | 3.3V |
| GND | GND |
| SDA | P0.26 (SDA bus) |
| SCL | P0.27 (SCL bus) |
| A0/A1/A2 | GND → address 0x18 |

### SPS30 — particulate matter (I2C 0x69)  ⚠ 5V power
| SPS30 pin (5-pin JST) | Connect to |
|-----------|-----------|
| Pin 1 VDD | **5V** (MB102 rail) |
| Pin 2 SDA | P0.26 (SDA bus) |
| Pin 3 SCL | P0.27 (SCL bus) |
| Pin 4 SEL | **GND** (selects I2C interface) |
| Pin 5 GND | GND |
> SPS30 SDA/SCL are 3.3V-logic compatible, so they connect directly to the DK.
> Only the **supply** must be 5V. SEL must be tied low **before** power-up.

### LDR (Iduino) — light / occupancy proxy (analog → AIN0)
Build a divider: `3.3V — LDR — (node A) — 10kΩ — GND`. Wire node **A** to **P0.02 (AIN0)**.
If you use the Iduino 3-pin module, its AO/signal pin goes to P0.02, VCC to 3.3V, GND to GND.

## I2C pull-ups
Add **one** pair of 4.7kΩ resistors from SDA→3.3V and SCL→3.3V. Many breakout
boards already include pull-ups; if several do, the bus may be slightly strong
but usually still works at 100 kHz. If the bus misbehaves, remove extra pull-ups.
