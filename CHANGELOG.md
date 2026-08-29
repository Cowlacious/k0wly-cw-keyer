# Changelog

All notable changes to the K0WLY Two-Way CW Keyer are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.2.1] — 2026 — Bug Fix: Receive Audio Not Playing

### Fixed
- **Incoming CW audio** was completely silent on receiving unit
  - LEDC channel 1 (remote) was never attached to PIN_SIDETONE — fixed by using LEDC channel 0 (local) for both local and remote playback since they never overlap
  - Inter-element gap between received elements corrected from hardcoded 10ms to proper timing

---

## [1.4.3] — 2026 — Bug Fix: Long File Playback

### Fixed
- **Long file playback stopping mid-file** — ring buffer wrap-around caused incorrect
  space calculation when fileElemHead wrapped past 255 back to 0, making the buffer
  appear full when it wasn't. Fixed with proper wrap-aware free space calculation.
- **Newlines in text files** now insert a word space on the TX line instead of
  being silently ignored — lines of text are now clearly separated during playback
- **Inter-element gap conflict** — separated inter-element gap (after keying ends)
  from char/word gap using dedicated fileElemGap flag, preventing double-gap issues
- Added playback watchdog to recover from any stuck playback state

---

## [1.4.3] — 2026 — File Playback Improvements

### Added
- **Pause/Resume** button on web page — instantly pauses file playback and resumes
  from approximately the same position (within one character)
- **Stop Playback** now truly immediate — flushes element buffer and stops audio instantly
- **Newlines** in text files insert a word space on TX line for cleaner display
- **Playback watchdog** recovers from any stuck playback state automatically

### Fixed
- **Long file playback stopping mid-file** — ring buffer wrap-around bug caused
  incorrect space calculation when fileElemHead wrapped past 255 back to 0.
  Fixed with proper wrap-aware free space calculation
- **Separate inter-element gap flag** (fileElemGap) prevents conflict with
  char/word gap timing that caused occasional playback stalls
- **Resume near end of file** — file is reopened correctly when pausing after
  the file has finished reading but buffer is still draining

---

## [1.4.2] — 2026 — Bug Fix: Iambic Output Logic

### Fixed
- **Iambic DIT/DAH output logic was inverted** — optocoupler circuit is active HIGH
  (GPIO HIGH = LED on = phototransistor conducts = radio keyed) not active LOW as originally coded
  - Added IAMBIC_ACTIVE and IAMBIC_INACTIVE macros for clarity
  - All iambic output writes updated to use correct logic
  - Pins now initialized LOW (inactive) at boot — radio no longer keys on startup
- Removed invalid RTC_CNTL_WDTCONFIG0_REG and esp_efuse calls that caused compile errors on ESP32-S3
- Removed unused esp_efuse includes

---

## [1.4.1] — 2026 — Bug Fix: Morse Decoding for 7, 8, 9

### Fixed
- **Morse decoder** — digits 7, 8, and 9 were mapped to incorrect positions in the binary decode tree
  - 7 (--...) corrected to position 55
  - 8 (---..) corrected to position 59
  - 9 (----.) corrected to position 61

---

## [1.4.0] — 2026 — Iambic Output and Web UI Redesign

### Added
- **Iambic DIT/DAH output** on GPIO40 and GPIO41
  - Active LOW, same optocoupler circuit as existing KEY OUT (GPIO12)
  - Connect to radio's 3.5mm paddle input (tip=DIT, ring=DAH, sleeve=GND)
  - Driven in sync with keyer ISR — exact timing matches actual keying
  - Also driven during file playback — radio transmits CW from practice files
  - Paddle reverse applies to outputs — swapping dit/dah swaps the outputs too
  - Always active — no switch needed, just plug in
- **Dit/Dah swap button** on web page (orange button at top)
  - Shows SWAPPED or NORMAL confirmation after tap
  - GPIO16 button now exclusively handles dit/dah swap (no file control)
- **Improved file list** on web page
  - Filenames shown without .txt extension
  - Underscores displayed as spaces (cq_call.txt → "cq call")
  - Compact Play/Delete buttons per file
  - Works cleanly with 10+ files

### Changed
- GPIO16 button simplified — only toggles dit/dah swap, no file control
- Web page redesigned — Practice Files section prominent, Upload moved to bottom
- File playback drives iambic outputs so radio transmits during practice

### Removed
- OTA firmware update section removed from web page (unreliable on Android)
- ESPAsyncHTTPUpdateServer library dependency removed

---

## [1.3.0] — 2026 — WiFi File Playback

### Added
- **WiFi Access Point** — keyer creates a unique hotspot (K0WLY-XXXX) on boot
  - Connect any phone, tablet, or computer — no password required
  - Browse to http://192.168.4.1 for the file management web page
  - Works on Android, iPhone, Windows, Mac, and Linux
- **Text file playback** — upload .txt files and play them as CW practice
  - Upload files from phone browser — no computer or cables needed
  - Files stored on-board in LittleFS flash filesystem
  - Multiple files supported — tap Play next to any file
  - Delete files from the web page
- **Synchronized TX display** — characters appear as they sound, not ahead
  - Each character displays at the character gap (after all elements played)
  - Word spaces display when the word gap silence plays
  - TX line clears automatically when a new file starts playing
- **GPIO16 button file control** (when files are present):
  - Long press — play/pause current file
  - Short press — cycle to next file (if multiple files loaded)
- **Unique unit ID** — last 4 hex digits of MAC shown on status area
  - Makes it easy to identify which unit is which
  - Matches the WiFi hotspot name (K0WLY-XXXX)
- **AP+STA simultaneous mode** — WiFi hotspot and ESP-NOW peer connection
  work at the same time on the same hardware

### Changed
- WiFi mode changed from WIFI_STA to WIFI_AP_STA
- AP SSID is now unique per unit (K0WLY-XXXX) instead of K0WLY-Keyer
- Status area now shows unit ID above callsign/version

---

## [1.2.3] — 2026 — Audio Only Mode

### Added
- **Audio Only (AO) mode** for head copy delay setting
  - Turn pot to top 10% of range while in DELAY edit mode to enable
  - Header shows `DLY:AO` when active
  - Status area shows `AUDIO ONLY` as the value
  - RX line shows `[Audio Only]` in dim text instead of decoded characters
  - Incoming CW audio plays normally — only the text display is suppressed
  - Perfect for operators who want pure audible head copy with no visual crutch

---

## [1.4.3] — 2026 — Bug Fix: Long File Playback

### Fixed
- **Long file playback stopping mid-file** — ring buffer wrap-around caused incorrect
  space calculation when fileElemHead wrapped past 255 back to 0, making the buffer
  appear full when it wasn't. Fixed with proper wrap-aware free space calculation.
- **Newlines in text files** now insert a word space on the TX line instead of
  being silently ignored — lines of text are now clearly separated during playback
- **Inter-element gap conflict** — separated inter-element gap (after keying ends)
  from char/word gap using dedicated fileElemGap flag, preventing double-gap issues
- Added playback watchdog to recover from any stuck playback state

---

## [1.4.3] — 2026 — File Playback Improvements

### Added
- **Pause/Resume** button on web page — instantly pauses file playback and resumes
  from approximately the same position (within one character)
- **Stop Playback** now truly immediate — flushes element buffer and stops audio instantly
- **Newlines** in text files insert a word space on TX line for cleaner display
- **Playback watchdog** recovers from any stuck playback state automatically

### Fixed
- **Long file playback stopping mid-file** — ring buffer wrap-around bug caused
  incorrect space calculation when fileElemHead wrapped past 255 back to 0.
  Fixed with proper wrap-aware free space calculation
- **Separate inter-element gap flag** (fileElemGap) prevents conflict with
  char/word gap timing that caused occasional playback stalls
- **Resume near end of file** — file is reopened correctly when pausing after
  the file has finished reading but buffer is still draining

---

## [1.4.2] — 2026 — Bug Fix: Iambic Output Logic

### Fixed
- **Iambic DIT/DAH output logic was inverted** — optocoupler circuit is active HIGH
  (GPIO HIGH = LED on = phototransistor conducts = radio keyed) not active LOW as originally coded
  - Added IAMBIC_ACTIVE and IAMBIC_INACTIVE macros for clarity
  - All iambic output writes updated to use correct logic
  - Pins now initialized LOW (inactive) at boot — radio no longer keys on startup
- Removed invalid RTC_CNTL_WDTCONFIG0_REG and esp_efuse calls that caused compile errors on ESP32-S3
- Removed unused esp_efuse includes

---

## [1.4.1] — 2026 — Bug Fix: Morse Decoding for 7, 8, 9

### Fixed
- **Morse decoder** — digits 7, 8, and 9 were mapped to incorrect positions in the binary decode tree
  - 7 (--...) corrected to position 55
  - 8 (---..) corrected to position 59
  - 9 (----.) corrected to position 61

---

## [1.4.0] — 2026 — Iambic Output and Web UI Redesign

### Added
- **Iambic DIT/DAH output** on GPIO40 and GPIO41
  - Active LOW, same optocoupler circuit as existing KEY OUT (GPIO12)
  - Connect to radio's 3.5mm paddle input (tip=DIT, ring=DAH, sleeve=GND)
  - Driven in sync with keyer ISR — exact timing matches actual keying
  - Also driven during file playback — radio transmits CW from practice files
  - Paddle reverse applies to outputs — swapping dit/dah swaps the outputs too
  - Always active — no switch needed, just plug in
- **Dit/Dah swap button** on web page (orange button at top)
  - Shows SWAPPED or NORMAL confirmation after tap
  - GPIO16 button now exclusively handles dit/dah swap (no file control)
- **Improved file list** on web page
  - Filenames shown without .txt extension
  - Underscores displayed as spaces (cq_call.txt → "cq call")
  - Compact Play/Delete buttons per file
  - Works cleanly with 10+ files

### Changed
- GPIO16 button simplified — only toggles dit/dah swap, no file control
- Web page redesigned — Practice Files section prominent, Upload moved to bottom
- File playback drives iambic outputs so radio transmits during practice

### Removed
- OTA firmware update section removed from web page (unreliable on Android)
- ESPAsyncHTTPUpdateServer library dependency removed

---

## [1.3.0] — 2026 — WiFi File Playback

### Added
- **WiFi Access Point** — keyer creates a unique hotspot (K0WLY-XXXX) on boot
  - Connect any phone, tablet, or computer — no password required
  - Browse to http://192.168.4.1 for the file management web page
  - Works on Android, iPhone, Windows, Mac, and Linux
- **Text file playback** — upload .txt files and play them as CW practice
  - Upload files from phone browser — no computer or cables needed
  - Files stored on-board in LittleFS flash filesystem
  - Multiple files supported — tap Play next to any file
  - Delete files from the web page
- **Synchronized TX display** — characters appear as they sound, not ahead
  - Each character displays at the character gap (after all elements played)
  - Word spaces display when the word gap silence plays
  - TX line clears automatically when a new file starts playing
- **GPIO16 button file control** (when files are present):
  - Long press — play/pause current file
  - Short press — cycle to next file (if multiple files loaded)
- **Unique unit ID** — last 4 hex digits of MAC shown on status area
  - Makes it easy to identify which unit is which
  - Matches the WiFi hotspot name (K0WLY-XXXX)
- **AP+STA simultaneous mode** — WiFi hotspot and ESP-NOW peer connection
  work at the same time on the same hardware

### Changed
- WiFi mode changed from WIFI_STA to WIFI_AP_STA
- AP SSID is now unique per unit (K0WLY-XXXX) instead of K0WLY-Keyer
- Status area now shows unit ID above callsign/version

---

## [1.2.3] — 2026 — Audio Only Mode

### Added
- **Audio Only (AO) mode** for head copy delay setting
  - Turn pot to top 10% of range while in DELAY edit mode to enable
  - Header shows `DLY:AO` when active
  - Status area shows `AUDIO ONLY` as the value
  - RX line shows `[Audio Only]` in dim orange when active
  - Incoming CW audio plays normally — only the text display is suppressed
  - Perfect for operators who want pure audible head copy with no visual crutch

---

## [1.2.2] — 2026 — Bug Fix: Received Audio Playback

### Fixed
- **Incoming CW audio** now plays at the sender's speed instead of the receiver's speed
  - Element durations are now transmitted as theoretical values (charDitLen_ms) rather than measured wall-clock time, eliminating timing jitter from the Core 0 loop
  - Inter-element gap on receiver correctly derived from received element duration
  - Audio no longer continues playing on receiver after sender has stopped
- Removed unused `elementStartMs` variable

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
