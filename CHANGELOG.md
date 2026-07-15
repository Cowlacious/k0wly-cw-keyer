# Changelog

All notable changes to the K0WLY Two-Way CW Keyer are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.2.0] — 2026 — Farnsworth Spacing

### Added
- **Firmware version** displayed in status area bottom right (K0WLY v1.2) — `FW_VERSION` define makes future updates a one-line change
- **Farnsworth spacing** — two-speed CW for learning
  - Characters sent at full character speed (fast, sounds like real code)
  - Gaps between characters and words stretched to a slower effective speed
  - Range: 4 WPM minimum effective speed, capped at character speed
  - When equal to character speed, Farnsworth is inactive
  - Header shows `25WPM` when equal, `25/8` (char/farnsworth) when active
- **Two-step WPM edit mode:**
  - Long press on WPM → enters CHAR SPEED step (pot sets character rate)
  - Short press → advances to FARNSWORTH step (pot sets effective rate)
  - Short press again → exits edit mode, saves both values

### Changed
- Internal timing split into `charDitLen_ms` (element duration) and `gapDitLen_ms` (gap duration)
- Word gap threshold uses gap speed dits — automatically scales with Farnsworth setting
- NVS settings version bumped to 3
- Word gap OFF zone widened to bottom 20% of pot travel for easier access

---

## [1.1.0] — 2026 — Word Gap and Display Updates

### Added
- **Word gap spacing** — adjustable word space insertion (OFF or 4–9 dits threshold)
  - GAP parameter added to pot mode cycle: WPM → FREQ → DELAY → VOL → GAP
  - First 20% of pot = OFF, remainder maps to 4–9 dit threshold
  - Spaces inserted on TX and RX lines when silence exceeds threshold
  - Saved to NVS with all other settings
- **K0WLY callsign** now displayed in status area bottom right (scale 2)

### Changed
- Header bar updated: K0WLY callsign removed from header to make room for GAP indicator
- GAP indicator shows GAP:OFF or GAP:4 through GAP:9, highlighted green when active
- NVS settings version bumped to 2

---

## [1.0.0] — 2026 — Initial Release

### Hardware
- LilyGO T-Display S3 AMOLED (ESP32-S3R8) platform
- PC817 optocoupler for radio key line isolation
- Dual 2N4401 NPN transistor audio circuit (speaker + headphones)
- Direct speaker connection — no coupling capacitor required
- 47µF bi-polar coupling cap on headphone output only

### Firmware
- Iambic Mode A keyer state machine on FreeRTOS Core 1
- ESP-NOW peer-to-peer auto-discovery (no router required)
- Two-way CW — transmit elements and decoded characters to peer
- Full Morse code decoding: A-Z, 0-9, punctuation, prosigns
- PARIS timing standard (ITU-R M.1677-1), 5–40 WPM
- Frame buffer display — instantaneous region updates, no glitch
- Single pot with short/long press mode cycling
- Logarithmic volume control via PWM duty cycle
- Head copy delay 0–3 seconds for incoming character display
- Non-volatile settings via ESP32 NVS (Preferences library)
- Straight key mode via SPST hardware switch (GPIO15)
- Paddle reverse via PBNO momentary button (GPIO16)
- Independent sidetone frequency — each unit sets its own pitch
- Radio keying output via PC817 optocoupler (GPIO12)

---

*73 de K0WLY*
