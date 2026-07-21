// ============================================================================
//  ESP32-S3 Two-Way CW Keyer — LilyGO T-Display S3 AMOLED 1.91" (RM67162)
//  K0WLY build  —  PlatformIO / Arduino framework
//  Version 1.2.1
//
//  Copyright © 2026 K0WLY (Carl Cowley)
//  Saratoga Springs, Utah — Grid Square DN40
//
//  Licensed under the CERN Open Hardware Licence Version 2 - Weakly Reciprocal
//  (CERN-OHL-W v2). You may redistribute and modify this work under the terms
//  of the CERN-OHL-W v2 (https://ohwr.org/cern_ohl_w_v2.txt).
//  Attribution to K0WLY must be retained on all copies and derivatives.
//
//  FEATURES:
//    - Iambic Mode A keyer with sidetone (or straight key via GPIO15 switch)
//    - ESP-NOW peer-to-peer WiFi (auto-discovery, no router needed)
//    - Two-way CW: outgoing keyed locally, transmitted to peer
//    - Incoming CW replayed with sender's frequency + shown on screen
//    - Head copy delay: incoming chars display delayed 0-3s after receipt
//    - Single pot cycles through WPM / FREQ / DELAY / VOL modes via button
//    - Logarithmic volume control (PWM duty), works for speaker and headphones
//    - Pot pickup mode: no value jump when switching pot modes
//    - Session frequency set by first station to send
//    - Display: header | TX scrolling line | RX scrolling line | status
//
//  platformio.ini:
//  ─────────────────────────────────────────────────────────────
//  [env:t_display_s3_amoled]
//  platform = espressif32
//  board = esp32-s3-devkitm-1
//  framework = arduino
//  board_build.mcu = esp32s3
//  board_build.f_cpu = 240000000L
//  board_build.flash_size = 16MB
//  board_build.flash_mode = dio
//  board_build.psram_type = opi
//  board_upload.flash_size = 16MB
//  monitor_speed = 115200
//  build_flags =
//      -DARDUINO_USB_CDC_ON_BOOT=1
//      -DBOARD_HAS_PSRAM
//  lib_deps =
//      https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series
//      https://github.com/moononournation/Arduino_GFX#v1.4.7
//  ─────────────────────────────────────────────────────────────
//
//  GPIO PIN ASSIGNMENTS (1.91" AMOLED — reserved: 2,3,5,6,7,9,17,18,21,38,47,48)
//  ┌──────────────────┬────────┬─────────────────────────────────────────┐
//  │ Function         │ GPIO   │ Notes                                   │
//  ├──────────────────┼────────┼─────────────────────────────────────────┤
//  │ DIT paddle       │ GPIO11 │ Active LOW, ext 10kΩ pull-up            │
//  │ DAH paddle       │ GPIO10 │ Active LOW, ext 10kΩ pull-up            │
//  │ KEY OUT          │ GPIO12 │ HIGH=keyed, drives PC817 optocoupler    │
//  │ SIDETONE         │ GPIO13 │ PWM → 470Ω → 2N4401 → 8Ω speaker       │
//  │ POT (ADC)        │ GPIO14 │ Wiper of 10kΩ pot (WPM/FREQ/DLY/VOL)  │
//  │ KEY MODE         │ GPIO15 │ LOW=straight key, open=iambic (pullup) │
//  │ PADDLE REVERSE   │ GPIO16 │ Active LOW momentary button             │
//  │ POT MODE SELECT  │ GPIO39 │ Active LOW momentary button             │
//  └──────────────────┴────────┴─────────────────────────────────────────┘
// ============================================================================

#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

Preferences prefs;

// Firmware version — update this whenever code changes
#define FW_VERSION "v1.2.1"

// ── Pin definitions ──────────────────────────────────────────────────────────
#define PIN_DIT         11
#define PIN_DAH         10
#define PIN_KEY_OUT     12
#define PIN_SIDETONE    13
#define PIN_POT         14
#define PIN_REVERSE     16
#define PIN_MODE_BTN    39
#define PIN_KEY_MODE    15   // LOW = straight key, HIGH (default) = iambic

// ── LEDC ─────────────────────────────────────────────────────────────────────
#define LEDC_CHANNEL_LOCAL  0
#define LEDC_CHANNEL_REMOTE 1
#define LEDC_RES_BITS       8
// SIDETONE_DUTY is now a variable (10-255) controlled by pot VOL mode
volatile uint8_t sidetone_duty = 128;  // ~50% default volume

// ── Keyer parameters ─────────────────────────────────────────────────────────
#define WPM_MIN         5
#define WPM_MAX         40
#define FREQ_MIN        400
#define FREQ_MAX        900
#define DELAY_MIN_MS    0
#define DELAY_MAX_MS    3000
#define GAP_MIN         0    // 0 = off
#define GAP_MAX         9    // max word gap threshold in dits

// ── Pot modes ────────────────────────────────────────────────────────────────
typedef enum { POT_WPM = 0, POT_FREQ, POT_DELAY, POT_VOL, POT_GAP, POT_MODE_COUNT } PotMode;
volatile PotMode potMode = POT_WPM;

// Pot pickup mode — prevents value jumping when switching modes
// All modes start picked up; pickup positions initialized in setup() from actual pot reading
int  potPickupRaw[POT_MODE_COUNT] = {2047, 2047, 2047, 2047, 2047};
bool potPickedUp[POT_MODE_COUNT]  = {true, true, true, true, true};
#define PICKUP_DEADBAND     300

// Edit mode — pot only changes values when in edit mode (long press to enter)
bool potEditMode = false;
uint8_t wpmEditStep = 0;  // 0=char speed, 1=Farnsworth speed (only used when potMode==POT_WPM)

// Long press detection
#define LONG_PRESS_MS       1000

// ── Runtime state ─────────────────────────────────────────────────────────────
volatile uint32_t charDitLen_ms   = 60;    // character speed dit length (20 WPM default)
volatile uint32_t gapDitLen_ms    = 60;    // gap speed dit length (Farnsworth, = char when off)
volatile uint32_t localFreq       = 700;   // local sidetone Hz — each unit sets its own
volatile uint32_t headCopyDelayMs = 0;     // incoming char display delay
volatile uint8_t  wordGapDits     = 0;     // 0=off, 4-9 = word gap threshold in dits
volatile bool     paddleReverse   = false;
volatile bool     straightKey     = false; // false = iambic, true = straight key

// Convenience: character WPM and Farnsworth effective WPM
static inline uint32_t charWPM() { return 1200 / charDitLen_ms; }
static inline uint32_t farnWPM() { return 1200 / gapDitLen_ms; }

// ── Keyer state machine ───────────────────────────────────────────────────────
typedef enum {
    KEYER_IDLE,
    KEYER_DIT, KEYER_DIT_GAP,
    KEYER_DAH, KEYER_DAH_GAP,
    KEYER_CHAR_GAP, KEYER_WORD_GAP
} KeyerState;

volatile KeyerState keyerState   = KEYER_IDLE;
volatile uint32_t   elementTimer = 0;
volatile bool       ditMemory    = false;
volatile bool       dahMemory    = false;

// ── Morse decoder ─────────────────────────────────────────────────────────────
// Binary tree: dit = pos*2+1, dah = pos*2+2
// 127 entries (7 levels) covering letters, numbers, punctuation, and prosigns
// Prosigns: + = AR (end of message), = BT (break/paragraph), ~ = SK (end of contact)
static const char morseTree[] = {
    ' ','E','T','I','A','N','M','S','U','R',  // 0-9
    'W','D','K','G','O','H','V','F',' ','L',  // 10-19
    ' ','P','J','B','X','C','Y','Z','Q',' ',  // 20-29
    ' ','5','4','~','3',' ',' ',' ','2','&',  // 30-39  (~=SK prosign, &=AS prosign)
    ' ','+',' ',' ',' ',' ','1','6','=','/', // 40-49  (+=AR prosign, ==BT prosign)
    ' ',' ','7','(',' ',' ','8',' ','9',' ', // 50-59
    ' ',' ','0',' ',' ',' ',' ',' ',' ',' ', // 60-69
    ' ',' ',' ',' ',' ','?','_',' ',' ',' ', // 70-79
    ' ','"',' ',' ','.',' ',' ',' ',' ','@', // 80-89
    ' ',' ',' ','\'',' ',' ','-',' ',' ',' ',// 90-99
    ' ',' ',' ',' ',' ',';','!',' ',')',' ', // 100-109
    ' ',' ',' ',' ',',',' ',' ',' ',' ',':', // 110-119
    ' ',' ',' ',' ',' ',' ',' '             // 120-126
};
volatile uint8_t morsePos = 0;

// ── Outgoing decoded char buffer ──────────────────────────────────────────────
#define OUT_BUF_SIZE 32
volatile char    outBuf[OUT_BUF_SIZE];
volatile uint8_t outBufHead = 0;
volatile uint8_t outBufTail = 0;

// ── ESP-NOW ───────────────────────────────────────────────────────────────────
// Packet types
#define PKT_DISCOVER    0x01  // broadcast: "I'm here, here's my MAC"
#define PKT_DISCOVER_ACK 0x02 // reply to discover
#define PKT_ELEMENT     0x03  // a dit or dah element (with timing + freq)
#define PKT_CHAR        0x04  // decoded character

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  isDah;       // PKT_ELEMENT: 0=dit, 1=dah
    uint32_t durationMs;  // PKT_ELEMENT: element duration
    char     ch;          // PKT_CHAR: the decoded character
} CWPacket;

static uint8_t broadcastAddr[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint8_t peerAddr[6]      = {0};
static bool    peerFound        = false;
static bool    peerRegistered   = false;

// Incoming element queue (for replaying received CW)
#define ELEM_BUF_SIZE 32
typedef struct {
    bool     isDah;
    uint32_t durationMs;
} RemoteElement;
static RemoteElement elemBuf[ELEM_BUF_SIZE];
static volatile uint8_t elemHead = 0;
static volatile uint8_t elemTail = 0;

// Incoming char delay queue
#define INCOMING_BUF_SIZE 32
typedef struct {
    char     ch;
    uint32_t showAtMs;  // millis() when to display
} DelayedChar;
static DelayedChar incomingBuf[INCOMING_BUF_SIZE];
static volatile uint8_t incomingHead = 0;
static volatile uint8_t incomingTail = 0;

// Ready-to-display incoming chars
#define READY_BUF_SIZE 32
volatile char    readyBuf[READY_BUF_SIZE];
volatile uint8_t readyHead = 0;
volatile uint8_t readyTail = 0;

// ── Hardware timer ISR ────────────────────────────────────────────────────────

static void keyer_isr() {
    bool dit_p = (digitalRead(PIN_DIT) == LOW);
    bool dah_p = (digitalRead(PIN_DAH) == LOW);

    // Straight key mode: DIT paddle = key down, ignore DAH and state machine
    if (straightKey) {
        if (dit_p) {
            digitalWrite(PIN_KEY_OUT, HIGH);
            ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
        } else {
            digitalWrite(PIN_KEY_OUT, LOW);
            ledcWrite(LEDC_CHANNEL_LOCAL, 0);
        }
        keyerState = KEYER_IDLE;
        ditMemory  = false;
        dahMemory  = false;
        return;
    }

    if (paddleReverse) { bool t = dit_p; dit_p = dah_p; dah_p = t; }

    // Iambic memory latching:
    // During active element: only latch OPPOSITE paddle (prevents same-element double-fire)
    // During gaps/idle: latch both paddles freely (enables auto-repeat and squeeze keying)
    if (keyerState == KEYER_DIT) {
        if (dah_p) dahMemory = true;
    } else if (keyerState == KEYER_DAH) {
        if (dit_p) ditMemory = true;
    } else {
        if (dit_p) ditMemory = true;
        if (dah_p) dahMemory = true;
    }

    switch (keyerState) {
        case KEYER_IDLE:
            morsePos = 0;
            if (ditMemory) {
                ditMemory = false;
                dahMemory = false;
                morsePos = 1;
                ledcSetup(LEDC_CHANNEL_LOCAL, localFreq, LEDC_RES_BITS);
                ledcAttachPin(PIN_SIDETONE, LEDC_CHANNEL_LOCAL);
                digitalWrite(PIN_KEY_OUT, HIGH);
                ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
                elementTimer = charDitLen_ms;        // character speed
                keyerState = KEYER_DIT;
            } else if (dahMemory) {
                ditMemory = false;
                dahMemory = false;
                morsePos = 2;
                ledcSetup(LEDC_CHANNEL_LOCAL, localFreq, LEDC_RES_BITS);
                ledcAttachPin(PIN_SIDETONE, LEDC_CHANNEL_LOCAL);
                digitalWrite(PIN_KEY_OUT, HIGH);
                ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
                elementTimer = charDitLen_ms * 3;    // character speed
                keyerState = KEYER_DAH;
            }
            break;

        case KEYER_DIT:
            if (--elementTimer == 0) {
                digitalWrite(PIN_KEY_OUT, LOW);
                ledcWrite(LEDC_CHANNEL_LOCAL, 0);
                elementTimer = gapDitLen_ms;         // gap speed (Farnsworth)
                keyerState = KEYER_DIT_GAP;
            }
            break;

        case KEYER_DIT_GAP:
            if (--elementTimer == 0) {
                if (dahMemory) {
                    dahMemory = false;
                    ditMemory = false;
                    morsePos = morsePos * 2 + 2;
                    if (morsePos >= sizeof(morseTree)) morsePos = 0;
                    digitalWrite(PIN_KEY_OUT, HIGH);
                    ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
                    elementTimer = charDitLen_ms * 3; // character speed
                    keyerState = KEYER_DAH;
                } else if (ditMemory) {
                    ditMemory = false;
                    dahMemory = false;
                    morsePos = morsePos * 2 + 1;
                    if (morsePos >= sizeof(morseTree)) morsePos = 0;
                    digitalWrite(PIN_KEY_OUT, HIGH);
                    ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
                    elementTimer = charDitLen_ms;     // character speed
                    keyerState = KEYER_DIT;
                } else {
                    elementTimer = gapDitLen_ms * 2;  // gap speed (Farnsworth)
                    keyerState = KEYER_CHAR_GAP;
                }
            }
            break;

        case KEYER_DAH:
            if (--elementTimer == 0) {
                digitalWrite(PIN_KEY_OUT, LOW);
                ledcWrite(LEDC_CHANNEL_LOCAL, 0);
                elementTimer = gapDitLen_ms;          // gap speed (Farnsworth)
                keyerState = KEYER_DAH_GAP;
            }
            break;

        case KEYER_DAH_GAP:
            if (--elementTimer == 0) {
                if (ditMemory) {
                    ditMemory = false;
                    dahMemory = false;
                    morsePos = morsePos * 2 + 1;
                    if (morsePos >= sizeof(morseTree)) morsePos = 0;
                    digitalWrite(PIN_KEY_OUT, HIGH);
                    ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
                    elementTimer = charDitLen_ms;      // character speed
                    keyerState = KEYER_DIT;
                } else if (dahMemory) {
                    dahMemory = false;
                    ditMemory = false;
                    morsePos = morsePos * 2 + 2;
                    if (morsePos >= sizeof(morseTree)) morsePos = 0;
                    digitalWrite(PIN_KEY_OUT, HIGH);
                    ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
                    elementTimer = charDitLen_ms * 3;  // character speed
                    keyerState = KEYER_DAH;
                } else {
                    elementTimer = gapDitLen_ms * 2;   // gap speed (Farnsworth)
                    keyerState = KEYER_CHAR_GAP;
                }
            }
            break;

        case KEYER_CHAR_GAP:
            if (--elementTimer == 0) {
                char decoded = ' ';
                if (morsePos > 0 && morsePos < (uint8_t)sizeof(morseTree)) {
                    decoded = morseTree[morsePos];
                }
                uint8_t nextHead = (outBufHead + 1) % OUT_BUF_SIZE;
                if (nextHead != outBufTail) {
                    outBuf[outBufHead] = decoded;
                    outBufHead = nextHead;
                }
                morsePos = 0;
                if (wordGapDits >= 4) {
                    elementTimer = gapDitLen_ms * (wordGapDits - 3); // gap speed
                    keyerState = KEYER_WORD_GAP;
                } else {
                    keyerState = KEYER_IDLE;
                }
            }
            break;

        case KEYER_WORD_GAP:
            if (ditMemory || dahMemory) {
                keyerState = KEYER_IDLE;
                break;
            }
            if (--elementTimer == 0) {
                uint8_t nextHead = (outBufHead + 1) % OUT_BUF_SIZE;
                if (nextHead != outBufTail) {
                    outBuf[outBufHead] = ' ';
                    outBufHead = nextHead;
                }
                keyerState = KEYER_IDLE;
            }
            break;
    }
}

// ── ESP-NOW callbacks ─────────────────────────────────────────────────────────
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    // could log failures here
}

static void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < (int)sizeof(CWPacket)) return;
    CWPacket pkt;
    memcpy(&pkt, data, sizeof(CWPacket));

    if (pkt.type == PKT_DISCOVER || pkt.type == PKT_DISCOVER_ACK) {
        // Register peer if not already done
        if (!peerFound) {
            memcpy(peerAddr, mac, 6);
            peerFound = true;
        }
        // Send ACK if this was a discover (not an ACK itself)
        if (pkt.type == PKT_DISCOVER) {
            CWPacket ack;
            memset(&ack, 0, sizeof(ack));
            ack.type = PKT_DISCOVER_ACK;
            esp_now_send(const_cast<uint8_t*>(mac), (uint8_t*)&ack, sizeof(ack));
        }
        return;
    }

    if (pkt.type == PKT_ELEMENT) {
        // Queue element for local replay at our own frequency
        uint8_t next = (elemHead + 1) % ELEM_BUF_SIZE;
        if (next != elemTail) {
            elemBuf[elemHead] = { pkt.isDah != 0, pkt.durationMs };
            elemHead = next;
        }
        return;
    }

    if (pkt.type == PKT_CHAR) {
        // Queue char with delay
        uint8_t next = (incomingHead + 1) % INCOMING_BUF_SIZE;
        if (next != incomingTail) {
            incomingBuf[incomingHead] = { pkt.ch, (uint32_t)(millis() + headCopyDelayMs) };
            incomingHead = next;
        }
        return;
    }
}

// ── Register peer with ESP-NOW ────────────────────────────────────────────────
static void registerPeer(const uint8_t *mac) {
    if (peerRegistered) return;
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    peerRegistered = true;
}

// ── Send a keyed element to peer ──────────────────────────────────────────────
static void sendElement(bool isDah, uint32_t durationMs) {
    if (!peerFound) return;
    registerPeer(peerAddr);
    CWPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type       = PKT_ELEMENT;
    pkt.isDah      = isDah ? 1 : 0;
    pkt.durationMs = durationMs;
    esp_now_send(peerAddr, (uint8_t*)&pkt, sizeof(pkt));
}

static void sendChar(char c) {
    if (!peerFound) return;
    registerPeer(peerAddr);
    CWPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = PKT_CHAR;
    pkt.ch   = c;
    esp_now_send(peerAddr, (uint8_t*)&pkt, sizeof(pkt));
}

// ── Display ───────────────────────────────────────────────────────────────────
LilyGo_Class amoled;

static inline uint16_t swapBytes(uint16_t c) { return (c >> 8) | (c << 8); }

static const uint16_t C_BLACK    = 0x0000;
static const uint16_t C_GREEN    = swapBytes(0x07E0);
static const uint16_t C_YELLOW   = swapBytes(0xFFE0);
static const uint16_t C_CYAN     = swapBytes(0x07FF);
static const uint16_t C_ORANGE   = swapBytes(0xFD20);
static const uint16_t C_DARKGRAY = swapBytes(0x2104);
static const uint16_t C_WHITE    = swapBytes(0xFFFF);
static const uint16_t C_MAGENTA  = swapBytes(0xF81F);
static const uint16_t C_LTBLUE   = swapBytes(0x051F);

#define SCR_W   536
#define SCR_H   240

// ── Frame buffer ──────────────────────────────────────────────────────────────
// Full 536×240 frame buffer in PSRAM — all drawing goes here first,
// then flushRect() pushes only the changed region to the display in one shot.
// This makes updates appear instantaneous with no visible redraw artifacts.
static uint16_t *frameBuf = nullptr;  // SCR_W * SCR_H * 2 bytes = ~257KB in PSRAM

// Pixel address in frame buffer
static inline uint16_t& fbPix(uint16_t x, uint16_t y) {
    return frameBuf[y * SCR_W + x];
}

// Draw a filled rectangle into the frame buffer (no display update yet)
void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (!frameBuf || w == 0 || h == 0) return;
    if (x >= SCR_W || y >= SCR_H) return;
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    for (uint16_t row = y; row < y + h; row++) {
        uint16_t *p = &frameBuf[row * SCR_W + x];
        for (uint16_t col = 0; col < w; col++) *p++ = color;
    }
}

// Push a rectangular region from frame buffer to display — fast single transfer
void flushRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!frameBuf || w == 0 || h == 0) return;
    if (x >= SCR_W || y >= SCR_H) return;
    if (x + w > SCR_W) w = SCR_W - x;
    if (y + h > SCR_H) h = SCR_H - y;
    for (uint16_t row = y; row < y + h; row++) {
        amoled.pushColors(x, row, w, 1, &frameBuf[row * SCR_W + x]);
    }
}

// Flush the entire screen
void flushAll() {
    flushRect(0, 0, SCR_W, SCR_H);
}

// Flush a horizontal band (useful for header, status, TX line, RX line)
void flushBand(uint16_t y, uint16_t h) {
    flushRect(0, y, SCR_W, h);
}

// ── 5×7 font ─────────────────────────────────────────────────────────────────
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x08,0x2A,0x1C,0x2A,0x08}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x00,0x08,0x14,0x22,0x41}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x41,0x22,0x14,0x08,0x00}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x08,0x14,0x54,0x54,0x3C}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x40,0x3C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x08,0x04,0x08,0x10,0x08}, // '~'
};

void drawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *g = font5x7[c - 32];
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t cd = g[col];
        for (uint8_t row = 0; row < 7; row++) {
            fillRect(x + col*scale, y + row*scale, scale, scale,
                     (cd & (1<<row)) ? fg : bg);
        }
    }
    fillRect(x + 5*scale, y, scale, 7*scale, bg);
}

void drawString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*str) {
        if (x + 6*scale > SCR_W) break;
        drawChar(x, y, *str++, fg, bg, scale);
        x += 6*scale;
    }
}

// ── UI Layout ─────────────────────────────────────────────────────────────────
//  y=0   h=32  Header bar (small text, scale 2)
//  y=32  h=2   Divider
//  y=36  h=16  "TX>" label area
//  y=34  h=50  Outgoing text line (large, scale 4 = 24x28px)
//  y=86  h=2   Divider
//  y=90  h=16  "RX>" label area
//  y=90  h=50  Incoming text line (large, scale 4)
//  y=142 h=2   Divider
//  y=145 h=95  Status / pot mode indicator

#define HEADER_Y        0
#define HEADER_H        32
#define DIV1_Y          32
#define TX_LABEL_Y      36
#define TX_LINE_Y       52
#define TX_LINE_H       28   // scale 4 char height
#define DIV2_Y          86
#define RX_LABEL_Y      90
#define RX_LINE_Y       106
#define RX_LINE_H       28
#define DIV3_Y          140
#define STATUS_Y        144
#define STATUS_H        96

// Large char scale for TX/RX lines
#define BIG_SCALE       4
#define BIG_CHAR_W      (6 * BIG_SCALE)   // 24px
#define BIG_CHAR_H      (7 * BIG_SCALE)   // 28px
#define BIG_CHARS       ((SCR_W - 8) / BIG_CHAR_W)  // ~22 chars

// Scrolling line buffers
static char txLine[BIG_CHARS + 1];
static char rxLine[BIG_CHARS + 1];

void drawDivider(uint16_t y) {
    fillRect(0, y, SCR_W, 2, C_DARKGRAY);
}

void drawHeader() {
    fillRect(0, HEADER_Y, SCR_W, HEADER_H, C_DARKGRAY);

    uint16_t x = 4;
    char tmp[16];

    // WPM — show char/farn when Farnsworth active, just WPM when equal
    if (charDitLen_ms == gapDitLen_ms) {
        snprintf(tmp, sizeof(tmp), "%luWPM", charWPM());
    } else {
        snprintf(tmp, sizeof(tmp), "%lu/%lu", charWPM(), farnWPM());
    }
    uint16_t wpmColor = (potMode == POT_WPM) ? C_YELLOW : C_WHITE;
    drawString(x, 8, tmp, wpmColor, C_DARKGRAY, 2);
    x += strlen(tmp) * 12 + 8;

    // FREQ
    snprintf(tmp, sizeof(tmp), "%luHz", localFreq);
    uint16_t freqColor = (potMode == POT_FREQ) ? C_CYAN : C_WHITE;
    drawString(x, 8, tmp, freqColor, C_DARKGRAY, 2);
    x += strlen(tmp) * 12 + 8;

    // DELAY
    snprintf(tmp, sizeof(tmp), "DLY:%.1fs", headCopyDelayMs / 1000.0f);
    uint16_t dlyColor = (potMode == POT_DELAY) ? C_ORANGE : C_WHITE;
    drawString(x, 8, tmp, dlyColor, C_DARKGRAY, 2);
    x += strlen(tmp) * 12 + 8;

    // VOL
    snprintf(tmp, sizeof(tmp), "VOL:%d%%", (sidetone_duty * 100) / 200);
    uint16_t volColor = (potMode == POT_VOL) ? C_MAGENTA : C_WHITE;
    drawString(x, 8, tmp, volColor, C_DARKGRAY, 2);
    x += strlen(tmp) * 12 + 8;

    // GAP
    if (wordGapDits == 0) {
        snprintf(tmp, sizeof(tmp), "GAP:OFF");
    } else {
        snprintf(tmp, sizeof(tmp), "GAP:%d", wordGapDits);
    }
    uint16_t gapColor = (potMode == POT_GAP) ? C_GREEN : C_WHITE;
    drawString(x, 8, tmp, gapColor, C_DARKGRAY, 2);

    // SK/IAM — right side
    const char *keyModeStr = straightKey ? "SK" : "IAM";
    drawString(SCR_W - 108, 8, keyModeStr, C_WHITE, C_DARKGRAY, 2);

    // SOLO/DUAL — far right
    const char *modeStr = peerFound ? "DUAL" : "SOLO";
    uint16_t modeColor = peerFound ? C_GREEN : C_WHITE;
    drawString(SCR_W - 60, 8, modeStr, modeColor, C_DARKGRAY, 2);

    flushBand(HEADER_Y, HEADER_H);
}

void drawTXLabel() {
    fillRect(0, TX_LABEL_Y, 50, 16, C_BLACK);
    drawString(2, TX_LABEL_Y, "TX>", C_GREEN, C_BLACK, 2);
    flushRect(0, TX_LABEL_Y, 50, 16);
}

void drawRXLabel() {
    fillRect(0, RX_LABEL_Y, 50, 16, C_BLACK);
    drawString(2, RX_LABEL_Y, "RX>", C_CYAN, C_BLACK, 2);
    flushRect(0, RX_LABEL_Y, 50, 16);
}

void drawTXLine() {
    fillRect(0, TX_LINE_Y, SCR_W, BIG_CHAR_H, C_BLACK);
    drawString(4, TX_LINE_Y, txLine, C_GREEN, C_BLACK, BIG_SCALE);
    flushBand(TX_LINE_Y, BIG_CHAR_H);
}

void drawRXLine() {
    fillRect(0, RX_LINE_Y, SCR_W, BIG_CHAR_H, C_BLACK);
    drawString(4, RX_LINE_Y, rxLine, C_CYAN, C_BLACK, BIG_SCALE);
    flushBand(RX_LINE_Y, BIG_CHAR_H);
}

void drawStatusArea() {
    fillRect(0, STATUS_Y, SCR_W, STATUS_H, C_BLACK);

    const char *modeLabel = "";
    uint16_t modeColor = C_WHITE;
    char valStr[32];

    switch (potMode) {
        case POT_WPM:
            modeColor = C_YELLOW;
            if (!potEditMode) {
                modeLabel = "POT: WPM";
                if (charDitLen_ms == gapDitLen_ms)
                    snprintf(valStr, sizeof(valStr), "%lu WPM", charWPM());
                else
                    snprintf(valStr, sizeof(valStr), "%lu / %lu WPM", charWPM(), farnWPM());
            } else if (wpmEditStep == 0) {
                modeLabel = "CHAR SPEED";
                snprintf(valStr, sizeof(valStr), "%lu WPM", charWPM());
            } else {
                modeLabel = "FARNSWORTH";
                snprintf(valStr, sizeof(valStr), "%lu WPM", farnWPM());
            }
            break;
        case POT_FREQ:
            modeLabel = "POT: FREQ";
            modeColor = C_CYAN;
            snprintf(valStr, sizeof(valStr), "%lu Hz", localFreq);
            break;
        case POT_DELAY:
            modeLabel = "POT: DELAY";
            modeColor = C_ORANGE;
            snprintf(valStr, sizeof(valStr), "%.1f sec", headCopyDelayMs / 1000.0f);
            break;
        case POT_VOL:
            modeLabel = "POT: VOLUME";
            modeColor = C_MAGENTA;
            snprintf(valStr, sizeof(valStr), "%d%%", (sidetone_duty * 100) / 200);
            break;
        case POT_GAP:
            modeLabel = "POT: WORD GAP";
            modeColor = C_GREEN;
            if (wordGapDits == 0)
                snprintf(valStr, sizeof(valStr), "OFF");
            else
                snprintf(valStr, sizeof(valStr), "%d dits", wordGapDits);
            break;
        default: break;
    }

    drawString(4, STATUS_Y + 4, modeLabel, modeColor, C_BLACK, 2);

    if (potEditMode) {
        drawString(230, STATUS_Y + 4, "[EDIT]", C_WHITE, C_BLACK, 2);
    } else {
        fillRect(230, STATUS_Y + 4, 150, 16, C_BLACK);
    }

    drawString(4, STATUS_Y + 30, valStr, C_WHITE, C_BLACK, 3);

    // Peer status bottom left
    if (peerFound) {
        char peerStr[32];
        snprintf(peerStr, sizeof(peerStr), "PEER: %02X:%02X:%02X",
                 peerAddr[3], peerAddr[4], peerAddr[5]);
        drawString(4, STATUS_Y + 68, peerStr, C_GREEN, C_BLACK, 1);
    } else {
        drawString(4, STATUS_Y + 68, "SEARCHING...", C_DARKGRAY, C_BLACK, 1);
    }

    // K0WLY callsign + firmware version — bottom right, scale 2
    char callStr[16];
    snprintf(callStr, sizeof(callStr), "K0WLY %s", FW_VERSION);
    uint16_t callX = SCR_W - (strlen(callStr) * 12) - 4;
    drawString(callX, STATUS_Y + 62, callStr, C_GREEN, C_BLACK, 2);

    flushBand(STATUS_Y, STATUS_H);
}

// Add char to TX scrolling line
void addTXChar(char c) {
    if (c == ' ' && strlen(txLine) == 0) return;
    uint8_t len = strlen(txLine);
    if (len >= BIG_CHARS) {
        // Shift left
        memmove(txLine, txLine + 1, BIG_CHARS - 1);
        txLine[BIG_CHARS - 1] = c;
    } else {
        txLine[len]     = c;
        txLine[len + 1] = '\0';
    }
    drawTXLine();
}

// Add char to RX scrolling line
void addRXChar(char c) {
    if (c == ' ' && strlen(rxLine) == 0) return;
    uint8_t len = strlen(rxLine);
    if (len >= BIG_CHARS) {
        memmove(rxLine, rxLine + 1, BIG_CHARS - 1);
        rxLine[BIG_CHARS - 1] = c;
    } else {
        rxLine[len]     = c;
        rxLine[len + 1] = '\0';
    }
    drawRXLine();
}

// ── Full UI draw ──────────────────────────────────────────────────────────────
// Draws entire UI to frame buffer then pushes to display in one shot
void drawUI() {
    fillRect(0, 0, SCR_W, SCR_H, C_BLACK);
    drawHeader();        // draws to buf + flushes header band
    fillRect(0, DIV1_Y, SCR_W, 2, C_DARKGRAY);
    flushRect(0, DIV1_Y, SCR_W, 2);
    drawTXLabel();       // draws to buf + flushes label
    drawTXLine();        // draws to buf + flushes TX line
    fillRect(0, DIV2_Y, SCR_W, 2, C_DARKGRAY);
    flushRect(0, DIV2_Y, SCR_W, 2);
    drawRXLabel();       // draws to buf + flushes label
    drawRXLine();        // draws to buf + flushes RX line
    fillRect(0, DIV3_Y, SCR_W, 2, C_DARKGRAY);
    flushRect(0, DIV3_Y, SCR_W, 2);
    drawStatusArea();    // draws to buf + flushes status area
}

// ── Remote element playback state ─────────────────────────────────────────────
static bool     remoteKeying    = false;
static uint32_t remoteKeyEndMs  = 0;
static uint32_t remoteGapEndMs  = 0;
static uint32_t remoteInterGapMs = 60;  // estimated inter-element gap in ms
static bool     remoteInGap     = false;

void serviceRemoteElements() {
    uint32_t now = millis();

    if (remoteKeying) {
        if (now >= remoteKeyEndMs) {
            // Element done — stop tone, start inter-element gap
            ledcWrite(LEDC_CHANNEL_LOCAL, 0);
            remoteKeying   = false;
            remoteInGap    = true;
            // Inter-element gap = same duration as a dit at the received speed
            // We approximate from the element duration stored
            remoteGapEndMs = now + remoteInterGapMs;
        }
        return;
    }

    if (remoteInGap) {
        if (now < remoteGapEndMs) return;
        remoteInGap = false;
    }

    // Pull next element
    if (elemTail == elemHead) return;
    RemoteElement &el = elemBuf[elemTail];
    elemTail = (elemTail + 1) % ELEM_BUF_SIZE;

    // Calculate inter-element gap from element duration
    // dit = 1 unit, dah = 3 units, so dit = dah/3
    // gap = 1 unit = dah/3 or dit itself
    remoteInterGapMs = el.isDah ? (el.durationMs / 3) : el.durationMs;

    // Play on local LEDC channel at our frequency
    ledcSetup(LEDC_CHANNEL_LOCAL, localFreq, LEDC_RES_BITS);
    ledcAttachPin(PIN_SIDETONE, LEDC_CHANNEL_LOCAL);
    ledcWrite(LEDC_CHANNEL_LOCAL, sidetone_duty);
    remoteKeying   = true;
    remoteKeyEndMs = now + el.durationMs;
}

// ── ADC read helpers ──────────────────────────────────────────────────────────
// Average 8 samples to reduce ESP32-S3 ADC noise
static int readPotSmoothed() {
    int sum = 0;
    for (int i = 0; i < 8; i++) sum += analogRead(PIN_POT);
    return sum / 8;
}

static uint32_t readWPM() {
    int raw = readPotSmoothed();
    return WPM_MIN + (uint32_t)((raw / 4095.0f) * (WPM_MAX - WPM_MIN));
}

static uint32_t readFreq() {
    int raw = readPotSmoothed();
    return FREQ_MIN + (uint32_t)((raw / 4095.0f) * (FREQ_MAX - FREQ_MIN));
}

static uint8_t readVol() {
    int raw = readPotSmoothed();
    // Logarithmic curve for perceptually linear volume
    // Maps 0-4095 to duty 5-200 (hard cap at 200 to avoid silent DC region)
    float normalized = raw / 4095.0f;
    float logVal = log10f(1.0f + normalized * 9.0f);
    uint8_t duty = (uint8_t)(5.0f + logVal * 195.0f);
    if (duty > 200) duty = 200;
    return duty;
}

static uint32_t readDelay() {
    int raw = analogRead(PIN_POT);
    uint32_t ms = (uint32_t)((raw / 4095.0f) * DELAY_MAX_MS);
    return (ms / 100) * 100;
}

static uint8_t readGap() {
    int raw = readPotSmoothed();
    // Bottom 20% of pot = OFF (clear dead zone, easy to find)
    // Top 80% maps to 4-9 dits in integer steps
    if (raw < 820) return 0;  // OFF — bottom 20%
    // Map 820-4095 to 4-9 (6 steps)
    uint8_t val = (uint8_t)(4 + ((raw - 820) / (float)(4095 - 820)) * (GAP_MAX - 4));
    if (val > GAP_MAX) val = GAP_MAX;
    return val;
}

// ── NVS save/load ─────────────────────────────────────────────────────────────
void saveSettings() {
    prefs.begin("keyer", false);
    prefs.putUInt("charDit",  charDitLen_ms);
    prefs.putUInt("gapDit",   gapDitLen_ms);
    prefs.putUInt("freq",     localFreq);
    prefs.putUInt("delay",    headCopyDelayMs);
    prefs.putUChar("vol",     sidetone_duty);
    prefs.putUChar("gap",     wordGapDits);
    prefs.end();
}

void loadSettings() {
    prefs.begin("keyer", true);  // read-only
    uint8_t ver = prefs.getUChar("ver", 0);
    prefs.end();

    if (ver < 3) {
        charDitLen_ms   = 60;   // 20 WPM
        gapDitLen_ms    = 60;   // no Farnsworth
        localFreq       = 700;
        headCopyDelayMs = 0;
        sidetone_duty   = 128;
        wordGapDits     = 0;
        prefs.begin("keyer", false);
        prefs.putUChar("ver",     3);
        prefs.putUInt ("charDit", charDitLen_ms);
        prefs.putUInt ("gapDit",  gapDitLen_ms);
        prefs.putUInt ("freq",    localFreq);
        prefs.putUInt ("delay",   headCopyDelayMs);
        prefs.putUChar("vol",     sidetone_duty);
        prefs.putUChar("gap",     wordGapDits);
        prefs.end();
        Serial.println("NVS: defaults written");
    } else {
        prefs.begin("keyer", true);
        charDitLen_ms   = prefs.getUInt ("charDit", 60);
        gapDitLen_ms    = prefs.getUInt ("gapDit",  60);
        localFreq       = prefs.getUInt ("freq",    700);
        headCopyDelayMs = prefs.getUInt ("delay",   0);
        sidetone_duty   = prefs.getUChar("vol",     128);
        wordGapDits     = prefs.getUChar("gap",     0);
        prefs.end();
        Serial.println("NVS: settings loaded");
    }
}

// ── setup() ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("K0WLY CW Keyer booting...");

    // Load saved settings from NVS
    loadSettings();

    // GPIO
    pinMode(PIN_DIT,      INPUT_PULLUP);
    pinMode(PIN_DAH,      INPUT_PULLUP);
    pinMode(PIN_KEY_OUT,  OUTPUT);
    digitalWrite(PIN_KEY_OUT, LOW);
    pinMode(PIN_REVERSE,  INPUT_PULLUP);
    pinMode(PIN_MODE_BTN, INPUT_PULLUP);
    pinMode(PIN_KEY_MODE, INPUT_PULLUP);
    straightKey = false;  // default iambic — set true when SK switch is wired to GPIO15

    // LEDC — local sidetone
    ledcSetup(LEDC_CHANNEL_LOCAL, localFreq, LEDC_RES_BITS);
    ledcAttachPin(PIN_SIDETONE, LEDC_CHANNEL_LOCAL);
    ledcWrite(LEDC_CHANNEL_LOCAL, 0);

    // LEDC — remote sidetone (same pin, different channel — actually need same pin)
    // We'll switch between channels dynamically on the same pin
    // Use channel 0 for local, channel 1 for remote, swap attachment as needed
    // Simpler: use one channel, update freq as needed
    // For now both channels share PIN_SIDETONE; remote takes over when local is silent

    // ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // Display
    bool ok = amoled.beginAMOLED_191();
    Serial.println(ok ? "Display OK" : "Display FAILED");
    if (!ok) { while (1) delay(1000); }
    amoled.setRotation(2);
    amoled.setBrightness(200);

    // Allocate full frame buffer in PSRAM (536×240×2 = ~257KB)
    // All drawing goes here first, then flushed to display in one shot per region
    frameBuf = (uint16_t*)ps_malloc(SCR_W * SCR_H * sizeof(uint16_t));
    if (!frameBuf) {
        // Fall back to regular heap if PSRAM unavailable
        frameBuf = (uint16_t*)malloc(SCR_W * SCR_H * sizeof(uint16_t));
        if (!frameBuf) { Serial.println("frameBuf malloc failed!"); while (1) delay(1000); }
        Serial.println("WARNING: frameBuf in SRAM (PSRAM unavailable)");
    } else {
        Serial.println("frameBuf allocated in PSRAM");
    }
    memset(frameBuf, 0, SCR_W * SCR_H * sizeof(uint16_t));
    memset(txLine, 0, sizeof(txLine));
    memset(rxLine, 0, sizeof(rxLine));

    // Initialize pot pickup positions from actual pot reading at boot
    // Pot is locked until user long-presses to enter edit mode
    int bootPotRaw = readPotSmoothed();
    for (int i = 0; i < POT_MODE_COUNT; i++) {
        potPickupRaw[i] = bootPotRaw;
        potPickedUp[i]  = true;
    }
    // NVS values loaded by loadSettings() above — do NOT override with pot position

    drawUI();

    // WiFi / ESP-NOW (runs on Core 0)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    esp_now_init();
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t bcastPeer = {};
    memcpy(bcastPeer.peer_addr, broadcastAddr, 6);
    bcastPeer.channel = 1;
    bcastPeer.encrypt = false;
    esp_now_add_peer(&bcastPeer);

    // Keyer task pinned to Core 1 — completely isolated from WiFi on Core 0
    // Uses vTaskDelayUntil for precise 1ms timing unaffected by WiFi interrupts
    xTaskCreatePinnedToCore(
        [](void*) {
            TickType_t xLastWakeTime = xTaskGetTickCount();
            const TickType_t xFrequency = pdMS_TO_TICKS(1);  // 1ms
            while (true) {
                vTaskDelayUntil(&xLastWakeTime, xFrequency);
                keyer_isr();
            }
        },
        "keyer",     // task name
        2048,        // stack size
        nullptr,     // parameter
        24,          // priority (high, below interrupts)
        nullptr,     // task handle
        1            // Core 1
    );

    Serial.println("Ready. 73 de K0WLY");
}

// ── loop() ───────────────────────────────────────────────────────────────────
static uint32_t lastPotUpdate     = 0;
static uint32_t lastHeaderUpdate  = 0;
static uint32_t lastDiscovery     = 0;
static uint32_t lastRevCheck      = 0;
static uint32_t lastModeCheck     = 0;
static uint32_t lastStatusUpdate  = 0;
static bool     lastPeerFound     = false;

// Track keyer state for element transmission
static KeyerState lastKeyerState  = KEYER_IDLE;
static uint32_t   elementStartMs  = 0;

void loop() {
    uint32_t now = millis();

    // ── Paddle reverse button ────────────────────────────────────────────────
    if (now - lastRevCheck >= 50) {
        lastRevCheck = now;
        static bool lastRevBtn = HIGH;
        bool cur = digitalRead(PIN_REVERSE);
        if (lastRevBtn == HIGH && cur == LOW) paddleReverse = !paddleReverse;
        lastRevBtn = cur;
    }

    // ── Straight key / iambic toggle (GPIO15) ───────────────────────────────
    // Uncomment when SK switch is physically wired to GPIO15
    /*
    {
        static bool lastKeyMode = HIGH;
        bool cur = digitalRead(PIN_KEY_MODE);
        if (cur != lastKeyMode) {
            lastKeyMode = cur;
            straightKey = (cur == LOW);
            drawHeader();
        }
    }
    */

    // ── Pot mode button (short press = next mode, long press = edit mode) ───────
    if (now - lastModeCheck >= 20) {  // 20ms debounce poll
        lastModeCheck = now;
        static bool    lastModeBtn   = HIGH;
        static uint32_t btnPressedAt = 0;
        static bool    longFired     = false;

        bool cur = digitalRead(PIN_MODE_BTN);

        if (lastModeBtn == HIGH && cur == LOW) {
            // Button just pressed — start timing
            btnPressedAt = now;
            longFired    = false;
        }

        if (cur == LOW && !longFired) {
            // Button held — check for long press
            if (now - btnPressedAt >= LONG_PRESS_MS) {
                longFired    = true;
                potEditMode  = !potEditMode;
                if (potEditMode) {
                    // Entering edit mode — sync pickup to current pot position
                    potPickupRaw[potMode] = readPotSmoothed();
                    potPickedUp[potMode]  = true;
                }
                drawStatusArea();
                drawHeader();
            }
        }

        if (lastModeBtn == LOW && cur == HIGH) {
            // Button released
            if (!longFired) {
                if (potEditMode && potMode == POT_WPM && wpmEditStep == 0) {
                    // Short press in WPM step 1 — advance to Farnsworth step
                    wpmEditStep = 1;
                    potPickupRaw[potMode] = readPotSmoothed();
                    potPickedUp[potMode]  = true;
                    drawStatusArea();
                    drawHeader();
                } else {
                    // Short press — exit edit mode, cycle to next mode
                    potEditMode  = false;
                    wpmEditStep  = 0;
                    potPickupRaw[potMode] = readPotSmoothed();
                    potMode = (PotMode)((potMode + 1) % POT_MODE_COUNT);
                    potPickupRaw[potMode] = readPotSmoothed();
                    potPickedUp[potMode]  = true;
                    drawStatusArea();
                    drawHeader();
                }
            }
        }

        lastModeBtn = cur;
    }

    // ── Pot reading (only active in edit mode) ────────────────────────────────
    if (potEditMode && now - lastPotUpdate >= 100) {
        lastPotUpdate = now;
        bool changed = false;
        int rawNow = readPotSmoothed();

        switch (potMode) {
            case POT_WPM: {
                if (wpmEditStep == 0) {
                    // Step 1: character speed
                    uint32_t wpm = WPM_MIN + (uint32_t)((rawNow / 4095.0f) * (WPM_MAX - WPM_MIN));
                    if (wpm < 1) wpm = 1;
                    uint32_t newDit = 1200 / wpm;
                    if (newDit != charDitLen_ms) {
                        charDitLen_ms = newDit;
                        // Keep Farnsworth <= character speed
                        if (gapDitLen_ms < charDitLen_ms) gapDitLen_ms = charDitLen_ms;
                        changed = true;
                    }
                } else {
                    // Step 2: Farnsworth effective speed (4 WPM min, capped at char speed)
                    uint32_t farnMax = charWPM();
                    uint32_t farnMin = 4;
                    if (farnMax < farnMin) farnMax = farnMin;
                    uint32_t wpm = farnMin + (uint32_t)((rawNow / 4095.0f) * (farnMax - farnMin));
                    if (wpm < farnMin) wpm = farnMin;
                    if (wpm > farnMax) wpm = farnMax;
                    uint32_t newGapDit = 1200 / wpm;
                    if (newGapDit != gapDitLen_ms) { gapDitLen_ms = newGapDit; changed = true; }
                }
                break;
            }
            case POT_FREQ: {
                uint32_t freq = FREQ_MIN + (uint32_t)((rawNow / 4095.0f) * (FREQ_MAX - FREQ_MIN));
                if (freq != localFreq) {
                    localFreq = freq;
                    // keyer task will pick up new localFreq on next element start
                    changed = true;
                }
                break;
            }
            case POT_DELAY: {
                uint32_t ms = (uint32_t)((rawNow / 4095.0f) * DELAY_MAX_MS);
                uint32_t del = (ms / 100) * 100;
                if (del != headCopyDelayMs) { headCopyDelayMs = del; changed = true; }
                break;
            }
            case POT_VOL: {
                uint8_t vol = readVol();
                if (vol != sidetone_duty) { sidetone_duty = vol; changed = true; }
                break;
            }
            case POT_GAP: {
                uint8_t gap = readGap();
                if (gap != wordGapDits) { wordGapDits = gap; changed = true; }
                break;
            }
            default: break;
        }
        potPickupRaw[potMode] = rawNow;

        if (changed) {
            saveSettings();   // save to NVS as value changes
            drawStatusArea();
            drawHeader();
        }
    }

    // ── Header refresh (peer status may change) ───────────────────────────────
    if (now - lastHeaderUpdate >= 1000) {
        lastHeaderUpdate = now;
        if (peerFound != lastPeerFound) {
            lastPeerFound = peerFound;
            drawHeader();
            drawStatusArea();
        }
    }

    // ── Discovery broadcast ───────────────────────────────────────────────────
    if (!peerFound && now - lastDiscovery >= 2000) {
        lastDiscovery = now;
        CWPacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = PKT_DISCOVER;
        esp_now_send(broadcastAddr, (uint8_t*)&pkt, sizeof(pkt));
        Serial.println("Broadcasting discover...");
    }

    // ── Detect keyer element transitions for transmission ────────────────────
    KeyerState curState = keyerState;

    // Element start: IDLE/GAP → DIT or DAH
    if ((lastKeyerState == KEYER_IDLE || lastKeyerState == KEYER_DIT_GAP || lastKeyerState == KEYER_DAH_GAP)
        && (curState == KEYER_DIT || curState == KEYER_DAH)) {
        elementStartMs = now;
    }

    // Element end: DIT/DAH → gap
    if ((lastKeyerState == KEYER_DIT && curState == KEYER_DIT_GAP) ||
        (lastKeyerState == KEYER_DAH && curState == KEYER_DAH_GAP)) {
        uint32_t duration = now - elementStartMs;
        bool isDah = (lastKeyerState == KEYER_DAH);
        sendElement(isDah, duration);
    }

    lastKeyerState = curState;

    // ── Outgoing decoded chars ────────────────────────────────────────────────
    while (outBufTail != outBufHead) {
        char c = outBuf[outBufTail];
        outBufTail = (outBufTail + 1) % OUT_BUF_SIZE;
        addTXChar(c);
        sendChar(c);
        Serial.print(c);
    }

    // ── Incoming delayed chars → ready queue ─────────────────────────────────
    while (incomingTail != incomingHead) {
        if (now >= incomingBuf[incomingTail].showAtMs) {
            uint8_t next = (readyHead + 1) % READY_BUF_SIZE;
            if (next != readyTail) {
                readyBuf[readyHead] = incomingBuf[incomingTail].ch;
                readyHead = next;
            }
            incomingTail = (incomingTail + 1) % INCOMING_BUF_SIZE;
        } else {
            break; // rest are in the future
        }
    }

    // ── Display ready incoming chars ──────────────────────────────────────────
    while (readyTail != readyHead) {
        char c = readyBuf[readyTail];
        readyTail = (readyTail + 1) % READY_BUF_SIZE;
        addRXChar(c);
    }

    // ── Remote element playback ───────────────────────────────────────────────
    serviceRemoteElements();

    delay(5);
}