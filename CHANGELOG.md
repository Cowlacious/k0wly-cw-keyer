# Changelog

All notable changes to the K0WLY Two-Way CW Keyer are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.0.0] — 2025 — Initial Release

### Hardware
- LilyGO T-Display S3 AMOLED (ESP32-S3R8) platform
- PC817 optocoupler for radio key line isolation
- Dual 2N4401 NPN transistor audio circuit (speaker + headphones)
- Direct speaker connection — no coupling capacitor required
- 47µF bi-polar coupling cap on headphone output only
- 8-pin GPIO breakout for paddle, key out, sidetone, pot, switches

### Firmware
- Iambic Mode A keyer state machine on FreeRTOS Core 1
- ESP-NOW peer-to-peer auto-discovery (no router required)
- Two-way CW — transmit elements and decoded characters to peer
- Full Morse code decoding: A-Z, 0-9, punctuation, prosigns
- PARIS timing standard (ITU-R M.1677-1), 5–40 WPM
- Frame buffer display — instantaneous region updates, no glitch
- Single pot with short/long press mode cycling (WPM/FREQ/DELAY/VOL)
- Pot pickup mode — no value jump when switching parameters
- Logarithmic volume control via PWM duty cycle
- Head copy delay 0–3 seconds for incoming character display
- Non-volatile settings via ESP32 NVS (Preferences library)
- Straight key mode via SPST hardware switch (GPIO15)
- Paddle reverse via PBNO momentary button (GPIO16)
- Independent sidetone frequency — each unit sets its own pitch

### Display
- Header bar: callsign, WPM, Hz, DLY, VOL, SK/IAM, SOLO/DUAL
- TX line: outgoing decoded characters (green, scrolls left)
- RX line: incoming decoded characters (cyan, scrolls left)
- Status area: active pot mode, current value, peer MAC address

### Known Issues
- None at initial release

---

*73 de K0WLY*
