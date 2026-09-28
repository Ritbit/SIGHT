/*
Name        : SIGHT (Shelf Indicators for Guided Handling Tasks)
Version     : 1.11
Date        : 2025-12-18
Author      : Bas van Ritbergen <bas.vanritbergen@adyen.com> / bas@ritbit.com
Description : LED strip controller with animations, RGBW support, and comprehensive safety features.

              v1.11 improvements:
              - Replaced raw-struct persistence (Se/Li:CONFIG:, flash save/
                load) with an explicit field-wise wire format (CONFIG_WIRE_SIZE), independent of compiler layout/padding
              - Reboot-required config changes (Cl, Cx) now interactively
                ask to save and reboot (Y/N) instead of just printing a notice; nothing is forced automatically

              v1.10 improvements:
              - Fixed group-ID truncation (uint16_t end-to-end) and undefined
                behavior in the M command; groups now validate against the active config
              - Added a shared validateConfig() (defaults/load/Li: import) and
                defensive bounds checks in setLEDGroup() (division-by-zero, bad state/pattern/width)
              - Config load now requires an exact file size before memcpy,
                closing an out-of-bounds read on short/corrupt files
              - Fed the watchdog during the startup animation/W command and
                applied brightness before first output, preventing boot-loops and current spikes
              - Fixed loop() render/show ordering (strip always shows the
                latest frame); marked Cl as reboot-required like Cx
              - Moved all CPU LED I/O out of interrupt context into one
                atomic critical section; timing now scales with actual F_CPU
              - Fixed FastLED 3.10+ build breakage from fl:: namespace
                pollution (min/round/abs)
              - Hardened serial parsers: NUL-terminated escape buffer,
                decimal GPIO parsing, strict Cc/Cp/strtol validation instead of atoi
              - LittleFS no longer auto-formats on mount failure; saves are
                atomic (temp file + rename); a format identifier now detects incompatible layouts
              - Closed an ISR/foreground race on animate_Step; errorCount and
                system-info stats are now accurate
              - Documented board-specific settings (CPU LED GPIO, channel
                pins) and added a FastLED version compatibility check

              v1.9.1 improvements:
              - Fixed serial input to accept CR, LF, or CR+LF line endings
              - Added configurable local echo (Ce command) for interactive/API use
              - Improved command line echo to use carriage return (no duplicate lines)
              - Added build-time validation for RP2040 architecture
              - Fixed typo in help text (CPIO -> GPIO)
              - Code cleanup: removed duplicate echo settings


Notes       : When compiling make sure to reserve a little space for littleFS (8-64k)
              Supports both RGB (WS2812B) and RGBW (WS2813B/SK6812) LED strips.
              Switch between modes using USE_RGB_LEDS or USE_RGBW_LEDS define.

Copyright (C) 2024,2025 Bas van Ritbergen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/
// Current firmware version
#define VERSION "1.11"

// Maximum length for system identifier and system default name
#define IDENTIFIER_MAX_LENGTH 16
#define IDENTIFIER_DEFAULT "SIGHT v" VERSION

// Configuration identifier for validation and versioning
// This is stored inside the persisted config struct (see LedData::formatId)
// and checked on load/import. Bump it whenever the LedData layout or the
// meaning of an existing field changes, so an incompatible saved/imported
// struct is detected and rejected instead of being silently misinterpreted.
#define CONFIG_IDENTIFIER "SIGHT-CFG1.11"

// LED strip configuration (LED count limits and defaults)
#define NUM_LEDS_PER_CHANNEL_DEFAULT 57
#define NUM_LEDS_PER_CHANNEL_MIN 6
#define NUM_LEDS_PER_CHANNEL_MAX 600

// Channel configuration (number of active channels)
#define NUM_CHANNELS_DEFAULT 8
#define NUM_CHANNELS_MAX 8

// Group configuration (groups per channel)
#define NUM_GROUPS_PER_CHANNEL_DEFAULT 6
#define NUM_GROUPS_PER_CHANNEL_MAX 100

// Spacer configuration (width between groups)
#define SPACER_WIDTH_DEFAULT 1
#define SPACER_WIDTH_MAX 20

// Default & maximum start offset for LED indexing
#define START_OFFSET 1
#define START_OFFSET_MAX 9

// Maximum state value for validation
#define MAX_STATE 9

// Single group threshold for plural handling
#define SINGLE_GROUP 1

// Command validation constants
#define MIN_COMMAND_CHAR 'A'
#define MAX_COMMAND_CHAR 'Z'

// Input validation constants
#define MIN_GROUP_ID 1
#define MAX_STATE_VALUE 9

// Maximum total groups supported
#define MAX_GROUPS 800

// Blink timing (default/min/max) in milliseconds
#define BLINK_INTERVAL 333
#define BLINK_INTERVAL_MIN 50
#define BLINK_INTERVAL_MAX 3000

// Animation timing (default/min/max) in milliseconds
#define ANIMATE_INTERVAL 150
#define ANIMATE_INTERVAL_MIN 10
#define ANIMATE_INTERVAL_MAX 1000

// Group state update timing (default/min/max) in milliseconds
#define SET_GROUPSTATE_INTERVAL 200
#define SET_GROUPSTATE_INTERVAL_MIN 50
#define SET_GROUPSTATE_INTERVAL_MAX 1000

// LED strip refresh timing (default/min/max) in milliseconds
#define UPDATE_INTERVAL 25
#define UPDATE_INTERVAL_MIN 5
#define UPDATE_INTERVAL_MAX 500

// LED color definitions for different states, use named colors from CRGB or RGB format.
#define COLOR_STATE_1 CRGB::Green
#define COLOR_STATE_2 CRGB::DarkOrange
#define COLOR_STATE_3 CRGB::Red
#define COLOR_STATE_4 CRGB::Blue
#define COLOR_STATE_5 CRGB::Green
#define COLOR_STATE_6 CRGB::DarkOrange
#define COLOR_STATE_7 CRGB::Red
#define COLOR_STATE_8 CRGB::Blue
#define COLOR_STATE_9 CRGB::White

// Animation pattern definitions
// These correspond to the visual patterns in setLEDGroup()
enum AnimationPattern {
  PATTERN_SOLID         = 0,   // [########]              Solid on, no blink
  PATTERN_BLINK         = 1,   // [########] / [        ] Blinking
  PATTERN_BLINK_INV     = 2,   // [        ] / [########] Blinking inverted
  PATTERN_ALT_LR        = 3,   // [####    ] / [    ####] Alternate left/right
  PATTERN_ALT_INOUT     = 4,   // [##    ##] / [  ####  ] Alternate in/out
  PATTERN_ALT_ODDEVEN   = 5,   // [# # # # ] / [ # # # #] Alternate odd/even
  PATTERN_GATED_SOLID   = 6,   // [###  ###]              Gated solid (1/3 gaps)
  PATTERN_GATED_BLINK   = 7,   // [###  ###] / [        ] Gated blink
  PATTERN_CHASE_UP      = 8,   // [>>>>>>>>]              Chase animation going up
  PATTERN_CHASE_DOWN    = 9,   // [<<<<<<<<]              Chase animation going down
  PATTERN_CHASE_UPDOWN  = 10,  // [>>>>>>>><<<<<<<<]      Cylon/Kitt effect
  PATTERN_CHASE_IN      = 11,  // [>>>>    <<<<]          Dual chase inward
  PATTERN_CHASE_OUT     = 12,  // [<<<<    >>>>]          Dual chase outward
  PATTERN_MAX           = 12   // Maximum valid pattern number
};

// System behavior configuration
// Enable startup animation on boot
#define STARTUP_ANIMATION true

// By design, this controller is not intended to run headless: setup()
// waits indefinitely for a serial (USB) connection before proceeding (see
// setup()). Do not add a timeout here without confirming with the product
// owner first -- an earlier attempt to add one was a mistaken, unrequested
// behavior change and was reverted.
//
// Extra grace period (ms) after a serial connection is detected, before any
// boot-banner output is sent. Covers a real race between the firmware
// seeing Serial==true and the host-side terminal actually being ready to
// read, which otherwise loses the first burst of output. This does not
// change the indefinite-wait behavior above; it only smooths out what
// happens once a connection is actually made.
#define SERIAL_CONNECT_SETTLE_MS 300

// Input Data from serial is stored in an array for further processing and editing.
#define HISTORY_SIZE 20

// Enable/disable local echo (character echo while typing on command line)
#define LOCAL_ECHO true

// System status LED configuration (dimmed brightness at 64 for visibility)
#define CPULED_STATUS_BRIGHTNESS 32  // Dimmed brightness for status indicators
#define CPULED_NORMAL_INTERVAL 2000  // Normal operation: slow green glow
#define CPULED_ERROR_INTERVAL 500    // Error state: fast red blink
#define CPULED_STARTUP_INTERVAL 1000  // Startup: medium blue pulse

// LED brightness configuration (0-255 range, with safety limits)
#define STRIP_BRIGHTNESS 255
#define STRIP_BRIGHTNESS_MIN 10
#define STRIP_BRIGHTNESS_MAX 255
#define STRIP_BRIGHTNESS_WARNING_THRESHOLD 200

// Animation fading speed configuration
#define FADING 48
#define FADING_2STEP_IN 50
#define FADING_2STEP_OUT 50

// Enable GPIO test on boot while serial port is not active yet.
// In this mode the GPIO pins wil be made high and low one by one
// so one can detect with a LED or Logic Analyser if all the GPIO ports still work.
#define POWERON_GPIOTEST false

// *** IMPORTANT: Set this to match your LED strip type ***
// Uncomment ONE of these lines:

// For WS2812B RGB strips (3 bytes per LED)
#define USE_RGB_LEDS

// For WS2813B-RGBW, SK6812 RGBW strips (4 bytes per LED)
//#define USE_RGBW_LEDS


// Auto-configure based on strip type
// Error if both LED types are defined
#if defined(USE_RGB_LEDS) && defined(USE_RGBW_LEDS)
  #error "ERROR: Both USE_RGB_LEDS and USE_RGBW_LEDS are defined! Uncomment only ONE."
#endif

// RGBW LED configuration (SK6812)
#ifdef USE_RGBW_LEDS
  // LED type identifier (4 bytes per LED)
  #define LED_TYPE 4
  // LED chipset model
  #define LED_CHIPSET SK6812
  // NOTE: this define is NOT actually honored on the wire in RGBW mode.
  // FastLED.addLeds() below is called with a (CRGB*)-cast CRGBW buffer (see
  // "RGBW mode: Cast CRGBW* to CRGB*..." further down), so FastLED applies
  // its color-order swap to what it thinks are plain 3-byte CRGB structs,
  // never seeing the real 4-byte CRGBW layout. The order actually
  // transmitted is fixed by CRGBW's own field declaration order in
  // FastLED_RGBW.h (g, r, b, w), independent of this define. Hardware-
  // verified with the pulse analyzer (tools/ws2812_pulse_analyzer/):
  // setting Cc:1:112233 (R=0x11 G=0x22 B=0x33) transmitted bytes
  // 22 11 33 00 -- i.e. true wire order is G,R,B,W, not R,G,B,W. This
  // happens to match common SK6812 RGBW wiring, but changing this define
  // will NOT change the transmitted order; see CODE_REVIEW.md finding 10.
  #define LED_COLOR_ORDER RGB
  // Compilation message for RGBW LEDs
  #pragma message "Compiling for RGBW LEDs (SK6812, 4 bytes/LED, true wire order G,R,B,W -- LED_COLOR_ORDER above is not honored, see comment)"
// RGB LED configuration (WS2812B)
#else
  // LED type identifier (3 bytes per LED)
  #define LED_TYPE 3
  // LED chipset model
  #define LED_CHIPSET WS2812B
  // LED color byte order
  #define LED_COLOR_ORDER GRB
  // Compilation message for RGB LEDs
  #pragma message "Compiling for RGB LEDs (WS2812B, 3 bytes/LED, GRB order)"
#endif

// Minimum and Maximum allowed GPIO pin number
#define GPIO_PIN_MIN 2
#define GPIO_PIN_MAX 26

// GPIO pin for the onboard status LED (bit-banged WS2812-style RGB LED).
// This is board-specific: GPIO 16 is correct for the Waveshare RP2040 Zero.
// If you target a different RP2040 board, update this to match its onboard
// addressable LED pin (or disable the status LED code entirely if the board
// has none). ARDUINO_WAVESHARE_RP2040_ZERO is defined by the Arduino-Pico
// core when that board is selected in the IDE.
#if defined(ARDUINO_WAVESHARE_RP2040_ZERO)
  #define CPULED_GPIO 16
#else
  #define CPULED_GPIO 16
  #warning "CPULED_GPIO defaults to 16, which is only verified correct for the Waveshare RP2040 Zero. Verify/update it for your board."
#endif

// CPU LED's onboard chip color order. sendRGB_CPULED() bit-bangs this
// directly (it is a separate, hand-written path from the main strips'
// FastLED/LED_COLOR_ORDER) and sends whichever order is selected here --
// there is no auto-detection. The real deployed SIGHT unit is verified
// (visually and with tools/ws2812_pulse_analyzer/) to want RGB order. Two
// other physical boards nominally the same model ("Waveshare RP2040
// Zero") were found to actually want GRB instead: green displayed as
// red (blue was correct either way, since blue's byte doesn't move
// between RGB/GRB -- it's the 3rd byte in both). This is genuine
// board/batch hardware variance in the onboard chip, not a firmware bug.
// If you swap in a different physical board, re-verify the colors
// (blue in SYSTEM_STARTUP, green in SYSTEM_NORMAL, red in SYSTEM_ERROR)
// and change this define if needed -- don't assume every board with the
// same model name has the same onboard chip order.
#define CPULED_COLOR_ORDER_RGB 0
#define CPULED_COLOR_ORDER_GRB 1
#define CPULED_COLOR_ORDER CPULED_COLOR_ORDER_RGB

// Configuration file path in LittleFS
#define CONFIG_FILENAME "/config.bin"
// Temporary path used while saving, so a reset/power loss mid-write cannot
// leave a truncated/corrupt config.bin (see saveConfiguration()).
#define CONFIG_FILENAME_TMP "/config.bin.tmp"

// ###########################################################################
// No configurable items below


// Buffer size configuration (input/output string limits)
#define MAX_INPUT_LEN 512
// Used only for short, single-line formatted messages (a few dozen bytes at
// most); previously 2560, which put an oversized 2.5 KiB buffer on the stack
// in checkInput() and showConfiguration() for no benefit.
#define MAX_OUTPUT_LEN 128

// We definitely need these libraries
#include <Arduino.h>            // Core Arduino library e.g. for GPIO pins
#include <cstring>              // String functions
#include <cstdlib>              // Memory functions
#include <cerrno>               // errno for strict numeric parsing (strictParseLong)
#include <Ticker.h>             // Ticker library for timed events (animations/flashing)
#include <Crypto.h>             // Crypto library for SHA256
#include <SHA256.h>             // SHA256 library for creating configfile checksum
#include "LittleFS.h"           // FileSystem library for storing config
#include <FastLED.h>            // Core LED control library (RGB/RGBW)
#include "FastLED_RGBW.h"       // Add RGBW support for FastLED  
#include "hardware/watchdog.h"  // Core watchdog timer
#include "hardware/sync.h"      // save_and_disable_interrupts/restore_interrupts for CPU LED bit-banging
#include <MicrocontrollerID.h>  // Figure MCU type/serial

// ============================================================================
// Build-time validation: Ensure compiling for RP2040
// ============================================================================
#if !defined(ARDUINO_ARCH_RP2040)
  #error "This firmware requires an RP2040-based board. Select the correct board in Arduino IDE."
#endif

// ============================================================================
// Build-time check: FastLED version this sketch was verified against
// ============================================================================
// FastLED >= 3.10 injects fl::min/fl::max/fl::round/fl::abs into the global
// namespace, which makes unqualified calls to those names ambiguous (this
// sketch avoids them entirely; see AGENTS.md/CODE_REVIEW.md). This is only a
// warning, not a hard version pin, since patch releases are expected to stay
// compatible; verify a min/round/abs-related build failure or CPU LED/strip
// timing change if you update FastLED and this fires.
#define FASTLED_TESTED_VERSION 3010005  // FastLED 3.10.5
#if defined(FASTLED_VERSION) && FASTLED_VERSION != FASTLED_TESTED_VERSION
  #pragma message "NOTE: This sketch was verified against FastLED 3.10.5. A different FastLED version is in use; re-verify the build and CPU LED/strip timing."
#endif

// If no FS space is allocated, LittleFS will fail to mount and configuration will not be saved/loaded
// To fix: Arduino IDE -> Tools -> Flash Size -> Select option with 'FS'
//         Example: "2MB (Sketch: 1984KB, FS: 64KB)"

// Declare LedStrip control arrays
#if LED_TYPE == 4
  // RGBW strips: Use CRGBW arrays (4 bytes per LED)
  CRGBW leds[NUM_CHANNELS_DEFAULT+1][NUM_LEDS_PER_CHANNEL_MAX];
  #define ZERO_W(led) led.w = 0  // Zero out W channel for RGBW mode
  using LedPixel = CRGBW;
#else
  // RGB strips: Use CRGB arrays (3 bytes per LED)
  CRGB leds[NUM_CHANNELS_DEFAULT+1][NUM_LEDS_PER_CHANNEL_MAX];
  #define ZERO_W(led)  // Do nothing for RGB mode (no W channel)
  using LedPixel = CRGB;
#endif

char mcuId[41];

// Exact size, in bytes, of the field-wise wire format used to persist/
// export/import LedData (see encodeConfig()/decodeConfig() further down).
// Computed from field widths, not sizeof(LedData), so it never depends on
// this compiler's struct layout/padding/CRGB representation.
#define CONFIG_WIRE_SIZE (16 + IDENTIFIER_MAX_LENGTH + 2 + 1 + 1 + 1 + 1 + 2 + 2 + 2 + 2 + 2 + 2 + 2 + 1 + 1 + \
                           NUM_CHANNELS_MAX + 12 + (10 * 3) + NUM_CHANNELS_MAX)
// Tripwire: if IDENTIFIER_MAX_LENGTH or NUM_CHANNELS_MAX ever change, this
// forces a review of encodeConfig()/decodeConfig() (which hardcode field
// counts/widths) instead of silently drifting out of sync with the macro.
#if CONFIG_WIRE_SIZE != 112
  #error "CONFIG_WIRE_SIZE changed -- review encodeConfig()/decodeConfig() field-by-field before updating this check"
#endif

// Config Data is conviently stored in a struct (to easy store and retrieve from EEPROM/Flash)
// Set defaults, they will be overwritten by load from EEPROM
struct LedData {
  // Persisted format identifier/version. Raw-struct persistence is sensitive to
  // ABI layout, padding, and field meaning; this is checked on load/import so a
  // struct from an incompatible firmware build is flagged instead of being
  // silently misinterpreted. Bump CONFIG_IDENTIFIER whenever the layout changes.
  char   formatId[16] = CONFIG_IDENTIFIER;
  char   identifier[IDENTIFIER_MAX_LENGTH] = IDENTIFIER_DEFAULT;
  uint16_t               numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
  uint8_t                      numChannels = NUM_CHANNELS_DEFAULT;
  uint8_t              numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
  uint8_t                      spacerWidth = SPACER_WIDTH_DEFAULT;
  uint8_t                      startOffset = START_OFFSET;
  uint16_t                   blinkinterval = BLINK_INTERVAL;
  uint16_t                 animateinterval = ANIMATE_INTERVAL;
  uint16_t                  updateinterval = UPDATE_INTERVAL;
  uint16_t                      brightness = STRIP_BRIGHTNESS;
  uint16_t                 fadingAnimation = FADING;
  uint16_t                   fading2StepIn = FADING_2STEP_IN;
  uint16_t                  fading2StepOut = FADING_2STEP_OUT;
  bool                    startupAnimation = STARTUP_ANIMATION;
  bool                           localEcho = LOCAL_ECHO;
  uint8_t channelGPIOpin[NUM_CHANNELS_MAX] = {2,3,4,5,6,7,8,9};
  uint8_t                state_pattern[12] = {0,0,0,0,0,1,1,1,1,0};
  CRGB                     state_color[10] = {CRGB::Black, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, CRGB::White};
  uint8_t   channelOrder[NUM_CHANNELS_MAX] = {1,2,3,4,5,6,7,8};
} LedConfig = {};

char inputBuffer[MAX_INPUT_LEN + 1]; // +1 for null terminator
uint16_t inputLength = 0;
uint16_t cursorPosition = 0;
bool insertMode = true;

// When a config change needs a reboot to fully take effect, we ask the
// operator whether to save and reboot now rather than doing either
// automatically. See promptSaveAndReboot() and its handling in checkInput().
bool pendingRebootConfirm = false;

char commandHistory[HISTORY_SIZE][MAX_INPUT_LEN + 1];
int historyHead = 0; // Points to the next slot to write
int historySize = 0; // Number of stored commands
int historyBrowseOffset = 0; // 0=current line, 1=last command, etc.
char historyStagingBuffer[MAX_INPUT_LEN + 1];
bool historyStagingValid = false;
bool lastCharWasCR = false;
uint16_t lastRenderedLength = 0;

enum EscapeParseState {
  ESC_STATE_NONE = 0,
  ESC_STATE_ESC,
  ESC_STATE_CSI
};

EscapeParseState escapeState = ESC_STATE_NONE;
char escapeDigits[5];  // Up to 4 CSI parameter digits + NUL terminator
uint8_t escapeDigitCount = 0;
const uint16_t ESC_TIMEOUT_MS = 80;
uint32_t escapeStartMillis = 0;


// For color/blinking-state feature we need some extra global parameters
uint8_t  TermState[MAX_GROUPS] = {0};
uint8_t    TermPct[MAX_GROUPS] = {0};
volatile bool blinkState = 0;
uint32_t lastToggleTimes = 0;

// System status LED management
enum SystemState {
  SYSTEM_STARTUP,
  SYSTEM_NORMAL,
  SYSTEM_ERROR
};
SystemState currentSystemState = SYSTEM_STARTUP;
uint32_t lastCpuLedUpdate = 0;
uint8_t cpuLedBrightness = 0;
bool cpuLedDirection = true; // true = brightening, false = dimming

// Statistics tracking
uint32_t bootTime = 0;
uint32_t CommandCount = 0;
uint32_t errorCount = 0;

// Forward declarations
void checkInput(char input[MAX_INPUT_LEN]);
void handleDisplayCommands(char Command);
void handleStateCommands(char Command, char *Data);
void handleSystemCommands(char Command, char *Data);
void handleConfigurationCommands(char Command, char *Data);

// Flags for updates
volatile bool ChannelUpdate = false;
volatile bool SetGroupStateFlag = false;

// Timer objects for pico_sdk
Ticker update_Timer;
Ticker blink_Timer;
Ticker setgroup_Timer;
Ticker animate_Timer;
// Concurrency model: animateStep() (a Ticker/interrupt callback) only
// increments each element. setLEDGroup() (foreground, called from loop())
// periodically bounds each element with a modulo via boundAnimateStep(),
// which wraps the read-modify-write in a short critical section so an
// increment landing between the read and the write can't be lost/torn.
uint8_t animate_Step[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/**
 * Bound an animate_Step[] element to [0, modulus) atomically with respect to
 * the animateStep() interrupt callback, which only increments these elements.
 * Returns the bounded value so callers can use it for indexing directly,
 * instead of re-reading animate_Step[pattern] afterward -- the ISR can fire
 * again in the gap between this call and a later unprotected read, drifting
 * the array element 1 past the bound that was just applied here.
 * @param pattern Index into animate_Step[]
 * @param modulus Exclusive upper bound (must be >= 1)
 * @return The bounded value of animate_Step[pattern]
 */
inline uint8_t boundAnimateStep(uint8_t pattern, uint16_t modulus) {
  uint32_t interruptStatus = save_and_disable_interrupts();
  animate_Step[pattern] = animate_Step[pattern] % modulus;
  uint8_t bounded = animate_Step[pattern];
  restore_interrupts(interruptStatus);
  return bounded;
}

// ##########################################################################################################

/**
 * Initialize microcontroller and setup system
 * Configures GPIO pins, serial communication, LED strips, timers, and loads configuration
 */
void setup() {

  // Setup USB-serial port
  Serial.begin(115200);

  // Initialize status led, set to blue to show we are waiting for input
  // We have to disable theCPU led as Fastled can only drive 8 led channels ! (due to 8 PIO registers)
  // So we have to bitbang if we want to use it...
  pinMode(CPULED_GPIO, OUTPUT);
  gpio_put(CPULED_GPIO, 0);
  CPULED(0x00,0x00,0x00);

  // By design, this controller requires an operator to connect over serial
  // before it proceeds -- it is not intended to run headless. Wait here
  // indefinitely for that connection; the pulsing blue status LED
  // (SYSTEM_STARTUP) is the operator-facing signal that the controller is
  // powered and waiting. This runs before watchdog_enable(), so no
  // watchdog feeding is needed during the wait.
  setSystemState(SYSTEM_STARTUP);
#if defined(POWERON_GPIOTEST) && POWERON_GPIOTEST == true
  // Bench-test aid: also toggle GPIO 2-9 high/low one at a time while
  // waiting, so a logic analyzer/LED can confirm every channel GPIO still
  // works, without changing the indefinite-wait behavior above.
  for (int PIN=0; PIN<NUM_CHANNELS_MAX; PIN++) {
    pinMode(PIN+GPIO_PIN_MIN, OUTPUT);
  }
  while (!Serial) {
    updateSystemStatusLED();
    for (int PIN=0; PIN<NUM_CHANNELS_MAX; PIN++) {
      digitalWrite(PIN+GPIO_PIN_MIN, HIGH);
      delay(100);
      digitalWrite(PIN+GPIO_PIN_MIN, LOW);
      updateSystemStatusLED();
    }
  }
#else
  while (!Serial) {
    updateSystemStatusLED();
    delay(10);
  }
#endif
  // Serial.operator bool() (tud_cdc_connected()) can briefly report
  // "connected" a moment before the host-side terminal/application has
  // actually attached its read buffer (a well-known race on native-USB
  // boards). This short grace period avoids losing the first burst of boot
  // banner output to that race; it does not change the indefinite wait
  // above in any way.
  delay(SERIAL_CONNECT_SETTLE_MS);

  // Enable watchdog timer (8 seconds timeout)
  // System will auto-reboot if watchdog is not fed within this time
  watchdog_enable(8000, 1);

  // Set status led to green glow to show normal operation
  setSystemState(SYSTEM_NORMAL);

  //   Reset all to low
  for (int PIN=2; PIN<=9; PIN++) {
    digitalWrite(PIN, LOW);
  }

  updateSystemStatusLED();
  // Show welcome to let us know the controller is booting.
  // Also show some details about the MCU and the codeversion

  // Show version
  // Serial.print("\x1b[2J\x1b[H"); // Clear screen, cursor home
  Serial.println();
  Serial.println("-=[ Shelf Indicators for Guided Handling Tasks ]=-");
  Serial.println();
  Serial.print("SIGHT Version  : " );
  Serial.println(VERSION);  // Why does this add a 0 to the string ??

  // Check if system recovered from watchdog reset
  if (watchdog_caused_reboot()) {
    Serial.println("*** WARNING: System recovered from watchdog timeout ***");
  }

  // Boardname
  Serial.print("MicroController : " );
  Serial.println(BOARD_NAME);

  // CPU id
  // Note: often the MCU hangs/crashes on this... why ?
  Serial.print("MCU-Serial      : " );
  MicroID.getUniqueIDString(mcuId, 8);
  Serial.println(mcuId);

  // Blank line
  Serial.println();
  Serial.println("Initializing..." );
  Serial.println();

  // Initialize LittleFS if available
  if (!LittleFS.begin()){
    // Do NOT auto-format: a transient mount/compatibility issue would
    // otherwise silently destroy any previously saved configuration.
    // Fall back to in-memory defaults (not persisted) and require an
    // explicit, confirmed 'F:YES' command before ever formatting.
    Serial.println("WARNING !!!");
    Serial.println("LittleFS mount failed. Load/Saving configuration is not possible.");
    Serial.println("Running with in-memory defaults for this session.");
    Serial.println("If this is expected (e.g. first boot with no filesystem yet), use");
    Serial.println("'F:YES' to format LittleFS, then 'S' to save a configuration.");
    Serial.println();
    resetToDefaults();
    setSystemState(SYSTEM_ERROR);
  } else {
    Serial.println("LittleFS mounted successfully.");
    // Load or set defaults
    if (loadConfiguration() == false) {
      Serial.println("Setting default configuration");
      resetToDefaults();
      // Set error state due to configuration failure
      setSystemState(SYSTEM_ERROR);
      Serial.println("Status LED: Red blink (config error)");
    }
  }

  // Calculate buffer size for FastLED
  int MAX_LEDS;
  #if LED_TYPE == 4
    // RGBW: We use CRGBW arrays (4 bytes per LED) for proper alignment
    // FastLED thinks they're RGB (3 bytes), so we tell it about more "RGB LEDs"
    // to cover all 4 bytes per physical LED (W channel always set to 0)
    MAX_LEDS = (LedConfig.numLedsPerChannel * 4 + 2) / 3;  // +2 for rounding up
  #else
    // RGB: Standard 3 bytes per LED, 1:1 mapping
    MAX_LEDS = LedConfig.numLedsPerChannel;
  #endif

  for (uint8_t CHANNEL=0; CHANNEL<NUM_CHANNELS_DEFAULT ; CHANNEL++) {
    pinMode(LedConfig.channelGPIOpin[CHANNEL], OUTPUT);
    if (LedConfig.channelGPIOpin[CHANNEL] != CPULED_GPIO) {
      #if LED_TYPE == 4
        // RGBW mode: Cast CRGBW* to CRGB* to trick FastLED into sending 4 bytes per LED
        switch (LedConfig.channelGPIOpin[CHANNEL]) {
          case  0: FastLED.addLeds<LED_CHIPSET,  0, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  1: FastLED.addLeds<LED_CHIPSET,  1, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  2: FastLED.addLeds<LED_CHIPSET,  2, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  3: FastLED.addLeds<LED_CHIPSET,  3, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  4: FastLED.addLeds<LED_CHIPSET,  4, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  5: FastLED.addLeds<LED_CHIPSET,  5, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  6: FastLED.addLeds<LED_CHIPSET,  6, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  7: FastLED.addLeds<LED_CHIPSET,  7, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  8: FastLED.addLeds<LED_CHIPSET,  8, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case  9: FastLED.addLeds<LED_CHIPSET,  9, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 10: FastLED.addLeds<LED_CHIPSET, 10, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 11: FastLED.addLeds<LED_CHIPSET, 11, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 12: FastLED.addLeds<LED_CHIPSET, 12, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 13: FastLED.addLeds<LED_CHIPSET, 13, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 14: FastLED.addLeds<LED_CHIPSET, 14, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 15: FastLED.addLeds<LED_CHIPSET, 15, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 17: FastLED.addLeds<LED_CHIPSET, 17, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 18: FastLED.addLeds<LED_CHIPSET, 18, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 19: FastLED.addLeds<LED_CHIPSET, 19, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 20: FastLED.addLeds<LED_CHIPSET, 20, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 21: FastLED.addLeds<LED_CHIPSET, 21, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 22: FastLED.addLeds<LED_CHIPSET, 22, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 23: FastLED.addLeds<LED_CHIPSET, 23, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 24: FastLED.addLeds<LED_CHIPSET, 24, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 25: FastLED.addLeds<LED_CHIPSET, 25, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 26: FastLED.addLeds<LED_CHIPSET, 26, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 27: FastLED.addLeds<LED_CHIPSET, 27, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 28: FastLED.addLeds<LED_CHIPSET, 28, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
          case 29: FastLED.addLeds<LED_CHIPSET, 29, LED_COLOR_ORDER>((CRGB*)leds[CHANNEL], MAX_LEDS); break;
        }
      #else
        // RGB mode: Use leds directly (already CRGB*), no cast needed
        switch (LedConfig.channelGPIOpin[CHANNEL]) {
          case  0: FastLED.addLeds<LED_CHIPSET,  0, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  1: FastLED.addLeds<LED_CHIPSET,  1, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  2: FastLED.addLeds<LED_CHIPSET,  2, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  3: FastLED.addLeds<LED_CHIPSET,  3, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  4: FastLED.addLeds<LED_CHIPSET,  4, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  5: FastLED.addLeds<LED_CHIPSET,  5, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  6: FastLED.addLeds<LED_CHIPSET,  6, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  7: FastLED.addLeds<LED_CHIPSET,  7, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  8: FastLED.addLeds<LED_CHIPSET,  8, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case  9: FastLED.addLeds<LED_CHIPSET,  9, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 10: FastLED.addLeds<LED_CHIPSET, 10, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 11: FastLED.addLeds<LED_CHIPSET, 11, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 12: FastLED.addLeds<LED_CHIPSET, 12, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 13: FastLED.addLeds<LED_CHIPSET, 13, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 14: FastLED.addLeds<LED_CHIPSET, 14, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 15: FastLED.addLeds<LED_CHIPSET, 15, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 17: FastLED.addLeds<LED_CHIPSET, 17, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 18: FastLED.addLeds<LED_CHIPSET, 18, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 19: FastLED.addLeds<LED_CHIPSET, 19, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 20: FastLED.addLeds<LED_CHIPSET, 20, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 21: FastLED.addLeds<LED_CHIPSET, 21, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 22: FastLED.addLeds<LED_CHIPSET, 22, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 23: FastLED.addLeds<LED_CHIPSET, 23, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 24: FastLED.addLeds<LED_CHIPSET, 24, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 25: FastLED.addLeds<LED_CHIPSET, 25, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 26: FastLED.addLeds<LED_CHIPSET, 26, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 27: FastLED.addLeds<LED_CHIPSET, 27, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 28: FastLED.addLeds<LED_CHIPSET, 28, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
          case 29: FastLED.addLeds<LED_CHIPSET, 29, LED_COLOR_ORDER>(leds[CHANNEL], MAX_LEDS); break;
        }
      #endif
    }
  }

  Serial.println("Initialization done..,");

  // Set system state to normal operation
  setSystemState(SYSTEM_NORMAL);
  Serial.println("System ready - Status LED: Green glow");

  // Apply brightness before any strip output, including the startup animation
 	FastLED.setBrightness(LedConfig.brightness);

  // Run startup animation if enabled
  if (LedConfig.startupAnimation) {
    StartupLoop();
  } else {
    // Clear LEDs if no startup animation
    FastLED.clear();
    FastLED.show();
  }

  // start timers for updating leds (x1000 so we set mSec)
  update_Timer.attach_ms(LedConfig.updateinterval, &writeChannelData);
  blink_Timer.attach_ms(LedConfig.blinkinterval, &setBlinkState);
  animate_Timer.attach_ms(LedConfig.animateinterval, &animateStep);
  setgroup_Timer.attach_ms(LedConfig.updateinterval*2, &setGroupState);

  // Show help & config:
  showConfiguration();

  Serial.println("Enter 'H' for help ");
  Serial.println();

  // Initialize boot time for uptime tracking
  bootTime = millis();

  // Ready to go, show prompt to show we are ready for input
  Serial.print("> ");
}

/**
 * Write channel Data update flag
 * Sets the ChannelUpdate flag to true to trigger LED updates
 */
void writeChannelData() {
  ChannelUpdate = true;
  return;
}

/**
 * Toggle blink state for strip blinking patterns
 * Switches blinkState between true/false. This runs in Ticker (interrupt)
 * context, so it must only set a flag/variable; all hardware I/O (including
 * the CPU status LED) is handled exclusively in loop()/updateSystemStatusLED().
 */
void setBlinkState() {
  blinkState = !blinkState;
  return;
}

/**
 * Increment animation step counters
 * Advances all animation step counters for smooth animation transitions
 */
void animateStep() {
  // just count up here, the clipping happens in the UpdateLED function as the length varies on the effect and group-size.
  for (int i = 0; i < sizeof(animate_Step); i++) {
    animate_Step[i]++;
  }
  return;
}

/**
 * Set group state update flag
 * Sets the SetGroupStateFlag to true to trigger group state processing
 */
void setGroupState() {
  SetGroupStateFlag = true;
  return;
}

// ##########################################################################################################


/**
 * Main program loop
 * Handles watchdog feeding, serial input processing, and LED updates
 */
void loop() {
  // Feed the watchdog to prevent timeout
  watchdog_update();

  // Update system status LED pattern
  updateSystemStatusLED();

  if (Serial.available() > 0) {
    handleSerialInput();
  }

  // Render the pixel buffer before sending it, so FastLED.show() always
  // transmits the most recently computed group states rather than the
  // previous iteration's buffer.
  if (ChannelUpdate) {
    ChannelUpdate=false;
    updateGroups();
  }

  if (SetGroupStateFlag) {
     FastLED.show();
     SetGroupStateFlag=false;
  }
}

/**
 * Handle serial input character by character
 * Supports backspace, cancel (ESC/Ctrl+C), and Command execution on newline
 */
void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (escapeState != ESC_STATE_NONE && escapeStartMillis && millis() - escapeStartMillis > ESC_TIMEOUT_MS) {
      escapeState = ESC_STATE_NONE;
      escapeDigitCount = 0;
    }

    if (escapeState == ESC_STATE_NONE) {
      if (c == '\x1B') {
        escapeState = ESC_STATE_ESC;
        escapeStartMillis = millis();
        continue;
      }
    } else if (escapeState == ESC_STATE_ESC) {
      if (c == '[') {
        escapeState = ESC_STATE_CSI;
        escapeDigitCount = 0;
        escapeStartMillis = millis();
        continue;
      } else {
        escapeState = ESC_STATE_NONE;
      }
    } else if (escapeState == ESC_STATE_CSI) {
      if (c >= '0' && c <= '9') {
        if (escapeDigitCount < sizeof(escapeDigits) - 1) {  // Leave room for NUL
          escapeDigits[escapeDigitCount++] = c;
        }
        escapeStartMillis = millis();
        continue;
      }

      escapeDigits[escapeDigitCount] = '\0';
      int value = (escapeDigitCount == 0) ? 0 : atoi(escapeDigits);
      if (c == 'A') {
        if (historyBrowseOffset < historySize) {
          historyBrowseOffset++;
          recallHistory(historyBrowseOffset);
        }
      } else if (c == 'B') {
        if (historyBrowseOffset > 0) {
          historyBrowseOffset--;
          recallHistory(historyBrowseOffset);
        }
      } else if (c == 'C') {
        moveCursorRight();
      } else if (c == 'D') {
        moveCursorLeft();
      } else if (c == 'H') {
        moveCursorHome();
      } else if (c == 'F') {
        moveCursorEnd();
      } else if (c == '~') {
        if (value == 1) moveCursorHome();
        else if (value == 4) moveCursorEnd();
        else if (value == 3) deleteCharacter(false);
      }

      escapeState = ESC_STATE_NONE;
      escapeDigitCount = 0;
      continue;
    }

    switch (c) {
      case '\x03':
        Serial.println();
        Serial.println("CANCELLED");
        resetInputBuffer();
        Serial.print("> ");
        lastCharWasCR = false;
        break;
      case '\r':
        // CR received - execute command, mark that we saw CR
        acceptCurrentLine();
        lastCharWasCR = true;
        break;
      case '\n':
        // LF received - only execute if not preceded by CR (to handle \r\n)
        if (!lastCharWasCR) {
          acceptCurrentLine();
        }
        lastCharWasCR = false;
        break;
      case 0x7F:
        deleteCharacter(false);
        break;
      case 0x08:
        deleteCharacter(true);
        break;
      case '\t':
        insertCharacter(' ');
        insertCharacter(' ');
        break;
      default:
        if (isPrintable(c)) {
          insertCharacter(c);
        }
        break;
    }
  }
}

/**
 * Check if character is printable (excludes control chars and some special chars)
 * @param c Character to check
 * @return true if character is printable, false otherwise
 */
bool isPrintable(const char c) {
  return c >= 0x20 && c < 0x7E && c != 0x5C && c != 0x60;
}

/**
 * Print current prompt and buffer with cursor positioning
 * Only outputs if localEcho is enabled
 * Uses carriage return to overwrite current line (no newlines while typing)
 */
void redrawInputLine() {
  if (!LedConfig.localEcho) return;
  
  // Carriage return to beginning of line, then clear line and redraw
  Serial.print("\r> ");
  Serial.print(inputBuffer);
  
  // Clear any leftover characters from previous longer input
  if (inputLength < lastRenderedLength) {
    for (uint16_t i = inputLength; i < lastRenderedLength; i++) {
      Serial.print(' ');
    }
    // Move cursor back after clearing
    for (uint16_t i = inputLength; i < lastRenderedLength; i++) {
      Serial.print('\b');
    }
  }
  lastRenderedLength = inputLength;
  
  // Position cursor correctly (move back from end to cursor position)
  for (uint16_t i = cursorPosition; i < inputLength; i++) {
    Serial.print('\b');
  }
}

void moveCursorLeft() {
  if (cursorPosition > 0) {
    cursorPosition--;
    redrawInputLine();
  }
}

void moveCursorRight() {
  if (cursorPosition < inputLength) {
    cursorPosition++;
    redrawInputLine();
  }
}

void moveCursorHome() {
  cursorPosition = 0;
  redrawInputLine();
}

void moveCursorEnd() {
  cursorPosition = inputLength;
  redrawInputLine();
}

void insertCharacter(char c) {
  if (inputLength >= MAX_INPUT_LEN - 1) {
    Serial.print('\a');
    return;
  }

  if (!insertMode && cursorPosition < inputLength) {
    inputBuffer[cursorPosition] = c;
  } else {
    memmove(inputBuffer + cursorPosition + 1,
            inputBuffer + cursorPosition,
            inputLength - cursorPosition + 1);
    inputBuffer[cursorPosition] = c;
    inputLength++;
  }

  cursorPosition++;
  redrawInputLine();
}

void deleteCharacter(bool backspace) {
  if (backspace) {
    if (cursorPosition == 0) {
      return;
    }
    cursorPosition--;
  }

  if (cursorPosition >= inputLength) {
    return;
  }

  memmove(inputBuffer + cursorPosition,
          inputBuffer + cursorPosition + 1,
          inputLength - cursorPosition);
  inputLength--;
  inputBuffer[inputLength] = '\0';
  redrawInputLine();
}

void resetInputBuffer() {
  inputBuffer[0] = '\0';
  inputLength = 0;
  cursorPosition = 0;
  historyBrowseOffset = 0;
  historyStagingValid = false;
  lastRenderedLength = 0;
}

void acceptCurrentLine() {
  Serial.println();
  if (inputLength >= MAX_INPUT_LEN) {
    inputLength = MAX_INPUT_LEN - 1;
  }
  inputBuffer[inputLength] = '\0';

  bool hadInput = inputLength > 0;
  char commandCopy[MAX_INPUT_LEN + 1];
  if (hadInput) {
    strncpy(commandCopy, inputBuffer, MAX_INPUT_LEN);
    commandCopy[MAX_INPUT_LEN] = '\0';
  }

  checkInput(inputBuffer);
  Serial.print("> ");

  if (hadInput) {
    strncpy(commandHistory[historyHead], commandCopy, MAX_INPUT_LEN);
    commandHistory[historyHead][MAX_INPUT_LEN] = '\0';
    historyHead = (historyHead + 1) % HISTORY_SIZE;
    if (historySize < HISTORY_SIZE) {
      historySize++;
    }
  }

  resetInputBuffer();
}

void recallHistory(int offset) {
  if (historySize == 0) {
    Serial.print('\a');
    return;
  }

  if (!historyStagingValid) {
    strncpy(historyStagingBuffer, inputBuffer, MAX_INPUT_LEN);
    historyStagingBuffer[MAX_INPUT_LEN] = '\0';
    historyStagingValid = true;
  }

  if (offset == 0) {
    strncpy(inputBuffer, historyStagingBuffer, MAX_INPUT_LEN);
    inputBuffer[MAX_INPUT_LEN] = '\0';
  } else {
    int index = historyHead - offset;
    if (index < 0) {
      index += HISTORY_SIZE;
    }

    strncpy(inputBuffer, commandHistory[index], MAX_INPUT_LEN);
    inputBuffer[MAX_INPUT_LEN] = '\0';
  }

  inputLength = strlen(inputBuffer);
  cursorPosition = inputLength;
  redrawInputLine();
}

/**
 * Parse and execute serial Commands
 * @param input Command string from serial input
 */
void checkInput(char input[MAX_INPUT_LEN]) {
  CPULED(0x00,0x00,0x80);

  // Move to new line after user input
  Serial.println();

  // A reboot-required change asked whether to save and reboot now; this
  // line answers that prompt instead of being parsed as a normal command.
  if (pendingRebootConfirm) {
    if (input[0] == 0) {
      Serial.println("Save configuration and reboot now to apply the pending change? (Y/N)");
      return;
    }
    pendingRebootConfirm = false;
    char answer = input[0];
    if (answer == 'Y' || answer == 'y') {
      Serial.print("Save configuration: ");
      if (saveConfiguration()) {
        Serial.println("Success.");
        Serial.println("Rebooting controller...");
        rebootMCU();
      } else {
        Serial.println("Failed. Reboot cancelled -- fix the issue, then use 'S' and 'R' manually.");
      }
    } else if (answer == 'N' || answer == 'n') {
      Serial.println("Reboot postponed. Use 'S' then 'R' later to apply the pending change.");
      return;
    } else {
      // Neither Y/N nor a valid command in this context -- the unconditional
      // return below discards this line rather than parsing it as a normal
      // command, so the prompt is simply cancelled.
      Serial.println("Reboot prompt cancelled -- use 'S' then 'R' manually if needed.");
    }
    return;
  }

  if (input[0] == 0) {
    return;
  }

  char output[MAX_OUTPUT_LEN];
  memset(output, '\0', sizeof(output));

  int parseResult = 0;
  int state = 0;
  int pct = 0;
  int groupID = 0;
  char command = input[0];
  char *Data = input + 1;


  // Increment Command counter
  CommandCount++;

  if (command >= MIN_COMMAND_CHAR && command <= MAX_COMMAND_CHAR) {
    switch(command) {

      // Help Command
      case 'H':
      case '?':
        showHelp();
        break;

      // Version information
      case 'V':
        Serial.println();
        Serial.println("=== SIGHT Version Information ===");
        Serial.print("Version          : ");
        Serial.println(VERSION);
        Serial.print("Build Date       : ");
        Serial.println(__DATE__ " " __TIME__);
        Serial.print("LED Type       : ");
        #ifdef USE_RGBW_LEDS
          Serial.println("RGBW (4 bytes/LED)");
          Serial.println("Chipset          : SK6812");
          Serial.println("Color Order      : GRBW (fixed, not LED_COLOR_ORDER-configurable -- see CONTEXT.md)");
        #else
          Serial.println("RGB (3 bytes/LED)");
          Serial.println("Chipset          : WS2812B");
          Serial.println("Color Order      : GRB");
        #endif
        Serial.print("MCU ID           : ");
        Serial.println(mcuId);
        Serial.println("=================================");
        Serial.println();
        break;

      // Display curren configuration
      case 'D':
        Serial.println("Display configuration:");
        showConfiguration();
        break;

      // Set configuration
      case 'C':
        setConfigParameters(Data);
        break;

      // Store current configuration
      case 'S':
        if (Data[0] == 'e' || Data[0] == 'E') {
          // Se - Export configuration as hex string (field-wise wire format,
          // not a raw struct dump -- see encodeConfig())
          Serial.println();
          Serial.println("=== Configuration Export ===");
          Serial.print("CONFIG:");
          uint8_t configBytes[CONFIG_WIRE_SIZE];
          encodeConfig(LedConfig, configBytes);
          for (size_t i = 0; i < sizeof(configBytes); i++) {
            if (configBytes[i] < 16) Serial.print("0");
            Serial.print(configBytes[i], HEX);
          }   
          Serial.println();
          Serial.println("============================");
          Serial.println("Copy the CONFIG: line to backup this configuration.") ;
          Serial.println("Use 'Li:CONFIG:<hex>' to restore it.");
          Serial.println();
        } else {
          // S - Save to flash
          Serial.print("Save configuration: " );
          if (saveConfiguration()) Serial.println("Success." );
          else                     Serial.println("Failed..." );
        }
        break;

      // Load configuration from Flash/EEPROM
      case 'L':
        if (Data[0] == 'i' || Data[0] == 'I') {
          // Li - Import configuration from hex string
          if (Data[1] == ':' && strncmp(Data+2, "CONFIG:", 7) == 0) {
            char* hexData = Data + 9;
            size_t hexLen = strlen(hexData);
            size_t expectedLen = CONFIG_WIRE_SIZE * 2;

            if (hexLen == expectedLen) {
              // Validate hex characters before importing
              bool validHex = true;
              for (size_t i = 0; i < hexLen; i++) {
                char c = hexData[i];
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                  validHex = false;
                  Serial.print("ERROR: Invalid hex character '");
                  Serial.print(c);
                  Serial.print("' at position ");
                  Serial.println(i);
                  errorCount++;
                  break;
                }
              }

              if (validHex) {
                // Decode the field-wise wire format into a temporary
                // struct and validate before touching the live configuration
                uint8_t configBytes[CONFIG_WIRE_SIZE];
                for (size_t i = 0; i < sizeof(configBytes); i++) {
                  char byteStr[3] = {hexData[i*2], hexData[i*2+1], '\0'};
                  configBytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
                }

                LedData temp;
                if (!decodeConfig(configBytes, sizeof(configBytes), temp)) {
                  Serial.println("ERROR: Configuration decode failed (corrupt wire format).");
                  errorCount++;
                } else {
                  if (validateConfig(temp)) {
                    Serial.println("WARNING: Imported configuration contained out-of-range values; corrected to safe defaults.");
                  }

                  // FastLED controllers are registered once at boot with the
                  // running numLedsPerChannel/channelGPIOpin[]; if the import
                  // changed either, treat it the same as Cl/Cx and ask
                  // before saving/rebooting instead of only the generic
                  // save-or-reboot reminder below.
                  bool rebootNeeded = (temp.numLedsPerChannel != LedConfig.numLedsPerChannel) ||
                      (memcmp(temp.channelGPIOpin, LedConfig.channelGPIOpin, sizeof(temp.channelGPIOpin)) != 0);

                  LedConfig = temp;

                  Serial.println("Configuration imported successfully!");
                  if (rebootNeeded) {
                    Serial.println("NOTE: LED count and/or GPIO pin assignments changed; FastLED controllers are fixed at boot.");
                    promptSaveAndReboot();
                  } else {
                    Serial.println("Use 'S' to save to flash, or 'R' to reboot and discard.");
                  }
                }
              }
            } else {
              Serial.print("ERROR: Invalid hex length. Expected ");
              Serial.print(expectedLen);
              Serial.print(" chars, got ");
              Serial.println(hexLen);
              errorCount++;
            }
          } else {
            Serial.println("ERROR: Format must be Li:CONFIG:<hex_string>");
            errorCount++;
          }
        } else {
          // L - Load from flash
          Serial.print("Load configuration: " );
          loadConfiguration();
        }
        break;

      // Set Groups state
      case 'T':
        parseResult = sscanf(Data, "%4d:%d", &groupID, &state);
        if (parseResult == 2) {
          if (isValidGroup(groupID)) {
            if (isValidState(state)) {
              TermState[groupID -1] = state;
              TermPct[groupID -1] = 100;
              sprintf(output,"Group %d state set to %d", groupID, state);
              Serial.println(output);
            } else {
              Serial.print("ERROR: Invalid state '");
              Serial.print(state);
              Serial.println("', use 0-9");
              errorCount++;
            }
          } else {
            Serial.print("ERROR: Invalid Group-ID '");
            Serial.print(groupID);
            Serial.print("', use 1-");
            Serial.println(LedConfig.numChannels * LedConfig.numGroupsPerChannel);
            errorCount++;
          }
        } else {
          Serial.println("Syntax error: Use T<Group-ID>:<STATE>");
          errorCount++;
        }
        break;

      // Set Group state
      case 'P':
        parseResult = sscanf(Data, "%4d:%d:%4d", &groupID, &state, &pct);
        if (parseResult == 3) {
          if (isValidGroup(groupID)) {
            if (isValidState(state)) {
              if (isValidPercent(pct)) {
                TermState[groupID -1] = state;
                TermPct[groupID -1] = uint8_t(pct);
                sprintf(output,"Group %d state set to %d with progress %d%%", groupID, state, pct);
                Serial.println(output);
              } else {
                Serial.print("ERROR: Invalid percentage '");
                Serial.print(pct);
                Serial.println("', use 0-100");
                errorCount++;
              }
            } else {
              Serial.print("ERROR: Invalid state '");
              Serial.print(state);
              Serial.println("', use 0-9");
              errorCount++;
            }
          } else {
            Serial.print("ERROR: Invalid Group-ID '");
            Serial.print(groupID);
            Serial.print("', use 1-");
            Serial.println(LedConfig.numChannels * LedConfig.numGroupsPerChannel);
            errorCount++;
          }
        } else {
          Serial.print("Syntax error: Use Pgg:s:ppp (gg=group 1-");
          Serial.print(MAX_GROUPS);
          Serial.println(", s=state 0-9, ppp=percent 0-100)");
          errorCount++;
        }
        break;

      // Set state for all Group
      case 'A':
        parseResult = sscanf(Data, ":%d", &state);
        if (parseResult == 1) {
          if (isValidState(state)) {
            Serial.print("All groups set to state " );
            Serial.println(state);
            for(int groupID = 1; groupID <= MAX_GROUPS; groupID++) {
              TermState[groupID -1 ] = state;
              TermPct[groupID -1] = 100;
            }
          } else {
            Serial.print("ERROR: Invalid state '");
            Serial.print(state);
            Serial.println("', use 0-9");
            errorCount++;
          }
        } else {
          Serial.println("ERROR: Syntax error, use A:<STATE>");
          errorCount++;
        }
        break;

      // Set mass state, a digit for each group (48 max)
      case 'M':
        if ( Data[0] == ':') {
          int groupID=1;
          int totalChars = strlen(Data+1);
          int appliedCount = 0;
          int invalidCount = 0;

          while ( Data[groupID] != 0 && groupID<=MAX_GROUPS ) {
            if (Data[groupID] >= '0' && Data[groupID] <= '9') {
              TermState[groupID -1] = Data[groupID] - '0';
              TermPct[groupID -1] = 100;
              appliedCount++;
            } else {
              invalidCount++;
            }
            groupID++;
          }

          // Warn if there are surplus Values
          if (totalChars > MAX_GROUPS) {
            Serial.print("WARNING: ");
            Serial.print(totalChars - MAX_GROUPS);
            Serial.print(" surplus Values ignored (max ");
            Serial.print(MAX_GROUPS);
            Serial.println(" groups)");
          }

          // Warn (but do not fail the command) on non-digit characters;
          // those group states are left unchanged.
          if (invalidCount > 0) {
            Serial.print("WARNING: ");
            Serial.print(invalidCount);
            Serial.println(" non-digit character(s) ignored (use 0-9 only)");
          }

          Serial.print("Set ");
          Serial.print(appliedCount);
          Serial.println(" group states");
        } else {
          Serial.println("Syntax error: Use M:<STATE><STATE<<STATE>...");
          errorCount++;
        }
        break;

      // Reset all states to off
      case 'X':
        Serial.println("Reset all Group states.");
        for(int groupID = 0; groupID < MAX_GROUPS; groupID++) {
          TermState[groupID] = 0;
          TermPct[groupID] = 100;
        }
        setAllLEDs(CRGB::Black);
        break;

      // Show startup loop
      case 'W':
        Serial.println("Showing startup loop.");
        StartupLoop();
        break;

      // Memory usage report
      case 'I':
        {
          Serial.println();
          Serial.println("=== System Information ===");
          Serial.print("Free RAM         : ");
          Serial.print(rp2040.getFreeHeap());
          Serial.println(" bytes");
          Serial.print("Total RAM        : ");
          Serial.print(rp2040.getTotalHeap());
          Serial.println(" bytes");
          Serial.print("Used RAM         : ");
          Serial.print(rp2040.getUsedHeap());
          Serial.println(" bytes");
          FSInfo fs_info;
          LittleFS.info(fs_info);
          Serial.print("Flash Used       : ");
          Serial.print(fs_info.usedBytes);
          Serial.println(" bytes");
          Serial.print("Flash Total      : ");
          Serial.print(fs_info.totalBytes);
          Serial.println(" bytes");

          // Uptime and statistics
          uint32_t uptime = (millis() - bootTime) / 1000;
          uint32_t days = uptime / 86400;
          uint32_t hours = (uptime % 86400) / 3600;
          uint32_t minutes = (uptime % 3600) / 60;
          uint32_t seconds = uptime % 60;

          Serial.print("Uptime           : ");
          if (days > 0) {
            Serial.print(days);
            Serial.print("d ");
          }
          Serial.print(hours);
          Serial.print("h ");
          Serial.print(minutes);
          Serial.print("m ");
          Serial.print(seconds);
          Serial.println("s");

          Serial.print("Commands         : ");
          Serial.println(CommandCount);
          Serial.print("Errors           : ");
          Serial.println(errorCount);
          Serial.println("==========================");
          Serial.println();

          // Count groups in each state (only the currently configured groups,
          // not the full MAX_GROUPS allocation)
          const int configuredGroups = LedConfig.numChannels * LedConfig.numGroupsPerChannel;
          int stateCounts[10] = {0};
          int activeGroups = 0;

          for (int i = 0; i < configuredGroups; i++) {
            if (TermState[i] <= 9) {
              stateCounts[TermState[i]]++;
              if (TermState[i] > 0) activeGroups++;
            }
          }

          Serial.print("Active groups    : ");
          Serial.print(activeGroups);
          Serial.print(" / ");
          Serial.println(configuredGroups);

          Serial.println();
          Serial.println("Groups per state:");
          for (int state = 0; state <= MAX_STATE; state++) {
            if (stateCounts[state] > 0) {
              Serial.print("  state ");
              Serial.print(state);
              Serial.print(": ");
              Serial.print(stateCounts[state]);
              Serial.print(" group");
              if (stateCounts[state] != SINGLE_GROUP) Serial.print("s");
              Serial.println();
            }
          }

          Serial.println(); 
          Serial.println("=== Group states ===");

          for(int Index = 0; Index < LedConfig.numChannels; Index++) {
            // Use manual channel order mapping
            int channelIdx = LedConfig.channelOrder[Index] - 1;

            Serial.print("Channel ");
            Serial.print(channelIdx + 1);
            Serial.print(" (Groups ");
            sprintf(output, "%2d",Index*LedConfig.numGroupsPerChannel+1);
            Serial.print(output);
            Serial.print("-");
            sprintf(output, "%2d",Index*LedConfig.numGroupsPerChannel+LedConfig.numGroupsPerChannel);
            Serial.print(output);
            Serial.print("): ");

            for(int term = 0; term < LedConfig.numGroupsPerChannel; term++) {
              int groupIdx = (Index*LedConfig.numGroupsPerChannel)+term;
              Serial.print(TermState[groupIdx]);
              if (TermPct[groupIdx] < 100) {
                Serial.print("(");
                Serial.print(TermPct[groupIdx]);
                Serial.print("%)");
              }
              Serial.print(' ');
            }
              Serial.println();
          }
          Serial.println();
        }
        break;

      // Reboot controller
      case 'R':
        Serial.println("Rebooting controller...");
        rebootMCU();
        break;

      // Format LittleFS (destructive; requires explicit confirmation).
      // Needed after a mount failure, since setup() no longer auto-formats.
      case 'F':
        if (strcmp(Data, ":YES") == 0) {
          Serial.println("Formatting LittleFS...");
          if (LittleFS.format()) {
            Serial.println("LittleFS formatted successfully. Rebooting to remount...");
            delay(200);
            rebootMCU();
          } else {
            Serial.println("LittleFS formatting failed.");
            errorCount++;
          }
        } else {
          Serial.println("WARNING: This erases all saved configuration.");
          Serial.println("Use 'F:YES' to confirm formatting LittleFS.");
        }
        break;

      default:
        Serial.print("ERROR: Command '");
        Serial.print(command);
        Serial.println("' unknown. Use H for help.");
        errorCount++;
        break;
    }
  } else {
    Serial.print("SYNTAX ERROR: '");
    Serial.print(input[0]);
    Serial.println("' is not a valid command. Use A-Z commands only, H for help.");
    errorCount++;
  }
}


/**
 * Validate and sanitize channel order string
 * @param orderStr Channel order string (e.g., "12345678", "15263748", "4321")
 * @param orderArray Output array to store validated channel order
 * @param numChannels Number of active channels
 * @return true if valid, false if invalid
 */
bool validateChannelOrder(const char* orderStr, uint8_t* orderArray, uint8_t numChannels) {
  size_t len = strlen(orderStr);

  // Check if length is reasonable (1-8 characters)
  if (len == 0 || len > 8) {
    Serial.println("ERROR: Channel order must be 1-8 digits");
    return false;
  }

  // Temporary array to track used channels
  bool usedChannels[9] = {false}; // Channels 1-8

  // Parse each character
  for (size_t i = 0; i < len; i++) {
    char c = orderStr[i];

    // Check if character is a digit
    if (c < '1' || c > '8') {
      Serial.print("ERROR: Invalid channel '");
      Serial.print(c);
      Serial.println("' - must be 1-8");
      return false;
    }

    uint8_t channel = c - '0';

    // Check for duplicates
    if (usedChannels[channel]) {
      Serial.print("ERROR: Duplicate channel '");
      Serial.print(channel);
      Serial.println("' in order");
      return false;
    }

    usedChannels[channel] = true;
    orderArray[i] = channel;
  }

  // If less than 8 channels specified, append missing channels in numeric order
  if (len < 8) {
    Serial.print("WARNING: Only ");
    Serial.print(len);
    Serial.print(" channel(s) specified, appending missing channels: ");

    size_t currentIndex = len;
    for (uint8_t ch = 1; ch <= 8 && currentIndex < 8; ch++) {
      if (!usedChannels[ch]) {
        orderArray[currentIndex++] = ch;
        Serial.print(ch);
      }
    }
    Serial.println();
  }

  // Validate that we have exactly numChannels unique channels
  for (uint8_t i = 0; i < numChannels; i++) {
    bool found = false;
    for (uint8_t j = 0; j < 8; j++) {
      if (orderArray[j] == i + 1) {
        found = true;
        break;
      }
    }
    if (!found) {
      Serial.print("ERROR: Channel ");
      Serial.print(i + 1);
      Serial.println(" not found in order but numChannels requires it");
      return false;
    }
  }

  return true;
}


/**
 * Ask the operator whether to save and reboot now, for a config change
 * that needs a reboot to fully take effect. Never saves or reboots on its
 * own -- only sets a flag so the next line of input is interpreted as the
 * Y/N answer (see checkInput()).
 */
void promptSaveAndReboot() {
  pendingRebootConfirm = true;
  Serial.println("Save configuration and reboot now to apply this change? (Y/N)");
}

/**
 * Reboot the microcontroller
 * Uses watchdog timer with minimum timeout to force immediate reboot
 */
void rebootMCU() {
  watchdog_enable(1, 1);  // Timeout is set to the minimum Value, which is 1ms
  while (true) {
    delay(10);
  }
}

/**
 * Display help information for all available Commands
 * Shows comprehensive Command reference with usage examples
 */
void showHelp() {
  Serial.println();
  Serial.println("=== SIGHT Command Reference ===");
  Serial.println("  V                             Show version information");
  Serial.println("  H                             Show this help");
  Serial.println("  D                             Display current configuration");
  Serial.println("  I                             Show system info and group states");
  Serial.println("  S                             Save configuration to flash");
  Serial.println("  Se                            Save/Export configuration as hex (backup)");
  Serial.println("  L                             Load configuration from flash");
  Serial.println("  Li:CONFIG:                    Load/Import configuration from hex (restore)");
  Serial.println("  R                             Reboot controller");
  Serial.println("  F:YES                         Format LittleFS (destructive; only needed after a mount failure)");
  Serial.println("  W                             Show startup loop animation");
  Serial.println();

  Serial.println("Group Control:");
  Serial.print  ("  T<groupID>:<state>            Set Group state. groupID: 1-");
  Serial.print  (MAX_GROUPS);
  Serial.println(" and state: 0-9");
  Serial.print  ("  P<groupID>:<state>:<Pct>      Set Group state. groupID: 1-");
  Serial.print  (MAX_GROUPS);
  Serial.println(", state: 0-9, PCt=0-100% progress");
  Serial.println("  M:<state><state>...           Set state for multiple Groups sequentially (e.g. '113110')");
  Serial.println("  A:<state>                     Set state for all Groups, state (0-9)");
  Serial.println("  X                             Set all states to off (same as 'A:0')");
  Serial.println();

  Serial.println("Configuration (C prefix):");
  Serial.println("  Cn:<string>                   Set Controller name (ID) (1-16 chars)");
  Serial.print  ("  Cl:<Value>                    Set amount of LEDs per channel (");
  Serial.print  (NUM_LEDS_PER_CHANNEL_MIN);
  Serial.print  ("-");
  Serial.print  (NUM_LEDS_PER_CHANNEL_MAX);
  Serial.println(") [reboot required to fully take effect]");
  Serial.print  ("  Ct:<Value>                    Set amount of groups per channel (1-");
  Serial.print  (NUM_GROUPS_PER_CHANNEL_MAX);
  Serial.println(")");
  Serial.print  ("  Cs:<Value>                    Set amount of active channels (1-");
  Serial.print  (NUM_CHANNELS_MAX);
  Serial.println(")");
  Serial.print  ("  Cw:<Value>                    Set spacer-width (LEDs between groups, 0-");
  Serial.print  (SPACER_WIDTH_MAX);
  Serial.println(")");
  Serial.print  ("  Co:<Value>                    Set starting offset (skipping leds at start of channel, 0-");
  Serial.print  (START_OFFSET_MAX);
  Serial.println(")");
  Serial.print  ("  Cb:<Value>                    Set blink-interval in msec (");
  Serial.print  (BLINK_INTERVAL_MIN);
  Serial.print  ("-");
  Serial.print  (BLINK_INTERVAL_MAX);
  Serial.println(")");
  Serial.print  ("  Cu:<Value>                    Set update interval in mSec (");
  Serial.print  (UPDATE_INTERVAL_MIN);
  Serial.print  ("-");
  Serial.print  (UPDATE_INTERVAL_MAX);
  Serial.println(")");
  Serial.print  ("  Ca:<Value>                    Set animate-interval in msec (");
  Serial.print  (ANIMATE_INTERVAL_MIN);
  Serial.print  ("-");
  Serial.print  (ANIMATE_INTERVAL_MAX);
  Serial.println(")");
  Serial.println("  Ci:<Value>                    Set brightness intensity (0-255)");
  Serial.println("  Cf:<anim>:<fade-in>:<fade-out> Configure animation + 2-step fade-in/out (0-255)");
  Serial.println("  Cc:<state>:<Value>            Set color for state in HEX RGB order (state 1-9, Value: RRGGBB)");
  Serial.print  ("  Cp:<state>:<pattern>          Set display-pattern for state (state: 1-9, pattern 0-");
  Serial.print  (PATTERN_MAX);
  Serial.println(") [for colorblind assist]");
  Serial.println("  Cz:<order>                    Set channel order (N=standard 12345678, or custom like 43215678)");
  Serial.println("  C4:<yes/true/no/false>        Set RGBW leds (4bytes) instead of RGB (3bytes) (False/True)");
  Serial.print  ("  Cx:<channel>:<gpio-pin>       Set GPIO pin (");
  Serial.print  (GPIO_PIN_MIN);
  Serial.print  ("-");
  Serial.print  (GPIO_PIN_MAX);
  Serial.println(") per channel (1-8)");
  Serial.println("  Ce:<Y/N>                      Enable/disable local echo (character echo while typing)");
  Serial.println("  Cd                            Reset all settings to factory defaults");
  Serial.println();
  Serial.println();
  Serial.println("  L                             Load stored configuration from EEPROM/FLASH");
  Serial.println("  S                             Save configuration to EEPROM/FLASH");
}

/**
 * Update all LED groups based on current state and percentage
 * Iterates through all groups and applies their current state settings
 */
void updateGroups() {
  // Loop over all groups and adapt state
  for (int i = 0; i < LedConfig.numGroupsPerChannel * LedConfig.numChannels; i++) {
      setLEDGroup(i, TermState[i],TermPct[i]);
  }
}

/**
 * Gradually fades a pixel toward black.
 * @param pixel Reference to LED pixel being faded
 * @param amount Fade intensity (0=no fade, higher=faster fade)
 */
inline void fadeLedPixel(LedPixel &pixel, uint8_t amount) {
#if LED_TYPE == 4
  CRGB rgb(pixel.r, pixel.g, pixel.b);
  rgb.fadeToBlackBy(amount);
  pixel = rgb;
#else
  pixel.fadeToBlackBy(amount);
#endif
  ZERO_W(pixel);
}

/**
 * Moves a pixel toward a target color using a percentage of the step interval.
 * @param pixel Reference to LED pixel being updated
 * @param target Target CRGB color to approach
 * @param percent Portion (0-100) of the step time used for this transition
 */
inline void rampPixelToward(LedPixel &pixel, const CRGB &target, uint8_t percent) {
  auto stepChannel = [percent](uint8_t &current, uint8_t targetValue) {
    if (percent == 0) {
      current = targetValue;
      return;
    }

    int16_t difference = static_cast<int16_t>(targetValue) - static_cast<int16_t>(current);
    if (difference == 0) {
      return;
    }

    int32_t step = ((difference < 0 ? -difference : difference) * percent) / 100;
    if (step == 0) {
      step = 1;
    }

    if (difference > 0) {
      current = (current + step > targetValue) ? targetValue : current + step;
    } else {
      current = (current < step) ? 0 : current - step;
      if (current < targetValue) {
        current = targetValue;
      }
    }
  };

  stepChannel(pixel.r, target.r);
  stepChannel(pixel.g, target.g);
  stepChannel(pixel.b, target.b);
  ZERO_W(pixel);
}

/**
 * Applies two-step fade behavior for turning LEDs on/off.
 * @param pixel Reference to LED pixel to update
 * @param turnOn True when activating (fade-in), false when deactivating (fade-out)
 * @param color Target color when turning on
 */
inline void applyTwoStepPixel(LedPixel &pixel, bool turnOn, const CRGB &color) {
  if (turnOn) {
    if (LedConfig.fading2StepIn == 0) {
      pixel = color;
      ZERO_W(pixel);
    } else {
      rampPixelToward(pixel, color, LedConfig.fading2StepIn);
    }
  } else {
    if (LedConfig.fading2StepOut == 0) {
      pixel = CRGB::Black;
      ZERO_W(pixel);
    } else {
      rampPixelToward(pixel, CRGB::Black, LedConfig.fading2StepOut);
    }
  }
}

/**
 * Set LEDs for a specific group based on state, pattern, and percentage
 * @param group Group number (0 to numChannels*numGroupsPerChannel-1)
 * @param state Display state (0-9)
 * @param Pct Percentage fill (0-100)
 */
void setLEDGroup(uint16_t group, uint8_t state, uint8_t Pct) {
  int pattern = 0, channelIndex = 0, groupIndex = 0, startLEDIndex = 0, groupWidth = 0;

  // Defensive validation: reject out-of-range inputs at the rendering boundary
  if (state > MAX_STATE) return;
  if (group >= (uint16_t)LedConfig.numChannels * LedConfig.numGroupsPerChannel) return;

  pattern = LedConfig.state_pattern[state];
  if (pattern > PATTERN_MAX) pattern = 0;  // Corrupt config: fall back to solid

  channelIndex = group / LedConfig.numGroupsPerChannel;

  // Use manual channel order mapping
  if (channelIndex >= NUM_CHANNELS_MAX) return; // Invalid channel, skip
  channelIndex = LedConfig.channelOrder[channelIndex] - 1; // Convert to 0-based index
  if (channelIndex < 0 || channelIndex >= NUM_CHANNELS_MAX) return; // Corrupt channel order

  groupIndex = group % LedConfig.numGroupsPerChannel;
  const int segmentWidth = LedConfig.numLedsPerChannel / LedConfig.numGroupsPerChannel;
  if (segmentWidth <= 0) return; // More groups than LEDs: nothing to draw

  startLEDIndex = groupIndex * segmentWidth + LedConfig.startOffset;
  groupWidth = segmentWidth - LedConfig.spacerWidth;

  // Bounds check: ensure we don't write past the configured strip or the LED array
  if (startLEDIndex < 0 || startLEDIndex >= NUM_LEDS_PER_CHANNEL_MAX) return;
  if (startLEDIndex + groupWidth > NUM_LEDS_PER_CHANNEL_MAX) {
    groupWidth = NUM_LEDS_PER_CHANNEL_MAX - startLEDIndex;
  }
  if (groupWidth < 1) return; // Zero/negative width: no room to draw
  if ((pattern == PATTERN_CHASE_IN || pattern == PATTERN_CHASE_OUT) && groupWidth < 2) return;
  const int fullGroupWidth = groupWidth;

  // Clear entire group only when no fade is configured (legacy behavior)
  if (pattern < 8 && LedConfig.fading2StepOut == 0) {
    for(int i = 0; i < fullGroupWidth; i++) {
      leds[channelIndex][startLEDIndex + i] = CRGB::Black;
      ZERO_W(leds[channelIndex][startLEDIndex + i]);
    }
  }

  // for percentage
  bool isPartialFill = (Pct < 100);
  if (Pct < 100) {
    float factor = float(Pct) / 100.0f;
    groupWidth = int(float(groupWidth) * factor + 0.5f);
    if (groupWidth == 0 && Pct > 0) {
      groupWidth = 1;  // only no leds on 0%
    }
  }

  // Re-check after partial fill: a 0% group or a 1-LED group must not reach
  // patterns that divide by groupWidth or groupWidth/2
  if (groupWidth < 1) return;
  if ((pattern == PATTERN_CHASE_IN || pattern == PATTERN_CHASE_OUT) && groupWidth < 2) return;

  switch (pattern) {
      case 0: boundAnimateStep(pattern, 1);
              // 0 solid no blink    [########]
              //                     [########]
              for(int i = 0; i < groupWidth; i++) {
                applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], true, LedConfig.state_color[state]);
              }
              break;

      case 1:	boundAnimateStep(pattern, 2);
              // 1 solid blink       [########]
              //                     [        ]
              for(int i = 0; i < groupWidth; i++) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], blinkState, LedConfig.state_color[state]);
              }
              break;

      case 2:	boundAnimateStep(pattern, 2);
              // 1 solid blink inv.  [        ]
              //                     [########]
              for(int i = 0; i < groupWidth; i++) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], !blinkState, LedConfig.state_color[state]);
              }
              break;

      case 3: boundAnimateStep(pattern, 2);
              // 2 Alternate L/R     [####    ] Count up c=0->1  s,s+(n/2) *c
              //                     [    ####]                  (n/2),n   *!c
              for(int i = 0; i < (groupWidth/2); i++) {
                  int activeIndex = startLEDIndex + i + ( blinkState * (groupWidth/2));
                  int inactiveIndex = startLEDIndex + i + (!blinkState * (groupWidth/2));
                  applyTwoStepPixel(leds[channelIndex][activeIndex], true, LedConfig.state_color[state]);
                  applyTwoStepPixel(leds[channelIndex][inactiveIndex], false, LedConfig.state_color[state]);
              }
              break;

      case 4: boundAnimateStep(pattern, 2);
              // 3 Alternate in/out  [##      ##] Count up c=0->1  s,s+(n/4) + n-(n/4),n
              //                     [  ##  ##  ]                  s+(n/4)-(n/2)+(n/4)
              for(int i = 0; i < (groupWidth); i++) {
                bool turnOn = (i < (groupWidth/4) || i >= (groupWidth-(groupWidth/4))) ? blinkState : !blinkState;
                applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], turnOn, LedConfig.state_color[state]);
              }
              break;

      case 5: boundAnimateStep(pattern, groupWidth);
              // 4 odd/even          [# # # # ]
              //                     [ # # # #]
              for(int i = 0; i < (groupWidth); i++) {
                  bool turnOn = (i % 2) ? blinkState : !blinkState;
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], turnOn, LedConfig.state_color[state]);
              }
              break;

      case 6: boundAnimateStep(pattern, 1);
              // 10 1/3 gated blink    [###  ###]
              for(int i = 0; i < groupWidth; i++) {
                if ( i <= (groupWidth/3) or i >= (groupWidth-(groupWidth/3)-1) ) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], true, LedConfig.state_color[state]);
                }
              }
              break;
      case 7: boundAnimateStep(pattern, 2);
              // gated blink    [###  ###]
              //                [        ]
              for(int i = 0; i < groupWidth; i++) {
                if ( i <= (groupWidth/3) or i >= (groupWidth-(groupWidth/3)-1) ) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], blinkState, LedConfig.state_color[state]);
                }
              }
              break;

      case 8: {
              uint8_t step = boundAnimateStep(pattern, groupWidth);
                // 5 Animate >         [#       ]
                //                     [ #      ]
                //                     [  #     ]
                //                     [   #    ]
                //                     [    #   ]
                //                     [     #  ]
                //                     [      # ]
                //                     [       #]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              leds[channelIndex][startLEDIndex + step ] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + step]);
              break;
              // if ( i/(groupWidth/2) == 0 )
              //   leds[channelIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              // break;
              }

      case 9: {
              uint8_t step = boundAnimateStep(pattern, groupWidth);
              // 6 Animate <         [       #]
              //                     [      # ]
              //                     [     #  ]
              //                     [    #   ]
              //                     [   #    ]
              //                     [  #     ]
              //                     [ #      ]
              //                     [#       ]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              leds[channelIndex][startLEDIndex + groupWidth - step - 1] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + groupWidth - step - 1]);
              break;
              }

      case 10: {
              uint8_t step = boundAnimateStep(pattern, groupWidth*2);
              // 7 Cyon/Kitt         [#       ]
              //                     [ #      ]
              //                     [  #     ]
              //                     [   #    ]
              //                     [    #   ]
              //                     [     #  ]
              //                     [      # ]
              //                     [       #]
              //                     [      # ]
              //                     [     #  ]
              //                     [    #   ]
              //                     [   #    ]
              //                     [  #     ]
              //                     [ #      ]
              //                     [#       ]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }

              if (step < groupWidth ) {
                leds[channelIndex][startLEDIndex + step] = LedConfig.state_color[state];
                ZERO_W(leds[channelIndex][startLEDIndex + step]);
              } else {
                leds[channelIndex][startLEDIndex + (groupWidth - (step - groupWidth)) - 1] = LedConfig.state_color[state];
                ZERO_W(leds[channelIndex][startLEDIndex + (groupWidth - (step - groupWidth)) - 1]);
              }
              break;
              }

      case 11: {
              uint8_t step = boundAnimateStep(pattern, groupWidth/2);
              // 8 Animate ><        [#      #]
              //                     [ #    # ]
              //                     [  #  #  ]
              //                     [   ##   ]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              leds[channelIndex][startLEDIndex + step] = LedConfig.state_color[state];
              leds[channelIndex][startLEDIndex+groupWidth - step -1] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + step]);
              ZERO_W(leds[channelIndex][startLEDIndex+groupWidth - step -1]);
              break;
              }

      case 12: {
              uint8_t step = boundAnimateStep(pattern, groupWidth/2);

              // 9 Animate ><        [   ##   ]
              //                     [  #  #  ]
              //                     [ #    # ]
              //                     [#      #]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              leds[channelIndex][startLEDIndex + (groupWidth/2) - step - 1] = LedConfig.state_color[state];
              leds[channelIndex][startLEDIndex + (groupWidth/2) + step] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + (groupWidth/2) - step - 1]);
              ZERO_W(leds[channelIndex][startLEDIndex + (groupWidth/2) + step]);
              break;
              }

    }

    if (isPartialFill) {
      for (int i = groupWidth; i < fullGroupWidth; i++) {
        applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], false, LedConfig.state_color[state]);
      }
    }
}

/**
 * Set all LEDs across all channels to a specific color
 * @param color CRGB color Value to apply to all LEDs
 */
void setAllLEDs(CRGB color) {
  for(int n = 0; n < LedConfig.numChannels; n++) {
    for(int i = 0; i < LedConfig.numLedsPerChannel; i++) {
      leds[n][i] = color;
      ZERO_W(leds[n][i]);
    }
  }
  FastLED.show();
}

/**
 * Validate and clamp integer value to specified range
 * @param value Value to validate
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @param defaultValue Default value if out of range
 * @return Clamped value
 */
int validateRange(int value, int min, int max, int defaultValue) {
  if (value < min || value > max) {
    return defaultValue;
  }
  return value;
}

/**
 * Validate group ID is within acceptable range
 * @param groupID Group ID to validate
 * @return true if valid, false otherwise
 */
bool isValidGroup(int groupID) {
  return (groupID >= MIN_GROUP_ID &&
          groupID <= LedConfig.numChannels * LedConfig.numGroupsPerChannel &&
          groupID <= MAX_GROUPS);
}

/**
 * Validate state value is within acceptable range
 * @param state State value to validate
 * @return true if valid, false otherwise
 */
bool isValidState(int state) {
  return (state >= 0 && state <= MAX_STATE_VALUE);
}

/**
 * Validate percentage value is within acceptable range
 * @param percent Percentage value to validate
 * @return true if valid, false otherwise
 */
bool isValidPercent(int percent) {
  return (percent >= 0 && percent <= 100);
}

/**
 * Strictly parse a decimal integer, requiring the whole string to be consumed
 * Used instead of atoi() for configuration setters so empty input, trailing
 * junk ("12abc"), and out-of-range values do not silently become 0.
 * @param str Input string to parse
 * @return Parsed value, or LONG_MIN if str is null, empty, malformed, has
 *         trailing characters, or overflows
 */
long strictParseLong(const char *str) {
  if (str == NULL || *str == '\0') return LONG_MIN;
  char *endPtr = NULL;
  errno = 0;
  long value = strtol(str, &endPtr, 10);
  if (endPtr == str || *endPtr != '\0' || errno == ERANGE) return LONG_MIN;
  return value;
}

/**
 * Update system status LED pattern based on current system state
 * Uses dimmed brightness (64) with glowing/blinking patterns to show MCU is active
 */
void updateSystemStatusLED() {
  uint32_t currentTime = millis();
  uint32_t interval;
  CRGB ledColor;

  switch (currentSystemState) {
    case SYSTEM_STARTUP:
      interval = CPULED_STARTUP_INTERVAL;
      ledColor = CRGB::Blue;
      // Pulsing blue effect while waiting for serial
      if (currentTime - lastCpuLedUpdate >= interval / 20) {
        if (cpuLedDirection) {
          cpuLedBrightness += 4;
          if (cpuLedBrightness >= CPULED_STATUS_BRIGHTNESS) {
            cpuLedBrightness = CPULED_STATUS_BRIGHTNESS;
            cpuLedDirection = false;
          }
        } else {
          cpuLedBrightness -= 4;
          if (cpuLedBrightness <= 0) {
            cpuLedBrightness = 0;
            cpuLedDirection = true;
          }
        }
        lastCpuLedUpdate = currentTime;
      }
      break;

    case SYSTEM_NORMAL:
      interval = CPULED_NORMAL_INTERVAL;
      ledColor = CRGB::Green;
      // Slow glowing green effect when running normally
      if (currentTime - lastCpuLedUpdate >= interval / 30) {
        if (cpuLedDirection) {
          cpuLedBrightness += 2;
          if (cpuLedBrightness >= CPULED_STATUS_BRIGHTNESS) {
            cpuLedBrightness = CPULED_STATUS_BRIGHTNESS;
            cpuLedDirection = false;
          }
        } else {
          cpuLedBrightness -= 2;
          if (cpuLedBrightness <= 0) {
            cpuLedBrightness = 0;
            cpuLedDirection = true;
          }
        }
        lastCpuLedUpdate = currentTime;
      }
      break;

    case SYSTEM_ERROR:
      interval = CPULED_ERROR_INTERVAL;
      ledColor = CRGB::Red;
      // Fast blinking red effect
      if (currentTime - lastCpuLedUpdate >= interval) {
        cpuLedBrightness = (cpuLedBrightness == 0) ? 32 : 0;
        lastCpuLedUpdate = currentTime;
      }
      break;

    default:
      ledColor = CRGB::Black;
      cpuLedBrightness = 0;
      break;
  }

  // Apply the color with current brightness.
  //
  // v1.11 previously tried a "only send if changed" optimization here
  // (avoid retransmitting an identical frame every loop() iteration).
  // That is REVERTED: it caused real, reproducible wrong colors on real
  // hardware (blue/green replaced by red-tinted colors, worse after
  // typing) across multiple boards, and a follow-up fix (centralizing the
  // last-sent-color cache in sendRGB_CPULED()) did not resolve it either.
  // Root cause not fully identified before reverting; unconditionally
  // sending every time, exactly as in v1.10, is the known-good behavior
  // confirmed correct on real hardware. Do not reintroduce a skip-if-
  // unchanged optimization here without hardware re-verification across
  // multiple physical boards, not just the pulse analyzer (the analyzer
  // only confirmed bit *timing*, not that the resulting visual color was
  // correct).
  CPULED(ledColor.nscale8(cpuLedBrightness));
}

/**
 * Set system state and update status LED pattern accordingly
 * @param state New system state (STARTUP, NORMAL, ERROR)
 */
void setSystemState(SystemState state) {
  currentSystemState = state;
  cpuLedBrightness = 0;
  cpuLedDirection = true;
  lastCpuLedUpdate = millis();
}

/**
 * Parse and set configuration parameters from Command input
 * @param Data Configuration Command string (format: "C<item>:<Value>")
 */
void setConfigParameters(char *Data) {
   char configItem = Data[0];
  char *Value = Data + 2;
  int ValueInt = 0;
  if (Data[1] == ':') {
    switch (configItem) {
      // set Name-Idenitfier
      case 'n':
        // Check is Value is not bigger than size of LedConfig.identifier
        if (strlen(Value) < IDENTIFIER_MAX_LENGTH ) {
          strncpy(LedConfig.identifier, Value, IDENTIFIER_MAX_LENGTH - 1);
          LedConfig.identifier[IDENTIFIER_MAX_LENGTH - 1] = '\0'; // Ensure null-termination
          Serial.print("Controller Name (ID)   : " );
          Serial.println(LedConfig.identifier);
        } else {
          Serial.println("Identifier too long, use 16 characters max.");
          errorCount++;
        }
        break;
      // set led per channel
      case 'l':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= NUM_LEDS_PER_CHANNEL_MIN and ValueInt <= NUM_LEDS_PER_CHANNEL_MAX) {
          Serial.print("LEDs per channel      : " );
          LedConfig.numLedsPerChannel = ValueInt;
          Serial.println(LedConfig.numLedsPerChannel);
          FastLED.clearData();
          Serial.println("NOTE: FastLED controller lengths are fixed at boot.");
          promptSaveAndReboot();
        } else {
          Serial.print("Invalid number of leds per channel(");
          Serial.print(NUM_LEDS_PER_CHANNEL_MIN);
          Serial.print("-");
          Serial.print(NUM_LEDS_PER_CHANNEL_MAX);
          Serial.println(")");
          errorCount++;
        }
        break;
      // set groups per channel
      case 't':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= 1 and ValueInt <= NUM_GROUPS_PER_CHANNEL_MAX) {
          // Check if total groups would exceed MAX_GROUPS
          if (LedConfig.numChannels * ValueInt > MAX_GROUPS) {
            Serial.print("ERROR: Channels (");
            Serial.print(LedConfig.numChannels);
            Serial.print(") * Groups Per Channel (");
            Serial.print(ValueInt);
            Serial.print(") = ");
            Serial.print(LedConfig.numChannels * ValueInt);
            Serial.print(" exceeds MAX_GROUPS (");
            Serial.print(MAX_GROUPS);
            Serial.println(")!");
            errorCount++;
          } else {
            Serial.print("Groups per channel : " );
            LedConfig.numGroupsPerChannel = ValueInt;
            Serial.println(LedConfig.numGroupsPerChannel);
            FastLED.clearData();
          }
        } else {
          Serial.print("Invalid number of groups per channel (1-");
          Serial.print(NUM_GROUPS_PER_CHANNEL_MAX);
          Serial.println(")");
          errorCount++;
        }
        break;
      // set number of channels
      case 's':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= 1 and ValueInt <= NUM_CHANNELS_MAX) {
          // Check if total groups would exceed MAX_GROUPS
          if (ValueInt * LedConfig.numGroupsPerChannel > MAX_GROUPS) {
            Serial.print("ERROR: Channels (");
            Serial.print(ValueInt);
            Serial.print(") * Groups Per Channel (");
            Serial.print(LedConfig.numGroupsPerChannel);
            Serial.print(") = ");
            Serial.print(ValueInt * LedConfig.numGroupsPerChannel);
            Serial.print(" exceeds MAX_GROUPS (");
            Serial.print(MAX_GROUPS);
            Serial.println(")!");
            errorCount++;
          } else {
            Serial.print("Amount of channels    : " );
            LedConfig.numChannels = ValueInt;
            Serial.println(LedConfig.numChannels);
            FastLED.clearData();
          }
        } else {
          Serial.print("Invalid number of channels (1-");
          Serial.print(NUM_CHANNELS_MAX);
          Serial.println(")");
          errorCount++;
        }
        break;
      // set spacer width
      case 'w':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= 0 and ValueInt <= SPACER_WIDTH_MAX) {
          Serial.print("Spacer width         : " );
          LedConfig.spacerWidth = ValueInt;
          Serial.println(LedConfig.spacerWidth);
          FastLED.clearData();
        } else {
          Serial.print("Invalid space width(0-");
          Serial.print(SPACER_WIDTH_MAX);
          Serial.println(")");
          errorCount++;
        }
        break;
      case 'o':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= 0 and ValueInt <= START_OFFSET_MAX) {
          Serial.print("Start offset         : " );
          LedConfig.startOffset = ValueInt;
          Serial.println(LedConfig.startOffset);
          FastLED.clearData();
        } else {
          Serial.print("Invalid start offset (0-");
          Serial.print(START_OFFSET_MAX);
          Serial.println(")");
          errorCount++;
        }
        break;
      // Set animate interval
      case 'a':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= ANIMATE_INTERVAL_MIN and ValueInt <= ANIMATE_INTERVAL_MAX) {
          Serial.print("Animation interval   : " );
          LedConfig.animateinterval = ValueInt;
          Serial.println(LedConfig.animateinterval);
          animate_Timer.detach();
          animate_Timer.attach_ms(LedConfig.animateinterval, &animateStep);
          FastLED.clearData();
        } else {
          Serial.print("Invalid animate interval (");
          Serial.print(ANIMATE_INTERVAL_MIN);
          Serial.print("-");
          Serial.print(ANIMATE_INTERVAL_MAX);
          Serial.println(" msec)");
          errorCount++;
        }
        break;
      // Set blink interval
      case 'b':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= BLINK_INTERVAL_MIN and ValueInt <= BLINK_INTERVAL_MAX) {
          if (ValueInt > LedConfig.updateinterval) {
            Serial.print("Blinking interval    : " );
            LedConfig.blinkinterval = ValueInt;
            Serial.println(LedConfig.blinkinterval);
            blink_Timer.detach();
            blink_Timer.attach_ms(LedConfig.blinkinterval, &setBlinkState);
            FastLED.clearData();
          } else {
            Serial.print("Invalid blinking-interval, needs to be bigger than current update-interval (");
            Serial.print(LedConfig.updateinterval);
            Serial.println(")");
            errorCount++;
          }
        } else {
          Serial.print("Invalid blink interval (");
          Serial.print(BLINK_INTERVAL_MIN);
          Serial.print("-");
          Serial.print(BLINK_INTERVAL_MAX);
          Serial.println(" msec)");
          errorCount++;
        }
          break;

      // Set update interval
      case 'u':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= UPDATE_INTERVAL_MIN and ValueInt <= UPDATE_INTERVAL_MAX) {
          if (ValueInt < LedConfig.blinkinterval) {
            Serial.print("Update interval      : ");
            LedConfig.updateinterval = ValueInt;
            Serial.println(LedConfig.updateinterval);
            setgroup_Timer.detach();
            setgroup_Timer.attach_ms(LedConfig.updateinterval*2, &setGroupState);
            update_Timer.detach();
            update_Timer.attach_ms(LedConfig.updateinterval, &writeChannelData);

            FastLED.clearData();
          } else {
            Serial.print("Invalid update-interval, needs to be smaller than current blink-interval (");
            Serial.print(LedConfig.blinkinterval);
            Serial.println(")");
            errorCount++;
          }
        } else {
          Serial.print("Invalid update interval (");
          Serial.print(UPDATE_INTERVAL_MIN);
          Serial.print("-");
          Serial.print(UPDATE_INTERVAL_MAX);
          Serial.println(" msec)");
          errorCount++;
        }
        break;

      // Set Brightness Inetensity
      case 'i':
        ValueInt = strictParseLong(Value);
        if (ValueInt >= STRIP_BRIGHTNESS_MIN and ValueInt <= STRIP_BRIGHTNESS_MAX) {
          Serial.print("Brightness intensity   : " );
          LedConfig.brightness = ValueInt;
          Serial.println(LedConfig.brightness);
          FastLED.setBrightness(LedConfig.brightness);
          FastLED.clearData();

          // Warn if brightness is very high
          if (ValueInt > STRIP_BRIGHTNESS_WARNING_THRESHOLD) {
            Serial.println("WARNING: High brightness may cause overheating or exceed power supply capacity!");
          }
        } else {
          Serial.print("Invalid brightness intensity (");
          Serial.print(STRIP_BRIGHTNESS_MIN);
          Serial.print("-");
          Serial.print(STRIP_BRIGHTNESS_MAX);
          Serial.println(")");
          errorCount++;
        }
        break;

      // Set fade factor
      case 'f':
        {
          // Cf:<anim>:<fade-in>:<fade-out>
          int values[3] = {LedConfig.fadingAnimation, LedConfig.fading2StepIn, LedConfig.fading2StepOut};
          char *token = strtok(Value, ":");
          int idx = 0;
          bool valid = true;
          while (token != NULL && idx < 3) {
            long parsed = strictParseLong(token);
            if (parsed < 0 || parsed > 255) {
              valid = false;
              break;
            }
            values[idx++] = parsed;
            token = strtok(NULL, ":");
          }

          if (!valid) {
            Serial.println("Invalid fade values (0-255)");
            errorCount++;
          } else if (idx == 0) {
            Serial.println("Usage: Cf:<anim>:<fade-in>:<fade-out>");
            errorCount++;
          } else if (idx == 1) {
            // Backwards compatibility: Cf:<Value>
            LedConfig.fadingAnimation = values[0];
            LedConfig.fading2StepIn = values[0] / 2;
            LedConfig.fading2StepOut = values[0] / 2;
            Serial.print("Fading factor          : ");
            Serial.print(LedConfig.fadingAnimation);
            Serial.print(" (2-step fade in/out: ");
            Serial.print(LedConfig.fading2StepIn);
            Serial.println(")");
            FastLED.clearData();
          } else if (idx == 2) {
            LedConfig.fadingAnimation = values[0];
            LedConfig.fading2StepIn = values[1];
            LedConfig.fading2StepOut = values[1];
            Serial.print("Animation fading factor: ");
            Serial.print(LedConfig.fadingAnimation);
            Serial.print(", 2-step fade (in/out): ");
            Serial.println(LedConfig.fading2StepIn);
            FastLED.clearData();
          } else if (idx == 3) {
            LedConfig.fadingAnimation = values[0];
            LedConfig.fading2StepIn = values[1];
            LedConfig.fading2StepOut = values[2];
            Serial.print("Animation fading factor: ");
            Serial.print(LedConfig.fadingAnimation);
            Serial.print(", 2-step fade in/out: ");
            Serial.print(LedConfig.fading2StepIn);
            Serial.print(" / ");
            Serial.println(LedConfig.fading2StepOut);
            FastLED.clearData();
          }
        }
        break;

      // Set channel-order
      case 'z':
        if (strcmp(Value, "N") == 0 || strcmp(Value, "n") == 0 || strcmp(Value, "F") == 0 || strcmp(Value, "f") == 0) {
          // Set to standard order (12345678)
          LedConfig.channelOrder[0] = 1; LedConfig.channelOrder[1] = 2; LedConfig.channelOrder[2] = 3; LedConfig.channelOrder[3] = 4;
          LedConfig.channelOrder[4] = 5; LedConfig.channelOrder[5] = 6; LedConfig.channelOrder[6] = 7; LedConfig.channelOrder[7] = 8;
          Serial.println("Channel order set to: 12345678 (standard)");
          FastLED.clearData();
        } else {
          // Try to parse as custom channel order
          uint8_t tempOrder[8];
          if (validateChannelOrder(Value, tempOrder, LedConfig.numChannels)) {
            // Copy validated order to config
            for (int i = 0; i < 8; i++) {
              LedConfig.channelOrder[i] = tempOrder[i];
            }
            Serial.print("Channel order set to: ");
            for (int i = 0; i < 8; i++) {
              Serial.print(LedConfig.channelOrder[i]);
            }
            Serial.println();
            FastLED.clearData();
          } else {
            Serial.println("Invalid channel order. Use: N (standard 12345678) or custom like 43215678");
            errorCount++;
          }
        }
        break;

      // Toggle startup animation
      case 'g':
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.startupAnimation = false;
          Serial.println("Startup animation    : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.startupAnimation = true;
          Serial.println("Startup animation    : Enabled");
        } else {
          Serial.println("Invalid Value, use Y/N or 1/0");
          errorCount++;
        }
        break;

      // Toggle local echo (character echo while typing)
      case 'e':
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.localEcho = false;
          Serial.println("Local echo           : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.localEcho = true;
          Serial.println("Local echo           : Enabled");
        } else {
          Serial.println("Invalid Value, use Y/N or 1/0");
          errorCount++;
        }
        break;

      // Set color-pattern (colorblind assist)
      case 'p':
        setLedstatePattern(Value);
        FastLED.clearData();
        break;
      // set colors
      case 'c':
        setLedstateColor(Value);
        FastLED.clearData();
        break;
      // Set GPIO pins
      case 'x':
        setLedStripGPIO(Value);
        FastLED.clearData();
        break;
      // set defaults
      case 'd':
        resetToDefaults();
        Serial.println("Configuration reset to defaults");
        Serial.println();
        FastLED.clearData();
        break;
      default:
        Serial.print("SYNTAX ERROR: Configuration item '");
        Serial.print(configItem);
        Serial.println("' unknown. Use H for help.");
        errorCount++;
        break;
    }
  } else {
    Serial.println("SYNTAX ERROR: Invalid configuration format. Use C<item>:<value> format, H for help.");
    errorCount++;
  }
}

/**
 * Set GPIO pins for LED channels from Command input
 * @param Value Comma-separated list of GPIO pin numbers (e.g., "2,3,4,5,6,7,8,9")
 */
void setLedStripGPIO(char *Value) {
  if (Value[1] == ':') {
    char channelChar = Value[0];
    // Validate the channel digit explicitly before converting, so a
    // non-digit character is rejected instead of wrapping around as an
    // unsigned value.
    if (channelChar < '1' || channelChar > ('0' + NUM_CHANNELS_MAX)) {
      Serial.print  ("Invalid channel, 1-");
      Serial.print  (NUM_CHANNELS_MAX);
      Serial.println(" only.");
      errorCount++;
    } else {
      uint8_t channel = (uint8_t)(channelChar - '1');
      char *GPIO_RAW = Value + 2;
      long gpioLong = strictParseLong(GPIO_RAW);
      if (gpioLong >= GPIO_PIN_MIN && gpioLong <= GPIO_PIN_MAX) {
        uint8_t GPIO_PIN = (uint8_t)gpioLong;

        // Test if GPIO pin is not assigned already
        bool conflict = false;
        for (uint8_t CHANNEL=0; CHANNEL<NUM_CHANNELS_DEFAULT ; CHANNEL++) {
          if (GPIO_PIN == LedConfig.channelGPIOpin[CHANNEL]) {
            Serial.print("ERROR: GPIO-PIN ");
            Serial.print(GPIO_PIN);
            Serial.print(" is already used for channel ");
            Serial.print(CHANNEL + 1);
            Serial.println(" !");
            errorCount++;
            conflict = true;
            break;
          }
        }
        if (!conflict && GPIO_PIN == CPULED_GPIO) {
          Serial.print("ERROR: GPIO-PIN ");
          Serial.print(GPIO_PIN);
          Serial.println(" is already used for CPULED !");
          errorCount++;
          conflict = true;
        }

        if (!conflict) {
          LedConfig.channelGPIOpin[channel] = GPIO_PIN;
          Serial.print("GPIO-PIN for channel ");
          Serial.print(channel + 1);  // Display 1-based channel number
          Serial.print(" is set to : ");
          Serial.print(LedConfig.channelGPIOpin[channel]);
          Serial.println();
          Serial.println("A MCU reboot is required to activate a change in GPIO pin assignments.");
          promptSaveAndReboot();
        }
      } else {
        Serial.print("Invalid GPIO-PIN number, use decimal ");
        Serial.print(GPIO_PIN_MIN);
        Serial.print("-");
        Serial.println(GPIO_PIN_MAX);
        errorCount++;
      }
    }
  } else {
      Serial.print  ("Syntax error: use Cx:<channel>:<GPIO-PIN>     (<channel>: 1-");
      Serial.print  (NUM_CHANNELS_MAX);
      Serial.print  (", <GPIO-PIN>: ");
      Serial.print  (GPIO_PIN_MIN);
      Serial.print  ("-");
      Serial.print  (GPIO_PIN_MAX);
      Serial.println(")");
      Serial.println();
  }
}

/**
 * Set LED colors for specific states from Command input
 * @param Value Color configuration string (format: "<state>:<RRGGBB>")
 */
void setLedstateColor(char *Value) {
  char buffer[10];
  if (Value[1] == ':') {
    int state = Value[0] - '0';
    if (state > 0 && state <= 9) {
      char *Color = Value + 2;
      // Require exactly 6 hex digits, fully consumed, so trailing junk or a
      // short/long value can't be silently accepted as a different color.
      char *endPtr = NULL;
      uint32_t RGB = strtoul(Color, &endPtr, 16);
      if (strlen(Color) == 6 && endPtr == Color + 6 && RGB <= 0xFFFFFF) {
        LedConfig.state_color[state] = RGB + 0xFF000000; // Add brightness
        Serial.print("Color for state ");
        Serial.print(state);
        Serial.print(" is set to : ");
        snprintf(buffer, sizeof(buffer), "%02X%02X%02X", 
                 LedConfig.state_color[state].red, 
                 LedConfig.state_color[state].green, 
                 LedConfig.state_color[state].blue);
        Serial.print(buffer);
        Serial.println(" (RR GG BB)");
      } else {
        Serial.println("Invalid color, use exactly 6 hex digits (000000-FFFFFF)");
        errorCount++;
      }
    } else {
      Serial.println("Invalid state, use 1-9");
      errorCount++;
    }
  } else {
    Serial.println("Syntax error: use Cc:<state>:<RRGGBB>   (<state>: 1-9, <RRGGBB>: Color in Hex)");
    Serial.println();
    errorCount++;
  }
}

/**
 * Set LED display patterns for specific states from Command input
 * @param Value Pattern configuration string (format: "<state>:<pattern>")
 */
void setLedstatePattern(char *Value) {
// Value = s:pp
//         0123

  if (Value[1] == ':') {
    uint8_t state = Value[0] - '0';
    if (state >= 1 && state <= 9) {
      // Require the full remainder to be a valid integer (no trailing junk);
      // this no longer truncates the input buffer to parse.
      long pattern = strictParseLong(Value + 2);
      if (pattern >= 0 && pattern <= PATTERN_MAX) {
        LedConfig.state_pattern[state] = (uint8_t)pattern;
        Serial.print("Pattern for state ");
        Serial.print(state);
        Serial.print(" is set to : ");
        Serial.print(LedConfig.state_pattern[state]);
        Serial.println();
      } else {
        Serial.print("Invalid pattern, use 0-");
        Serial.println(PATTERN_MAX);
        errorCount++;
      }
    } else {
      Serial.println("Invalid state, use 1-9");
      errorCount++;
    }
  } else {
    Serial.print("Syntax error: use Cp:<state>:<pattern>   (<state>: 1-9, <pattern>: 0-");
    Serial.print(PATTERN_MAX);
    Serial.println(")");
    errorCount++;
  }
}

/**
 * Reset all configuration parameters to default Values
 * Restores factory settings and clears any custom configurations
 */
void resetToDefaults() {
  CPULED(0x00,0x00,0x80);

  strncpy(LedConfig.formatId, CONFIG_IDENTIFIER, sizeof(LedConfig.formatId) - 1);
  LedConfig.formatId[sizeof(LedConfig.formatId) - 1] = '\0';
  strcpy(LedConfig.identifier, IDENTIFIER_DEFAULT);
  LedConfig.numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
  LedConfig.numChannels = NUM_CHANNELS_DEFAULT;
  LedConfig.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
  LedConfig.spacerWidth = SPACER_WIDTH_DEFAULT;
  LedConfig.startOffset = START_OFFSET;
  LedConfig.blinkinterval = BLINK_INTERVAL;
  LedConfig.updateinterval = UPDATE_INTERVAL;
  LedConfig.brightness = STRIP_BRIGHTNESS;
  LedConfig.animateinterval = ANIMATE_INTERVAL;
  LedConfig.fadingAnimation = FADING;
  LedConfig.fading2StepIn = FADING_2STEP_IN;
  LedConfig.fading2StepOut = FADING_2STEP_OUT;
  LedConfig.startupAnimation = STARTUP_ANIMATION;
  LedConfig.localEcho = LOCAL_ECHO;
  LedConfig.channelOrder[0] = 1; LedConfig.channelOrder[1] = 2; LedConfig.channelOrder[2] = 3; LedConfig.channelOrder[3] = 4;
  LedConfig.channelOrder[4] = 5; LedConfig.channelOrder[5] = 6; LedConfig.channelOrder[6] = 7; LedConfig.channelOrder[7] = 8;
  LedConfig.state_pattern[0] = 0; // Fixed since this is black.
  LedConfig.state_pattern[1] = 0; // no blink
  LedConfig.state_pattern[2] = 0; // no blink
  LedConfig.state_pattern[3] = 0; // no blink
  LedConfig.state_pattern[4] = 0; // no blink
  LedConfig.state_pattern[5] = 1; // blink by default
  LedConfig.state_pattern[6] = 1; // blink by default
  LedConfig.state_pattern[7] = 1; // blink by default
  LedConfig.state_pattern[8] = 1; // blink by default
  LedConfig.state_pattern[9] = 10; // up/down
  LedConfig.state_color[0] = CRGB::Black;
  LedConfig.state_color[1] = COLOR_STATE_1;
  LedConfig.state_color[2] = COLOR_STATE_2;
  LedConfig.state_color[3] = COLOR_STATE_3;
  LedConfig.state_color[4] = COLOR_STATE_4;
  LedConfig.state_color[5] = COLOR_STATE_1;
  LedConfig.state_color[6] = COLOR_STATE_2;
  LedConfig.state_color[7] = COLOR_STATE_3;
  LedConfig.state_color[8] = COLOR_STATE_4;
  LedConfig.state_color[9] = CRGB::White;
  LedConfig.channelGPIOpin[0] = 2;
  LedConfig.channelGPIOpin[1] = 3;
  LedConfig.channelGPIOpin[2] = 4;
  LedConfig.channelGPIOpin[3] = 5;
  LedConfig.channelGPIOpin[4] = 6;
  LedConfig.channelGPIOpin[5] = 7;
  LedConfig.channelGPIOpin[6] = 8;
  LedConfig.channelGPIOpin[7] = 9;
}

/**
 * Display current configuration settings
 * Shows all parameter Values including LED counts, timing, and channel configuration
 */
void showConfiguration() {
  char output[MAX_OUTPUT_LEN];
  memset(output, '\0', sizeof(output));

  Serial.print("Identifier           : ");
  Serial.println(LedConfig.identifier);

  Serial.print("LEDs per channel     : ");
  Serial.println(LedConfig.numLedsPerChannel);

  Serial.print("Groups per channel   : ");
  Serial.println(LedConfig.numGroupsPerChannel);

  Serial.print("Amount of channels   : ");
  Serial.println(LedConfig.numChannels);

  Serial.print("Spacer width         : ");
  Serial.println(LedConfig.spacerWidth);

  Serial.print("Start Offset         : ");
  Serial.println(LedConfig.startOffset);

  Serial.print("Blinking interval    : ");
  Serial.println(LedConfig.blinkinterval);

  Serial.print("Update interval      : ");
  Serial.println(LedConfig.updateinterval);

  Serial.print("Animate interval     : ");
  Serial.println(LedConfig.animateinterval);

  Serial.print("Animation fading     : ");
  Serial.println(LedConfig.fadingAnimation);

  Serial.print("2-step fade (in/out) : ");
  Serial.print(LedConfig.fading2StepIn);
  Serial.print(" / ");
  Serial.println(LedConfig.fading2StepOut);

  Serial.print("Channel order        : ");
  for (int i = 0; i < NUM_CHANNELS_MAX; i++) {
    Serial.print(LedConfig.channelOrder[i]);
  }
  Serial.println();

  Serial.println("LED Mode             : RGB-only (W channel = 0)");

  Serial.print("Overall brightness   : ");
  Serial.println(LedConfig.brightness);

  Serial.print("Local echo           : ");
  Serial.println(LedConfig.localEcho ? "Enabled" : "Disabled");

  Serial.println();
  Serial.print("Channel              : | ");
  for (uint8_t channel=0; channel<NUM_CHANNELS_MAX; channel++) {
    sprintf(output, "%2d | ", channel+1);
    Serial.print(output);
  }
  Serial.println();
  Serial.print("GPIO-PIN             : | ");
  for (uint8_t channel=0; channel<NUM_CHANNELS_MAX; channel++) {
    sprintf(output, "%02d | ", LedConfig.channelGPIOpin[channel] );
    Serial.print(output);
  }
  Serial.println();

  Serial.println();
  Serial.println("Color state          : RRGGBB    Pattern:");
  Serial.println("            0        : 000000       0 (fixed)");
  for (int state=1; state<=9; state++) {
    sprintf(output, "            %d        : %02X%02X%02X      %2d",
      state,
      LedConfig.state_color[state].red,
      LedConfig.state_color[state].green,
      LedConfig.state_color[state].blue,
      LedConfig.state_pattern[state]
    );
    Serial.println(output);
  }

  Serial.println();
}

/**
 * Read Data from a file on LittleFS
 * @param path File path to read from
 * @return Pointer to allocated buffer containing file Data, or nullptr if failed
 */
char* readFile(const char * path, long *outSize) {
  CPULED(0x00,0x00,0x80);
  File fileH = LittleFS.open(F(path), "r");
  if (!fileH) {
    Serial.println("NOTE: Failed opening confgfile");
    return 0;
  }

  long fileSize=fileH.size();
  if (outSize) *outSize = fileSize;
  if (fileSize > 0) {
    char *Data = new char[fileSize+1];
    if (Data == nullptr) {
      Serial.println("NOTE: Memory allocation failed!");
      fileH.close();
      return 0;
    }
    fileH.readBytes(Data, fileSize);
    Data[fileSize] = '\0';

    fileH.close();
    return Data;
  } else {
      Serial.println("NOTE: File is empty!");
      return nullptr;  // Return nullptr if the file is empty
  }
}

/**
 * Write Data to a file on LittleFS
 * @param path File path to write to
 * @param Data Data buffer to write
 * @param DataSize Size of Data in bytes
 * @return true if successful, false otherwise
 */
bool writeFile(const char * path, const char * Data, size_t DataSize) {
  CPULED(0x00,0x00,0x80);
  File fileH = LittleFS.open(F(path), "w");
  if (!fileH) {
    Serial.println("* Opening Failed");
    return false;
  }
  size_t bytesWritten = fileH.write((const uint8_t*)Data, DataSize);
  if (bytesWritten != DataSize) {
    Serial.println("* Write Failed");
    fileH.close();
    return false;
  }
  fileH.close();
  return true;
}

/**
 * Validate and correct a configuration struct in place
 * Shared by file load, hex import, and runtime defaults so every path that can
 * produce a configuration applies the same semantic checks.
 * @param cfg Configuration to validate (corrected in place)
 * @return true if any value was out of range and corrected
 */
bool validateConfig(LedData &cfg) {
  bool needsCorrection = false;

  // Detect a persisted/imported struct from an incompatible firmware build.
  // Raw-struct persistence is layout-sensitive; every field below is still
  // clamped defensively, but flag the mismatch so it's visible rather than
  // silently accepted.
  if (strncmp(cfg.formatId, CONFIG_IDENTIFIER, sizeof(cfg.formatId)) != 0) {
    Serial.println("WARNING: Configuration format identifier mismatch (saved with a different firmware build); values are being validated defensively.");
    strncpy(cfg.formatId, CONFIG_IDENTIFIER, sizeof(cfg.formatId) - 1);
    cfg.formatId[sizeof(cfg.formatId) - 1] = '\0';
    needsCorrection = true;
  }

  // Identifier must always be NUL-terminated
  if (cfg.identifier[IDENTIFIER_MAX_LENGTH - 1] != '\0') {
    cfg.identifier[IDENTIFIER_MAX_LENGTH - 1] = '\0';
    needsCorrection = true;
  }

  if (cfg.numLedsPerChannel < NUM_LEDS_PER_CHANNEL_MIN || cfg.numLedsPerChannel > NUM_LEDS_PER_CHANNEL_MAX) {
    cfg.numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
    needsCorrection = true;
  }

  if (cfg.numChannels < 1 || cfg.numChannels > NUM_CHANNELS_MAX) {
    cfg.numChannels = NUM_CHANNELS_DEFAULT;
    needsCorrection = true;
  }

  if (cfg.numGroupsPerChannel < 1 || cfg.numGroupsPerChannel > NUM_GROUPS_PER_CHANNEL_MAX) {
    cfg.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
    needsCorrection = true;
  }

  // Critical: Ensure total groups doesn't exceed MAX_GROUPS (cast to avoid uint8_t overflow)
  if ((uint16_t)cfg.numChannels * (uint16_t)cfg.numGroupsPerChannel > MAX_GROUPS) {
    Serial.println("WARNING: numChannels * numGroupsPerChannel exceeds MAX_GROUPS!");
    cfg.numChannels = NUM_CHANNELS_DEFAULT;
    cfg.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
    needsCorrection = true;
  }

  if (cfg.spacerWidth > SPACER_WIDTH_MAX) {
    cfg.spacerWidth = SPACER_WIDTH_DEFAULT;
    needsCorrection = true;
  }

  if (cfg.startOffset > START_OFFSET_MAX) {
    cfg.startOffset = START_OFFSET;
    needsCorrection = true;
  }

  // Combined geometry: a complete group must fit inside the configured strip
  // with positive width; otherwise rendering divides by zero or writes
  // out of bounds. Reset the geometry fields as a set when inconsistent.
  {
    const uint16_t segmentWidth = cfg.numLedsPerChannel / cfg.numGroupsPerChannel;
    const uint32_t lastGroupEnd = (uint32_t)cfg.startOffset +
                                  (uint32_t)(cfg.numGroupsPerChannel - 1) * segmentWidth +
                                  (uint32_t)(segmentWidth - cfg.spacerWidth);
    if (segmentWidth == 0 || cfg.spacerWidth >= segmentWidth || lastGroupEnd > cfg.numLedsPerChannel) {
      cfg.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
      cfg.spacerWidth = SPACER_WIDTH_DEFAULT;
      cfg.startOffset = START_OFFSET;
      needsCorrection = true;
    }
  }

  if (cfg.blinkinterval < BLINK_INTERVAL_MIN || cfg.blinkinterval > BLINK_INTERVAL_MAX) {
    cfg.blinkinterval = BLINK_INTERVAL;
    needsCorrection = true;
  }

  if (cfg.animateinterval < ANIMATE_INTERVAL_MIN || cfg.animateinterval > ANIMATE_INTERVAL_MAX) {
    cfg.animateinterval = ANIMATE_INTERVAL;
    needsCorrection = true;
  }

  if (cfg.updateinterval < UPDATE_INTERVAL_MIN || cfg.updateinterval > UPDATE_INTERVAL_MAX) {
    cfg.updateinterval = UPDATE_INTERVAL;
    needsCorrection = true;
  }

  // Blinking must stay slower than the refresh interval, same rule as the
  // runtime Cb/Cu commands.
  if (cfg.blinkinterval <= cfg.updateinterval) {
    cfg.blinkinterval = BLINK_INTERVAL;
    cfg.updateinterval = UPDATE_INTERVAL;
    needsCorrection = true;
  }

  if (cfg.brightness < STRIP_BRIGHTNESS_MIN || cfg.brightness > STRIP_BRIGHTNESS_MAX) {
    cfg.brightness = STRIP_BRIGHTNESS;
    needsCorrection = true;
  }

  if (cfg.fadingAnimation > 255 || cfg.fading2StepIn > 255 || cfg.fading2StepOut > 255) {
    cfg.fadingAnimation = FADING;
    cfg.fading2StepIn = FADING_2STEP_IN;
    cfg.fading2StepOut = FADING_2STEP_OUT;
    needsCorrection = true;
  }

  // Patterns index animate_Step[]; anything above PATTERN_MAX is undefined
  for (uint8_t i = 0; i <= MAX_STATE; i++) {
    if (cfg.state_pattern[i] > PATTERN_MAX) {
      cfg.state_pattern[i] = 0;
      needsCorrection = true;
    }
  }

  // Channel order must be a permutation of 1..NUM_CHANNELS_MAX; a 0 or a
  // duplicate would index leds[] out of bounds
  {
    bool seen[NUM_CHANNELS_MAX + 1] = {false};
    bool orderValid = true;
    for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
      uint8_t ch = cfg.channelOrder[i];
      if (ch < 1 || ch > NUM_CHANNELS_MAX || seen[ch]) {
        orderValid = false;
        break;
      }
      seen[ch] = true;
    }
    if (!orderValid) {
      for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
        cfg.channelOrder[i] = i + 1;
      }
      needsCorrection = true;
    }
  }

  // Validate GPIO pins are within range, not the CPU LED, and unique
  for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
    if (cfg.channelGPIOpin[i] < GPIO_PIN_MIN || cfg.channelGPIOpin[i] > GPIO_PIN_MAX ||
        cfg.channelGPIOpin[i] == CPULED_GPIO) {
      cfg.channelGPIOpin[i] = i + GPIO_PIN_MIN; // Reset to default sequential pins
      needsCorrection = true;
    }
    // Check for duplicate GPIO pins
    for (uint8_t j = i + 1; j < NUM_CHANNELS_MAX; j++) {
      if (cfg.channelGPIOpin[i] == cfg.channelGPIOpin[j]) {
        Serial.print("WARNING: Duplicate GPIO pin ");
        Serial.print(cfg.channelGPIOpin[j]);
        Serial.println(" detected, resetting to defaults");
        // Reset all GPIO pins to defaults
        for (uint8_t k = 0; k < NUM_CHANNELS_MAX; k++) {
          cfg.channelGPIOpin[k] = k + GPIO_PIN_MIN;
        }
        needsCorrection = true;
        break;
      }
    }
  }

  // Normalize bools in case a corrupt image stored a non-0/1 byte
  cfg.startupAnimation = cfg.startupAnimation ? true : false;
  cfg.localEcho = cfg.localEcho ? true : false;

  return needsCorrection;
}

// Field-wise wire format for LedData (CONFIG_WIRE_SIZE is defined near the
// LedData struct above), used by both flash persistence
// (saveConfiguration()/loadConfiguration()) and the Se/Li:CONFIG: hex
// export/import. Each field is written in a fixed order with an explicit
// width, instead of memcpy-ing the raw struct, so the format does not
// depend on this compiler's struct layout/padding/CRGB representation and
// is stable across firmware builds. Update CONFIG_WIRE_SIZE (and its
// tripwire) if a field is ever added, removed, or resized here.

static inline void wireWriteU8(uint8_t *buf, size_t &pos, uint8_t v) {
  buf[pos++] = v;
}
static inline void wireWriteU16(uint8_t *buf, size_t &pos, uint16_t v) {
  buf[pos++] = (uint8_t)(v & 0xFF);
  buf[pos++] = (uint8_t)((v >> 8) & 0xFF);
}
static inline void wireWriteBytes(uint8_t *buf, size_t &pos, const void *src, size_t n) {
  memcpy(buf + pos, src, n);
  pos += n;
}
static inline uint8_t wireReadU8(const uint8_t *buf, size_t &pos) {
  return buf[pos++];
}
static inline uint16_t wireReadU16(const uint8_t *buf, size_t &pos) {
  uint16_t v = (uint16_t)buf[pos] | ((uint16_t)buf[pos + 1] << 8);
  pos += 2;
  return v;
}
static inline void wireReadBytes(const uint8_t *buf, size_t &pos, void *dst, size_t n) {
  memcpy(dst, buf + pos, n);
  pos += n;
}

/**
 * Encode a LedData struct into the fixed-width field-wise wire format.
 * @param cfg Configuration to encode
 * @param buf Output buffer, must be at least CONFIG_WIRE_SIZE bytes
 * @return Number of bytes written (always CONFIG_WIRE_SIZE)
 */
size_t encodeConfig(const LedData &cfg, uint8_t *buf) {
  size_t pos = 0;
  wireWriteBytes(buf, pos, cfg.formatId, sizeof(cfg.formatId));
  wireWriteBytes(buf, pos, cfg.identifier, sizeof(cfg.identifier));
  wireWriteU16(buf, pos, cfg.numLedsPerChannel);
  wireWriteU8(buf, pos, cfg.numChannels);
  wireWriteU8(buf, pos, cfg.numGroupsPerChannel);
  wireWriteU8(buf, pos, cfg.spacerWidth);
  wireWriteU8(buf, pos, cfg.startOffset);
  wireWriteU16(buf, pos, cfg.blinkinterval);
  wireWriteU16(buf, pos, cfg.animateinterval);
  wireWriteU16(buf, pos, cfg.updateinterval);
  wireWriteU16(buf, pos, cfg.brightness);
  wireWriteU16(buf, pos, cfg.fadingAnimation);
  wireWriteU16(buf, pos, cfg.fading2StepIn);
  wireWriteU16(buf, pos, cfg.fading2StepOut);
  wireWriteU8(buf, pos, cfg.startupAnimation ? 1 : 0);
  wireWriteU8(buf, pos, cfg.localEcho ? 1 : 0);
  for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) wireWriteU8(buf, pos, cfg.channelGPIOpin[i]);
  for (uint8_t i = 0; i < 12; i++) wireWriteU8(buf, pos, cfg.state_pattern[i]);
  for (uint8_t i = 0; i < 10; i++) {
    wireWriteU8(buf, pos, cfg.state_color[i].r);
    wireWriteU8(buf, pos, cfg.state_color[i].g);
    wireWriteU8(buf, pos, cfg.state_color[i].b);
  }
  for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) wireWriteU8(buf, pos, cfg.channelOrder[i]);
  return pos;  // always == CONFIG_WIRE_SIZE
}

/**
 * Decode the fixed-width field-wise wire format into a LedData struct.
 * Does not validate ranges -- call validateConfig() on the result before
 * trusting/using it, same as every other path that produces a LedData.
 * @param buf Input buffer
 * @param len Length of buf in bytes; must equal CONFIG_WIRE_SIZE
 * @param cfg Output configuration
 * @return true if len matched and decode succeeded, false otherwise
 */
bool decodeConfig(const uint8_t *buf, size_t len, LedData &cfg) {
  if (len != CONFIG_WIRE_SIZE) return false;
  size_t pos = 0;
  wireReadBytes(buf, pos, cfg.formatId, sizeof(cfg.formatId));
  wireReadBytes(buf, pos, cfg.identifier, sizeof(cfg.identifier));
  cfg.numLedsPerChannel = wireReadU16(buf, pos);
  cfg.numChannels = wireReadU8(buf, pos);
  cfg.numGroupsPerChannel = wireReadU8(buf, pos);
  cfg.spacerWidth = wireReadU8(buf, pos);
  cfg.startOffset = wireReadU8(buf, pos);
  cfg.blinkinterval = wireReadU16(buf, pos);
  cfg.animateinterval = wireReadU16(buf, pos);
  cfg.updateinterval = wireReadU16(buf, pos);
  cfg.brightness = wireReadU16(buf, pos);
  cfg.fadingAnimation = wireReadU16(buf, pos);
  cfg.fading2StepIn = wireReadU16(buf, pos);
  cfg.fading2StepOut = wireReadU16(buf, pos);
  cfg.startupAnimation = wireReadU8(buf, pos) != 0;
  cfg.localEcho = wireReadU8(buf, pos) != 0;
  for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) cfg.channelGPIOpin[i] = wireReadU8(buf, pos);
  for (uint8_t i = 0; i < 12; i++) cfg.state_pattern[i] = wireReadU8(buf, pos);
  for (uint8_t i = 0; i < 10; i++) {
    cfg.state_color[i].r = wireReadU8(buf, pos);
    cfg.state_color[i].g = wireReadU8(buf, pos);
    cfg.state_color[i].b = wireReadU8(buf, pos);
  }
  for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) cfg.channelOrder[i] = wireReadU8(buf, pos);
  return pos == len;
}

/**
 * Load configuration from LittleFS file
 * Reads saved configuration and applies it to LedConfig struct
 * @return true if successful, false if file not found or invalid
 */
bool loadConfiguration() {
  CPULED(0x00,0x00,0x80);

  long fileSize = 0;
  char *buffer = readFile(CONFIG_FILENAME, &fileSize);
  if (buffer != 0) {

    // Calculate sizes to separate struct and checksum in buffer
    const int structSize = CONFIG_WIRE_SIZE;
    const int totalSize = structSize + 32;  // 32 for the saved binary checksum

    // Require an exact-size file before touching its contents
    if (fileSize != totalSize) {
      Serial.print("Configuration size mismatch (expected ");
      Serial.print(totalSize);
      Serial.print(" bytes, got ");
      Serial.print(fileSize);
      Serial.println("), using defaults.");
      delete[] buffer;
      return false;
    }

    // Separate the Data and the checksum
    uint8_t loadedChecksum[32];
    memcpy(loadedChecksum, buffer + structSize, 32);  // Extract the checksum from the file

   // Calculate checksum from the loaded LedData
    SHA256 sha256;
    sha256.reset();
    sha256.update((const uint8_t*)buffer, structSize);  // Recalculate the checksum for LedData
    uint8_t calculatedChecksum[32];
    sha256.finalize(calculatedChecksum, sizeof(calculatedChecksum));

    // Compare the loaded checksum with the recalculated checksum
    if (memcmp(loadedChecksum, calculatedChecksum, 32) == 0) {
      // Checksums match: decode the field-wise wire format into a
      // temporary struct and validate before it replaces the live
      // configuration
      LedData temp;
      bool decodeOk = decodeConfig((const uint8_t*)buffer, structSize, temp);
      delete[] buffer;  // Free memory

      if (!decodeOk) {
        Serial.println("Configuration decode failed (corrupt wire format).");
        Serial.println();
        return false;
      }

      bool needsCorrection = validateConfig(temp);
      LedConfig = temp;

      Serial.println("Checksum matches, configuration loaded.");
      if (needsCorrection) {
        Serial.println("WARNING: Some Values were out of range and corrected to defaults.");
        Serial.println("Use 'S' to save corrected configuration.");
        Serial.println();
      } else {
        Serial.println();
      }
      return true;
    } else {
      Serial.println("Checksum mismatch, configuration is corrupted!");
      Serial.println();
      delete[] buffer;  // Free memory
      return false;
    }
  } else {
    // No Data, set defaults
    delete[] buffer;   // Free memory
    return false;
  }
}

/**
 * Save current configuration to LittleFS file
 * Writes LedConfig struct to flash memory for persistence. The new data is
 * written to a temporary file and then renamed over the real config file, so
 * a reset/power loss mid-write leaves the previous known-good file intact
 * instead of a truncated/corrupt one.
 * @return true if successful, false otherwise
 */
bool saveConfiguration() {
  CPULED(0x00,0x00,0x80);
  // Serialize LedData into the field-wise wire format so we can save it
  char buffer[CONFIG_WIRE_SIZE];
  SHA256 sha256;
  encodeConfig(LedConfig, (uint8_t*)buffer);

  // Calculate SHA-256 checksum
  sha256.reset();
  sha256.update((const uint8_t*)buffer, sizeof(buffer));
  uint8_t hash[32];
  sha256.finalize(hash, sizeof(hash));

  // Create a final buffer containing the serialized Data + checksum (in binary)
  char finalBuffer[sizeof(buffer) + 32];           // 32 bytes for the checksum
  memcpy(finalBuffer, buffer, sizeof(buffer));     // Copy serialized LedData
  memcpy(finalBuffer + sizeof(buffer), hash, 32);  // Copy binary checksum after the Data (append)

  // Write to a temp file first...
  if (!writeFile(CONFIG_FILENAME_TMP, finalBuffer, sizeof(finalBuffer))) {
    LittleFS.remove(CONFIG_FILENAME_TMP);
    return false;
  }

  // ...then atomically replace the real config file. This is the only step
  // that can be interrupted without losing the previous good configuration:
  // either the rename completes and the new file is live, or it doesn't and
  // the old config.bin (if any) is still there untouched.
  if (!LittleFS.rename(CONFIG_FILENAME_TMP, CONFIG_FILENAME)) {
    Serial.println("* Rename to final config file failed");
    LittleFS.remove(CONFIG_FILENAME_TMP);
    return false;
  }

  return true;
}

/**
 * Apply fading effect to all LEDs across all channels
 * @param StartLed Starting LED number (default: 1)
 * @param EndLed Ending LED number (default: numLedsPerChannel)
 * @param fadeFactor Fading multiplier (default: 1.4)
 */
void FadeAll(int StartLed, int EndLed, float fadeFactor) {
  CPULED(0x00,0x00,0x80);
  for(int i = StartLed; i < EndLed; i++)
    for(int n = 0; n < LedConfig.numChannels; n++) {
      // leds[n][i].nscale8(200)
      leds[n][i].r=leds[n][i].r/fadeFactor;
      leds[n][i].g=leds[n][i].g/fadeFactor;
      leds[n][i].b=leds[n][i].b/fadeFactor;
      ZERO_W(leds[n][i]);
    }
}

/**
 * Execute startup animation sequence
 * Displays animated pattern on all channels to show system is ready
 */
void StartupLoop() {
  // Boot animation
  CPULED(0x00,0x00,0x80);

  // Green closing [->><<-]
  uint8_t DELAY = (LedConfig.numLedsPerChannel / 10);
  CRGB color = 0x00FF00;
  for(int i = 0; i < LedConfig.numLedsPerChannel/2; i+=1) {
    for(int n = 0; n < LedConfig.numChannels; n++) {
      leds[n][i] = CRGB::Green;
      leds[n][LedConfig.numLedsPerChannel -i -1] = CRGB::Green;
      ZERO_W(leds[n][i]);
      ZERO_W(leds[n][LedConfig.numLedsPerChannel -i -1]);
    }
    FastLED.show();
    FadeAll(0,LedConfig.numLedsPerChannel,1.4);
    watchdog_update();  // Long animations can exceed the watchdog timeout
    delay(DELAY);
  }

  //  Flash [######]
  setAllLEDs(CRGB::Green);
  FastLED.show();
  delay(75);
  watchdog_update();
  for(int i = 0; i < 12; i++) {
    FadeAll(0,LedConfig.numLedsPerChannel,1.5);
    FastLED.show();
    watchdog_update();
    delay(65);
  }

  // All Off
  FastLED.clear(true);
}

//
// BitBang code for CPU led (can't use FastLED as all 8 PIO chanels are used for the ledstrips)
//

// WS2812B CPULED bit timing targets: T0H 400ns, T0L 850ns, T1H 800ns, T1L 450ns.
//
// delay_cycles() previously used a plain C while-loop, on the assumption that
// each iteration cost ~4 CPU cycles. Measured via objdump against the actual
// compiled binary, the loop the compiler generated actually cost 7 cycles per
// iteration (extra compare/branch overhead), making every delay ~75% longer
// than intended -- enough to push both '0' and '1' bit pulses out of WS2812
// spec and make the LED latch onto garbage (observed as solid bright white
// instead of flashing/glowing).
//
// delay_cycles() is now hand-written in raw assembly using the "subs; bne"
// idiom: a well-documented, exact 3-cycles-per-iteration loop on Cortex-M0+
// (1 cycle for SUBS, 2 cycles for a taken BNE, 1 on the final not-taken
// pass), independent of compiler version/optimization level.
//
// The surrounding per-bit overhead in sendByte_CPULED() (the bit test/
// branch, register bookkeeping, the bl/bx call overhead, and the GPIO
// stores) was then measured by objdumping the actual compiled binary and
// costing each instruction against the ARM Cortex-M0+ Technical Reference
// Manual's instruction timing table (1 cycle for MOVS/SUBS/TST/ASRS/LSLS;
// 1 cycle for STR to the RP2040 SIO single-cycle IO block used by
// gpio_put(); 2 cycles for a stack LDR; 1/2 cycles for a not-taken/taken
// conditional branch; 2 for an unconditional B; 3 for BL). This gave four
// *different* per-segment overheads (the T1H/T0H high-side overhead is 10
// cycles; T1L's low-side overhead is 15; T0L's is 17, because its path
// includes an extra jump into shared code) -- a single shared estimate,
// as used previously, cannot be exact for all four segments simultaneously.
//
// These overhead constants are tied to the *exact* instruction sequence
// generated for sendByte_CPULED() by this compiler/optimization level; if
// the toolchain or this function's code changes, re-derive them the same
// way (objdump the .elf, cost each instruction from the table above) rather
// than assuming they still apply. This remains a software-timed path (not
// a hardware PIO state machine); prefer a PIO-based driver if it ever needs
// to be bulletproof against compiler changes. See AGENTS.md.
constexpr float CPULED_NS_PER_CYCLE = 1000000000.0f / (float)F_CPU;
constexpr uint32_t cpuledIterations(float ns, uint32_t overheadCycles) {
  uint32_t targetCycles = (uint32_t)(ns / CPULED_NS_PER_CYCLE + 0.5f);
  uint32_t remaining = (targetCycles > overheadCycles) ? (targetCycles - overheadCycles) : 3;
  uint32_t iterations = (remaining + 1) / 3;  // delay_cycles() loop: 3 cycles/iteration, round to nearest
  return (iterations > 0) ? iterations : 1;
}
// These MUST be true compile-time constants (not just calls to a constexpr
// function from a non-constant context), otherwise the arithmetic inside
// cpuledIterations() could run at every single bit send instead of being
// folded away. A constexpr variable initializer is guaranteed to be
// evaluated at compile time (it's a compile error otherwise), so use
// variables here, not macros expanding to a plain function call.
constexpr uint32_t T1H = cpuledIterations(800.0f, 10);  // 800ns high time for a '1' bit
constexpr uint32_t T1L = cpuledIterations(450.0f, 15);  // 450ns low time for a '1' bit
constexpr uint32_t T0H = cpuledIterations(400.0f, 10);  // 400ns high time for a '0' bit
constexpr uint32_t T0L = cpuledIterations(850.0f, 17);  // 850ns low time for a '0' bit
#define RESET_TIME 60  // >50us reset time. v1.11 briefly raised this to 300
                        // alongside a "skip if unchanged" send optimization;
                        // both were reverted after real hardware showed wrong
                        // CPU LED colors (see updateSystemStatusLED()). Back
                        // to the known-good v1.10 value.

/**
 * Busy-wait for (approximately) iterations * 3 CPU cycles.
 * @param iterations Number of loop iterations to run (see T1H/T1L/T0H/T0L)
 */
__attribute__((noinline)) void delay_cycles(uint32_t iterations) {
  if (iterations == 0) return;
  __asm volatile (
    ".syntax unified \n"     // required for GAS to accept the 3-operand SUBS form below
    "1: \n"
    "   subs %0, %0, #1 \n"  // 1 cycle
    "   bne 1b \n"           // 2 cycles taken, 1 cycle on the final (not-taken) pass
    : "+l" (iterations)      // must be a low register (r0-r7) for Thumb16 SUBS
    :
    : "cc"
  );
}

/**
 * Send a single byte to CPU LED via bit-banging
 * Caller is responsible for the surrounding critical section; this function
 * does not touch interrupts so that a full RGB triplet can be sent atomically.
 * @param byte Byte Value to send to CPU LED
 */
void sendByte_CPULED(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    if (byte & (1 << i)) {
      gpio_put(CPULED_GPIO, 1);
      delay_cycles(T1H);
      gpio_put(CPULED_GPIO, 0);
      delay_cycles(T1L);
    } else {
      gpio_put(CPULED_GPIO, 1);
      delay_cycles(T0H);
      gpio_put(CPULED_GPIO, 0);
      delay_cycles(T0L);
    }
  }
}

/**
 * Send one RGB triplet to the CPU LED in a single critical section
 * Disables interrupts for the whole R/G/B sequence (rather than per byte) so
 * a nested or nearly-simultaneous call cannot interleave colors or leave
 * interrupts incorrectly enabled/disabled. The previous interrupt state is
 * saved and restored instead of unconditionally re-enabling interrupts.
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void sendRGB_CPULED(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t interruptStatus = save_and_disable_interrupts();
#if CPULED_COLOR_ORDER == CPULED_COLOR_ORDER_GRB
  sendByte_CPULED(g);
  sendByte_CPULED(r);
  sendByte_CPULED(b);
#else
  sendByte_CPULED(r);
  sendByte_CPULED(g);
  sendByte_CPULED(b);
#endif
  restore_interrupts(interruptStatus);
  busy_wait_us(RESET_TIME); // Reset time after sending color
}

/**
 * Set CPU LED color using 32-bit color Value
 * @param color 32-bit RGB color Value
 */
void CPULED(uint32_t color) {
    sendRGB_CPULED((color & 0x00FF0000) >> 16, (color & 0x0000FF00) >> 8, (color & 0x000000FF));
}

/**
 * Set CPU LED color using CRGB color object
 * @param color CRGB color object
 */
void CPULED(CRGB color) {
    sendRGB_CPULED(color.r, color.g, color.b);
}

/**
 * Set CPU LED color using individual RGB components
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void CPULED(uint8_t r, uint8_t g, uint8_t b) {
    sendRGB_CPULED(r, g, b);
}
