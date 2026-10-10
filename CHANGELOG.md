# Changelog

All notable changes to the K0WLY Two-Way CW Keyer are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.5.0] — 2026 — Ultimatic Keyer Mode

Adds an Ultimatic mode, contributed by UtahDave in
[PR #9](https://github.com/Cowlacious/k0wly-cw-keyer/pull/9) and merged onto
v1.4.10, with one change to how held paddles are remembered. Modes A and B
are unchanged. Tested on a host simulation and on a LilyGO unit by K0WLY (mode cycling,
squeeze priority, tap memory, release behavior).

### Added
- **Ultimatic keyer mode** — when both paddles are squeezed, the paddle pressed
  most recently takes over and repeats (no alternation). Releasing it returns to
  the paddle still held, or stops if none is. A new tap of the opposite paddle
  during an element is remembered and sent next. A paddle that is merely held
  down is not remembered, so letting go of both paddles stops the keyer after
  the current element (the letter A sends as A, not R). Source:
  [morsecode.world](https://morsecode.world/iambic.html).
  - GPIO16 long press now cycles A → B → U (Ultimatic) → A
  - Header shows A, B or U after the GAP setting

### Changed
- Keyer mode is stored in NVS as `kmode` (0=A, 1=B, 2=U) instead of the boolean
  `modeB`. Existing units keep their A/B setting on first boot.
- **Manuals** — User Guide and Manual updated to v1.5.0 with the Ultimatic mode;
  manual PDF regenerated.

### Upgrade notes
- Going back to an older firmware will not see a mode chosen in 1.5.0, because
  the old key is no longer updated. The older firmware starts in the A/B mode
  last saved before the upgrade.

---

## [1.4.10] — 2026 — Farnsworth Speed Now Means Overall Speed

### Changed
- **Farnsworth setting is now the overall (effective) speed.** Previously the
  second number set the speed of the gaps, so 20/10 played at about 14.5 WPM
  overall. It now follows the standard definition: characters are sent at
  character speed and the spacing is stretched so the word PARIS takes
  60000 / overall-speed ms. At 20/10 the gap unit is about 218 ms, ten
  repeats of "PARIS" take about 58.5 seconds, and the display shows `20/10`.
  Sources: [W6ZE](https://w6ze.org/btt/BTT054A.pdf),
  [morsecode.world](https://morsecode.world/international/timing/farnsworth.html).
- Changing the character speed with Farnsworth on now keeps the overall speed
  and recalculates the gaps. With Farnsworth off, the gaps follow the character
  speed as before.
- Manuals updated to v1.4.10 with the new wording.

### Upgrade notes
- Saved settings keep their exact timing, but a Farnsworth setting saved by an
  earlier version will now display its true overall speed (an old 20/10 shows
  as 20/14). Re-set Farnsworth if you want a specific overall speed.
- Not yet verified on hardware.

---

## [1.4.9] — 2026 — Fixes: Decoder, Encoder, Docs and Build Config

Cleanup release from a review of the firmware, README, build config and manuals.
The PlatformIO build and a `firmware.bin` have not been verified in this
environment; build with `pio run` before publishing a binary.

### Fixed
- **Underscore (`_`) was sent wrong from files** — the encoder table had the
  wrong pattern; it now sends `..--.-`.
- **`$` could not be decoded from the paddles** — the Morse decode tree held only
  127 entries, too small for the 7-element `...-..-`. It now holds 255 entries,
  and a 16-bit helper (`nextMorsePos`) prevents the position counter wrapping on
  over-long sequences. Longer sequences are discarded.
- **Stale comments** — volume default comment, header feature text and the
  embedded `platformio.ini` block now match the code.

### Changed
- **Build config** — removed the unused OTA library and `ELEGANTOTA` flag
  (OTA was removed in 1.4.0) from `platformio.ini`; deleted the stray
  `firmware/platform.ini/` folder.
- **README** — removed the obsolete OTA option, updated the `platformio.ini`
  block, build-from-source steps and repository structure.
- **Manuals** — User Guide and Manual updated to v1.4.9: default volume 64%
  (firmware default unchanged), volume range 2–100%, edit-mode short-press
  behaviour, and file line-break handling. The manual PDF is regenerated from
  the Word file (the previous PDF was an outdated v1.0 document).

---

## [1.4.8] — 2026 — Bug Fix: CW Timing (Farnsworth and File Playback)

Covers interim builds 1.4.6 and 1.4.7. Timing figures below come from code
analysis and have not yet been bench-verified on hardware.

### Fixed
- **File playback ran slow and choppy** — every dit/dah had two 1-unit gaps after
  it (the playback runner's post-element gap plus a second queued gap), doubling
  intra-character spacing. Character gap was 4 units and word gap 8 units.
  Playback now follows standard timing: 1 unit inside a character, 3 between
  characters, 7 between words. At 20 WPM it previously played at roughly 78% of
  the set speed.
- **Live keyer Farnsworth spacing was non-standard** — the gap after each dit/dah
  used the slow Farnsworth gap speed, stretching spacing inside characters.
  Intra-character gaps now use character speed; character gaps total 3 gap-units
  and word gaps N gap-units. With Farnsworth off, timing is unchanged.
- **Farnsworth setting was cancelled by raising character speed** — the clamp in
  the WPM edit code was inverted. The gap dit length is now kept at or above the
  character dit length.
- Added `effGapDit()` so the `3 * gap - charDit` gap math can never underflow if
  saved settings load with the gap faster than the characters.

### Changed
- **File playback line breaks** — a newline now adds a word gap only if the
  previous character was not already a space or newline. Blank lines, hard-wrapped
  text, `space + newline` and Windows `\r\n` endings no longer add extra pauses.
  Literal spaces are still always honored, so intentional extra spaces still
  lengthen the pause.
---

## [1.4.5] — 2026 — Bug Fix: Iambic Mode A/B Logic

### Fixed
- **Iambic Mode A was incorrectly implemented** — it was latching opposite paddle
  memory during active elements, which is Mode B behavior. Mode A correctly does
  NOT latch any memory during an active element; the opposite paddle must be
  pressed during the inter-element gap to register.
- **Mode B now correctly defined** — latches opposite paddle memory during active
  elements, allowing the next element to be queued early for smoother squeeze keying.
- The original firmware (before v1.4.4) was always behaving as Mode B regardless
  of the mode setting.
---

## [1.4.4] — 2026 — Iambic Mode A/B Selection

### Added
- **Iambic Mode A/B toggle** via GPIO16 long press (1 second)
  - Short press still toggles dit/dah swap as before
  - Header shows A or B after GAP setting
  - Mode saved to NVS and restored on power cycle
- **Header layout improved** — all items now flow left to right with consistent
  9px spacing, no fixed positions that cause overlap or large gaps
- **Straight key switch** now updates header display immediately when toggled

### Changed
- NVS version bumped to 4 — existing units reset to defaults on first boot
- Header shows A/B/SK instead of IAM/SK to fit all items without overlap
---

## [1.4.3] — 2026 — File Playback Improvements and Bug Fixes

### Added
- **Pause/Resume** button on web page — instantly pauses file playback and resumes
  from approximately the same position (within one character)
- **Stop Playback** now truly immediate — flushes element buffer and stops audio instantly
- **Newlines** in text files now insert a word space on the TX line instead of
  being silently ignored — lines of text are clearly separated during playback
- **Playback watchdog** recovers from any stuck playback state automatically

### Fixed
- **Long file playback stopping mid-file** — ring buffer wrap-around bug caused
  incorrect space calculation when fileElemHead wrapped past 255 back to 0, making
  the buffer appear full when it wasn't. Fixed with proper wrap-aware free space
  calculation
- **Inter-element gap conflict** — separated inter-element gap (after keying ends)
  from char/word gap using dedicated fileElemGap flag, preventing double-gap issues
  and occasional playback stalls
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

## [1.2.1] — 2026 — Bug Fix: Receive Audio Not Playing

### Fixed
- **Incoming CW audio** was completely silent on receiving unit
  - LEDC channel 1 (remote) was never attached to PIN_SIDETONE — fixed by using LEDC channel 0 (local) for both local and remote playback since they never overlap
  - Inter-element gap between received elements corrected from hardcoded 10ms to proper timing
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
