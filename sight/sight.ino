/*
Name        : SIGHT (Shelf Indicators for Guided Handling Tasks)
Version     : 1.9
Date        : 2025-12-02
Author      : Bas van Ritbergen <bas.vanritbergen@adyen.com> / bas@ritbit.com
Description : LED strip controller with animations, RGBW support, and comprehensive safety features.

              v1.9 improvements:
              - Renamed shelf/strip/output to channel to make it more generic
              - Added support for 2-step fading apart from the regular fading
              - Renamed Fading to FadingAnimation to distinguish between the two
              - Fixed all codeStyle issues, made it more readable and consistent
              - Added more comments and documentation
              - Added an proper interactive line editor with history and cursor control.
              - Added CPU status LED states (startup blue, normal green, error red) with brightness control
              - Implemented percent-based two-step fade-in/fade-out with configurable Cf:<anim>:<in>:<out>
              - Improved LED group handling for partial fills and ensured flashing works with percentages
              - Added Console wrapper class for Unix-style LF-only line endings (replaces Serial)

              v1.8 improvements:
              - Added comprehensive config validation on load and runtime
              - Added more error checking and validation
              - Added more statistics tracking (uptime, Command count, error count)
              - Added more safety features (watchdog timer, brightness safety limits, buffer overflow protection)
              - Added Command echo, version info, help, and system info Commands
              - Added config backup/restore via hex export/import (Se/Li Commands)
              - Added quick status summary (Q Command) and improved group state display (G Command)
              - Added brightness safety limits and warnings
              - Added buffer overflow protection and input validation
              - Improved error messages with actual Values shown
              - Added function documentation and const correctness
              - Fixed group LED clearing to prevent color overlap

              v1.7 improvements:

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


// Maximum length for system identifier and system default name
#define IDENTIFIER_MAX_LENGTH 16
#define IDENTIFIER_DEFAULT "SIGHT v1.9"

// Configuration identifier for validation and versioning
#define CONFIG_IDENTIFIER "SIGHT-CFG1.9"

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

// System behavior configuration
// Enable startup animation on boot
#define STARTUP_ANIMATION true

// Input Data from serial is stored in an array for further processing and editing.
#define HISTORY_SIZE 20

// Enable/disable command echo for debugging
#define COMMAND_ECHO false

// System status LED configuration (dimmed brightness at 64 for visibility)
#define CPULED_STATUS_BRIGHTNESS 32  // Dimmed brightness for status indicators
#define CPULED_NORMAL_INTERVAL 2000  // Normal operation: slow blue glow
#define CPULED_ERROR_INTERVAL 500    // Error state: fast red blink
#define CPULED_STARTUP_INTERVAL 1000  // Startup: medium green pulse

// LED brightness configuration (0-255 range, with safety limits)
#define BRIGHTNESS 255
#define BRIGHTNESS_MIN 10
#define BRIGHTNESS_MAX 255
#define BRIGHTNESS_WARNING_THRESHOLD 200

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
// #define USE_RGB_LEDS

// For WS2813B-RGBW, SK6812 RGBW strips (4 bytes per LED)
#define USE_RGBW_LEDS


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
  // LED color byte order
  #define LED_COLOR_ORDER RGB
  // Compilation message for RGBW LEDs
  #pragma message "Compiling for RGBW LEDs (SK6812, 4 bytes/LED, RGB order)"
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

// GPIO pin for onboard status LED
#define CPULED_GPIO 16

// Configuration file path in LittleFS
#define CONFIG_FILENAME "/config.bin"

// ###########################################################################
// No configurable items below

// Current firmware version
#define VERSION "1.9"

// Buffer size configuration (input/output string limits)
#define MAX_INPUT_LEN 512
#define MAX_OUTPUT_LEN 2560

// We definitely need these libraries
#include <Arduino.h>            // Core Arduino library e.g. for GPIO pins
#include <cstring>              // String functions
#include <cstdlib>              // Memory functions
#include <Ticker.h>             // Ticker library for timed events (animations/flashing)
#include <Crypto.h>             // Crypto library for SHA256
#include <SHA256.h>             // SHA256 library for creating configfile checksum
#include "LittleFS.h"           // FileSystem library for storing config
#include <FastLED.h>            // Core LED control library (RGB/RGBW)
#include "FastLED_RGBW.h"       // Add RGBW support for FastLED  
#include "hardware/watchdog.h"  // Core watchdog timer
#include <MicrocontrollerID.h>  // Figure MCU type/serial
char mcuId[41];

// Console class: Serial wrapper with Unix-style LF-only line endings
class ConsoleClass : public Print {
public:
  void begin(unsigned long baud) { Serial.begin(baud); }
  int available() { return Serial.available(); }
  int read() { return Serial.read(); }
  int peek() { return Serial.peek(); }
  void flush() { Serial.flush(); }
  size_t write(uint8_t c) override { return Serial.write(c); }
  size_t write(const uint8_t *buffer, size_t size) override { return Serial.write(buffer, size); }
  
  // Override println to use LF only (no CR)
  size_t println() { return write('\n'); }
  template<typename T> size_t println(T value) { size_t n = print(value); n += write('\n'); return n; }
  
  // Explicit operator bool for while(!Console) usage
  explicit operator bool() { return Serial; }
} Console;

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

// Config Data is conviently stored in a struct (to easy store and retrieve from EEPROM/Flash)
// Set defaults, they will be overwritten by load from EEPROM
struct LedData {
  char   identifier[IDENTIFIER_MAX_LENGTH] = IDENTIFIER_DEFAULT;
  uint16_t               numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
  uint8_t                      numChannels = NUM_CHANNELS_DEFAULT;
  uint8_t              numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
  uint8_t                      spacerWidth = SPACER_WIDTH_DEFAULT;
  uint8_t                      startOffset = START_OFFSET;
  uint16_t                   blinkinterval = BLINK_INTERVAL;
  uint16_t                 animateinterval = ANIMATE_INTERVAL;
  uint16_t                  updateinterval = UPDATE_INTERVAL;
  uint16_t                      brightness = BRIGHTNESS;
  uint16_t                 fadingAnimation = FADING;
  uint16_t                   fading2StepIn = FADING_2STEP_IN;
  uint16_t                  fading2StepOut = FADING_2STEP_OUT;
  bool                    startupAnimation = STARTUP_ANIMATION;
  bool                         CommandEcho = COMMAND_ECHO;
  uint8_t channelGPIOpin[NUM_CHANNELS_MAX] = {2,3,4,5,6,7,8,9};
  uint8_t                state_pattern[12] = {0,0,0,0,0,1,1,1,1,0};
  CRGB                     state_color[10] = {CRGB::Black, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, CRGB::White};
  uint8_t   channelOrder[NUM_CHANNELS_MAX] = {1,2,3,4,5,6,7,8};
} LedConfig = {};

char inputBuffer[MAX_INPUT_LEN + 1]; // +1 for null terminator
uint16_t inputLength = 0;
uint16_t cursorPosition = 0;
bool insertMode = true;

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
char escapeDigits[4];
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

// for LEDstrip update frequency
int      updateinterval = 100;
uint32_t lastLedUpdate=0;

// for status update
uint32_t laststateUpdate;

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
uint8_t animate_Step[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// ##########################################################################################################

/**
 * Initialize microcontroller and setup system
 * Configures GPIO pins, serial communication, LED strips, timers, and loads configuration
 */
void setup() {

  // Setup USB-serial port
  Console.begin(115200);

  // Initialize status led, set to blue to show we are waiting for input
  // We have to disable theCPU led as Fastled can only drive 8 led channels ! (due to 8 PIO registers)
  // So we have to bitbang if we want to use it...
  pinMode(CPULED_GPIO, OUTPUT);
  gpio_put(CPULED_GPIO, 0);
  CPULED(0x00,0x00,0x00);

#ifdef POWERON_GPIOTEST && POWERON_GPIOTEST == true 
  // Enable GPIO 2-9 for pin test.
  for (int PIN=0; PIN<NUM_CHANNELS_MAX; PIN++) {
    pinMode(PIN+GPIO_PIN_MIN, OUTPUT);
  }

  setSystemState(SYSTEM_STARTUP);
  while (!Console) {
    // wait for serial port to connect.
    // Run a slow GPIO pintest while waiting
    // Status LED pulses blue via SYSTEM_STARTUP state

    updateSystemStatusLED();
    for (int PIN=0; PIN<NUM_CHANNELS_MAX; PIN++) {
      digitalWrite(PIN+GPIO_PIN_MIN, HIGH);
      delay(100);
      digitalWrite(PIN+GPIO_PIN_MIN, LOW);
      updateSystemStatusLED();
    }
  }
#endif

  // Enable watchdog timer (8 seconds timeout)
  // System will auto-reboot if watchdog is not fed within this time
  watchdog_enable(8000, 1);

  // Set status led to blue to show we are busy
  setSystemState(SYSTEM_NORMAL);

  //   Reset all to low
  for (int PIN=2; PIN<=9; PIN++) {
    digitalWrite(PIN, LOW);
  }

  updateSystemStatusLED();
  // Show welcome to let us know the controller is booting.
  // Also show some details about the MCU and the codeversion

  // Show version
  // Console.print("\x1b[2J\x1b[H"); // Clear screen, cursor home
  Console.println();
  Console.print("-=[ Shelf Indicators for Guided Handling Tasks ]=-\n" );
  Console.println();
  Console.print("SIGHT Version  : " );
  Console.println(VERSION);  // Why does this add a 0 to the string ??

  // Check if system recovered from watchdog reset
  if (watchdog_caused_reboot()) {
    Console.println("*** WARNING: System recovered from watchdog timeout ***");
  }

  // Boardname
  Console.print("MicroController : " );
  Console.println(BOARD_NAME);

  // CPU id
  // Note: often the MCU hangs/crashes on this... why ?
  Console.print("MCU-Serial      : " );
  MicroID.getUniqueIDString(mcuId, 8);
  Console.println(mcuId);

  // Blank line
  Console.println();
  Console.println("Initializing..." );
  Console.println();

  // Initialize LittleFS if available
  if (!LittleFS.begin()){
    Console.println("LittleFS mount failed!");

    // Attempt to format the filesystem
    Console.println("Formatting LittleFS...");
    if (LittleFS.format()) {
      Console.println("LittleFS formatting successful!");

      // Try to mount again after formatting
      if (LittleFS.begin()) {
        Console.println("LittleFS mounted successfully after formatting.");
      } else {
        Console.println("WARNING !!!\nLittleFS mount failed after formatting --> Load/Saving configuration not possible...\n" );
        // delay(2000);
        // rebootMCU();
      }
    } else {
        Console.println("WARNING !!!\nLittleFS formatting failed --> Load/Saving configuration not possible...\n" );
      // delay(2000);
      // rebootMCU();
    }
  } else {
    Console.println("LittleFS mounted successfully.");
    // Load or set defaults
    if (loadConfiguration() == false) {
      Console.println("Setting default configuration");
      resetToDefaults();
      // Set error state due to configuration failure
      setSystemState(SYSTEM_ERROR);
      Console.println("Status LED: Red blink (config error)");
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

  Console.println("Initialization done..,");

  // Set system state to normal operation
  setSystemState(SYSTEM_NORMAL);
  Console.println("System ready - Status LED: Blue glow");

  // Run startup animation if enabled
  if (LedConfig.startupAnimation) {
    StartupLoop();
  } else {
    // Clear LEDs if no startup animation
    FastLED.clear();
    FastLED.show();
  }

 	FastLED.setBrightness(LedConfig.brightness);

  // start timers for updating leds (x1000 so we set mSec)
  update_Timer.attach_ms(LedConfig.updateinterval, &writeChannelData);
  blink_Timer.attach_ms(LedConfig.blinkinterval, &setBlinkState);
  animate_Timer.attach_ms(LedConfig.animateinterval, &animateStep);
  setgroup_Timer.attach_ms(LedConfig.updateinterval*2, &setGroupState);

  // Show help & config:
  showConfiguration();

  Console.println("Enter 'H' for help ");
  Console.println();

  // Initialize boot time for uptime tracking
  bootTime = millis();

  // Ready to go, show prompt to show we are ready for input
  Console.print("> ");
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
 * Toggle blink state for CPU LED and animation timing
 * Switches blinkState between true/false and updates CPU LED accordingly
 */
void setBlinkState() {
  blinkState = !blinkState;
  (blinkState == true)?CPULED(0x40,0x00,0x00):CPULED(0x00,0x00,0x00);
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

  if (Console.available() > 0) {
    handleSerialInput();
  }

  //updateLEDs();
  if (SetGroupStateFlag) {
     FastLED.show();
     SetGroupStateFlag=false;
  }

  if (ChannelUpdate) {
    ChannelUpdate=false;
    updateGroups();
  }
}

/**
 * Handle serial input character by character
 * Supports backspace, cancel (ESC/Ctrl+C), and Command execution on newline
 */
void handleSerialInput() {
  while (Console.available() > 0) {
    char c = Console.read();

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
        if (escapeDigitCount < sizeof(escapeDigits)) {
          escapeDigits[escapeDigitCount++] = c;
        }
        escapeStartMillis = millis();
        continue;
      }

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

    lastCharWasCR = (c == '\n');

    switch (c) {
      case '\x03':
        Console.println("\nCANCELLED");
        resetInputBuffer();
        Console.print("> ");
        break;
      case '\r':
      case '\n':
        if (c == '\n' && !lastCharWasCR) {
          break;
        }
        lastCharWasCR = false;
        acceptCurrentLine();
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
 */
void redrawInputLine() {
  Console.print("\n> ");
  Console.print(inputBuffer);
  if (inputLength < lastRenderedLength) {
    for (uint16_t i = inputLength; i < lastRenderedLength; i++) {
      Console.print(' ');
    }
  }
  lastRenderedLength = inputLength;

  Console.print("\n> ");
  for (uint16_t i = 0; i < cursorPosition && i < inputLength; i++) {
    Console.print(inputBuffer[i]);
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
    Console.print('\a');
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
  Console.println();
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
  Console.print("> ");

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
    Console.print('\a');
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
  Console.println();

  if (input[0] == 0) {
    return;
  }

  char output[MAX_OUTPUT_LEN];
  memset(output, '\0', sizeof(output));

  int test = 0;
  int state;
  int Pct;
  int groupID;
  int strip;
  char Command = input[0];
  char *Data = input + 1;

  // Echo Command if enabled (useful for debugging/logging)
  if (LedConfig.CommandEcho) {
    Console.print("CMD> ");
    Console.println(input);
  }

  // Increment Command counter
  CommandCount++;

  if (Command >= MIN_COMMAND_CHAR && Command <= MAX_COMMAND_CHAR) {
    switch(Command) {

      // Help Command
      case 'H':
      case '?':
        showHelp();
        break;

      // Version information
      case 'V':
        Console.println("\n=== SIGHT Version Information ===");
        Console.print("Version          : ");
        Console.println(VERSION);
        Console.print("Build Date       : ");
        Console.println(__DATE__ " " __TIME__);
        Console.print("LED Type       : ");
        #ifdef USE_RGBW_LEDS
          Console.println("RGBW (4 bytes/LED)");
          Console.println("Chipset          : SK6812");
          Console.println("Color Order      : RGB");
        #else
          Console.println("RGB (3 bytes/LED)");
          Console.println("Chipset          : WS2812B");
          Console.println("Color Order      : GRB");
        #endif
        Console.print("MCU ID           : ");
        Console.println(mcuId);
        Console.println("=================================\n");
        break;

      // Display curren configuration
      case 'D':
        Console.print("Display configuration:\n" );
        showConfiguration();
        break;

      // Set configuration
      case 'C':
        setConfigParameters(Data);
        break;

      // Store current configuration
      case 'S':
        if (Data[0] == 'e' || Data[0] == 'E') {
          // Se - Export configuration as hex string
          Console.println("\n=== Configuration Export ===");
          Console.print("CONFIG:");
          uint8_t* configBytes = (uint8_t*)&LedConfig;
          for (size_t i = 0; i < sizeof(LedConfig); i++) {
            if (configBytes[i] < 16) Console.print("0");
            Console.print(configBytes[i], HEX);
          }
          Console.println();
          Console.println("============================");
          Console.println("Copy the CONFIG: line to backup this configuration.");
          Console.println("Use 'Li:CONFIG:<hex>' to restore it.\n");
        } else {
          // S - Save to flash
          Console.print("Save configuration: " );
          if (saveConfiguration()) Console.println("Success." );
          else                     Console.println("Failed..." );
        }
        break;

      // Load configuration from Flash/EEPROM
      case 'L':
        if (Data[0] == 'i' || Data[0] == 'I') {
          // Li - Import configuration from hex string
          if (Data[1] == ':' && strncmp(Data+2, "CONFIG:", 7) == 0) {
            char* hexData = Data + 9;
            size_t hexLen = strlen(hexData);
            size_t expectedLen = sizeof(LedConfig) * 2;

            if (hexLen == expectedLen) {
              // Validate hex characters before importing
              bool validHex = true;
              for (size_t i = 0; i < hexLen; i++) {
                char c = hexData[i];
                if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
                  validHex = false;
                  Console.print("ERROR: Invalid hex character '");
                  Console.print(c);
                  Console.print("' at position ");
                  Console.println(i);
                  break;
                }
              }

              if (validHex) {
                uint8_t* configBytes = (uint8_t*)&LedConfig;

                for (size_t i = 0; i < sizeof(LedConfig); i++) {
                  char byteStr[3] = {hexData[i*2], hexData[i*2+1], '\0'};
                  configBytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
                }

                Console.println("Configuration imported successfully!");
                Console.println("Use 'S' to save to flash, or 'R' to reboot and discard.");
              }
            } else {
              Console.print("ERROR: Invalid hex length. Expected ");
              Console.print(expectedLen);
              Console.print(" chars, got ");
              Console.println(hexLen);
            }
          } else {
            Console.println("ERROR: Format must be Li:CONFIG:<hex_string>");
          }
        } else {
          // L - Load from flash
          Console.print("Load configuration: " );
          loadConfiguration();
        }
        break;

      // Set Groups state
      case 'T':
        test = sscanf(Data, "%2d:%d", &groupID, &state);
        if (test == 2) {
          if (isValidGroup(groupID)) {
            if (isValidState(state)) {
              TermState[groupID -1] = state;
              TermPct[groupID -1] = 100;
              sprintf(output,"Group %d state set to %d", groupID, state);
              Console.println(output);
            } else {
              Console.print("ERROR: Invalid state '");
              Console.print(state);
              Console.println("', use 0-9");
            }
          } else {
            Console.print("ERROR: Invalid Group-ID '");
            Console.print(groupID);
            Console.println("', use 1-48");
          }
        } else
          Console.print("Syntax error: Use T<Group-ID>:<STATE>\n" );
        break;

      // Set Group state
      case 'P':
        test = sscanf(Data, "%2d:%d:%3d", &groupID, &state, &Pct);
        if (test == 3) {
          if (isValidGroup(groupID)) {
            if (isValidState(state)) {
              if (isValidPercent(Pct)) {
                TermState[groupID -1] = state;
                TermPct[groupID -1] = uint8_t(Pct);
                sprintf(output,"Group %d state set to %d with progress %d%%", groupID, state, Pct);
                Console.println(output);
              } else {
                Console.print("ERROR: Invalid percentage '");
                Console.print(Pct);
                Console.println("', use 0-100");
              }
            } else {
              Console.print("ERROR: Invalid state '");
              Console.print(state);
              Console.println("', use 0-9");
            }
          } else {
            Console.print("ERROR: Invalid Group-ID '");
            Console.print(groupID);
            Console.println("', use 1-48");
          }
        } else {
          Console.print("Syntax error: Use Pgg:s:ppp (gg=group 1-48, s=state 0-9, ppp=percent 0-100)\n");
        }
        break;

      // Set state for all Group
      case 'A':
        test = sscanf(Data, ":%d", &state);
        if (test == 1) {
          if (isValidState(state)) {
            Console.print("All groups set to state " );
            Console.println(state);
            for(int groupID = 1; groupID <= MAX_GROUPS; groupID++) {
              TermState[groupID -1 ] = state;
              TermPct[groupID -1] = 100;
            }
          } else {
            Console.print("ERROR: Invalid state '");
            Console.print(state);
            Console.println("', use 0-9");
          }
        } else
          Console.print("ERROR: Syntax error, use A:<STATE>\n" );
        break;

      // Set mass state, a digit for each group (48 max)
      case 'M':
        char ST;
        if ( Data[0] == ':') {
          int groupID=1;
          int totalChars = strlen(Data+1);

          while ( Data[groupID] != 0 && groupID<=MAX_GROUPS ) {
            ST=Data[groupID];
            state = atoi(&ST);
            if (state >= 0 && state <= 9) {
              TermState[groupID -1]=state;
              TermPct[groupID -1] = 100;
            }
            groupID++;
          }

          // Warn if there are surplus Values
          if (totalChars > MAX_GROUPS) {
            Console.print("WARNING: ");
            Console.print(totalChars - MAX_GROUPS);
            Console.print(" surplus Values ignored (max ");
            Console.print(MAX_GROUPS);
            Console.println(" groups)");
          }

          Console.print("Set ");
          Console.print(min(totalChars, MAX_GROUPS));
          Console.println(" group states");
        } else
          Console.print("Syntax error: Use M:<STATE><STATE<<STATE>...\n");
        break;

      // Reset all states to off
      case 'X':
        Console.print("Reset all Group states.\n" );
        for(int groupID = 0; groupID < MAX_GROUPS; groupID++) {
          TermState[groupID] = 0;
          TermPct[groupID] = 100;
        }
        setAllLEDs(CRGB::Black);
        break;

      // Show startup loop
      case 'W':
        Console.print("Showing startup loop.\n" );
        StartupLoop();
        break;

      // Memory usage report
      case 'I':
        {
          Console.println("\n=== System Information ===");
          Console.print("Free RAM         : ");
          Console.print(rp2040.getFreeHeap());
          Console.println(" bytes");
          Console.print("Total RAM        : ");
          Console.print(rp2040.getTotalHeap());
          Console.println(" bytes");
          Console.print("Used RAM         : ");
          Console.print(rp2040.getUsedHeap());
          Console.println(" bytes");
          FSInfo fs_info;
          LittleFS.info(fs_info);
          Console.print("Flash Used       : ");
          Console.print(fs_info.usedBytes);
          Console.println(" bytes");
          Console.print("Flash Total      : ");
          Console.print(fs_info.totalBytes);
          Console.println(" bytes");

          // Uptime and statistics
          uint32_t uptime = (millis() - bootTime) / 1000;
          uint32_t days = uptime / 86400;
          uint32_t hours = (uptime % 86400) / 3600;
          uint32_t minutes = (uptime % 3600) / 60;
          uint32_t seconds = uptime % 60;

          Console.print("Uptime           : ");
          if (days > 0) {
            Console.print(days);
            Console.print("d ");
          }
          Console.print(hours);
          Console.print("h ");
          Console.print(minutes);
          Console.print("m ");
          Console.print(seconds);
          Console.println("s");

          Console.print("Commands         : ");
          Console.println(CommandCount);
          Console.print("Errors           : ");
          Console.println(errorCount);
          Console.println("==========================\n");

          // Count groups in each state
          int stateCounts[10] = {0};
          int activeGroups = 0;

          for (int i = 0; i < MAX_GROUPS; i++) {
            if (TermState[i] >= 0 && TermState[i] <= 9) {
              stateCounts[TermState[i]]++;
              if (TermState[i] > 0) activeGroups++;
            }
          }

          Console.print("Active groups    : ");
          Console.print(activeGroups);
          Console.print(" / ");
          Console.println(MAX_GROUPS);

          Console.println("\nGroups per state:");
          for (int state = 0; state <= MAX_STATE; state++) {
            if (stateCounts[state] > 0) {
              Console.print("  state ");
              Console.print(state);
              Console.print(": ");
              Console.print(stateCounts[state]);
              Console.print(" group");
              if (stateCounts[state] != SINGLE_GROUP) Console.print("s");
              Console.println();
            }
          }

          Console.println("\n=== Group states ===");

          for(int Index = 0; Index < LedConfig.numChannels; Index++) {
            // Use manual channel order mapping
            strip = LedConfig.channelOrder[Index] - 1;

            Console.print("Channel ");
            Console.print(strip + 1);
            Console.print(" (Groups ");
            sprintf(output, "%2d",Index*LedConfig.numGroupsPerChannel+1);
            Console.print(output);
            Console.print("-");
            sprintf(output, "%2d",Index*LedConfig.numGroupsPerChannel+LedConfig.numGroupsPerChannel);
            Console.print(output);
            Console.print("): ");

            for(int term = 0; term < LedConfig.numGroupsPerChannel; term++) {
              int groupIdx = (Index*LedConfig.numGroupsPerChannel)+term;
              Console.print(TermState[groupIdx]);
              if (TermPct[groupIdx] < 100) {
                Console.print("(");
                Console.print(TermPct[groupIdx]);
                Console.print("%)");
              }
              Console.print(' ');
            }
            Console.println();
          }
          Console.println();
        }
        break;

      // Reboot controller
      case 'R':
        Console.print("Rebooting controller...\n" );
        rebootMCU();
        break;

      default:
        Console.print("ERROR: Command '");
        Console.print(Command);
        Console.println("' unknown. Use H for help.");
        errorCount++;
        break;
    }
  } else {
    Console.print("SYNTAX ERROR: '");
    Console.print(input[0]);
    Console.println("' is not a valid command. Use A-Z commands only, H for help.");
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
    Console.println("ERROR: Channel order must be 1-8 digits");
    return false;
  }

  // Temporary array to track used channels
  bool usedChannels[9] = {false}; // Channels 1-8

  // Parse each character
  for (size_t i = 0; i < len; i++) {
    char c = orderStr[i];

    // Check if character is a digit
    if (c < '1' || c > '8') {
      Console.print("ERROR: Invalid channel '");
      Console.print(c);
      Console.println("' - must be 1-8");
      return false;
    }

    uint8_t channel = c - '0';

    // Check for duplicates
    if (usedChannels[channel]) {
      Console.print("ERROR: Duplicate channel '");
      Console.print(channel);
      Console.println("' in order");
      return false;
    }

    usedChannels[channel] = true;
    orderArray[i] = channel;
  }

  // If less than 8 channels specified, append missing channels in numeric order
  if (len < 8) {
    Console.print("WARNING: Only ");
    Console.print(len);
    Console.print(" channel(s) specified, appending missing channels: ");

    size_t currentIndex = len;
    for (uint8_t ch = 1; ch <= 8 && currentIndex < 8; ch++) {
      if (!usedChannels[ch]) {
        orderArray[currentIndex++] = ch;
        Console.print(ch);
      }
    }
    Console.println();
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
      Console.print("ERROR: Channel ");
      Console.print(i + 1);
      Console.println(" not found in order but numChannels requires it");
      return false;
    }
  }

  return true;
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
  Console.println("\n=== SIGHT Command Reference ===");
  Console.println("  V                             Show version information");
  Console.println("  H                             Show this help");
  Console.println("  D                             Display current configuration");
  Console.println("  I                             Show system info and group states");
  Console.println("  S                             Save configuration to flash");
  Console.println("  Se                            Save/Export configuration as hex (backup)");
  Console.println("  L                             Load configuration from flash");
  Console.println("  Li:CONFIG:                    Load/Import configuration from hex (restore)");
  Console.println("  R                             Reboot controller");
  Console.println("  W                             Show startup loop animation");
  Console.println();

  Console.println("Group Control:");
  Console.print  ("  T<groupID>:<state>            Set Group state. groupID: 1-");
  Console.print  (MAX_GROUPS);
  Console.println(" and state: 0-9");
  Console.print  ("  P<groupID>:<state>:<Pct>      Set Group state. groupID: 1-");
  Console.print  (MAX_GROUPS);
  Console.println(", state: 0-9, PCt=0-100% progress");
  Console.println("  M:<state><state>...           Set state for multiple Groups sequentially (e.g. '113110')");
  Console.println("  A:<state>                     Set state for all Groups, state (0-9)");
  Console.println("  X                             Set all states to off (same as 'A:0')");
  Console.println();

  Console.println("Configuration (C prefix):");
  Console.println("  Cn:<string>                   Set Controller name (ID) (1-16 chars)");
  Console.print  ("  Cl:<Value>                    Set amount of LEDs per channel (");
  Console.print  (NUM_LEDS_PER_CHANNEL_MIN);
  Console.print  ("-");
  Console.print  (NUM_LEDS_PER_CHANNEL_MAX);
  Console.println(")");
  Console.print  ("  Ct:<Value>                    Set amount of groups per channel (1-");
  Console.print  (NUM_GROUPS_PER_CHANNEL_MAX);
  Console.println(")");
  Console.print  ("  Cs:<Value>                    Set amount of active channels (1-");
  Console.print  (NUM_CHANNELS_MAX);
  Console.println(")");
  Console.print  ("  Cw:<Value>                    Set spacer-width (LEDs between groups, 0-");
  Console.print  (SPACER_WIDTH_MAX);
  Console.println(")");
  Console.print  ("  Co:<Value>                    Set starting offset (skipping leds at start of channel, 0-");
  Console.print  (START_OFFSET_MAX);
  Console.println(")");
  Console.print  ("  Cb:<Value>                    Set blink-interval in msec (");
  Console.print  (BLINK_INTERVAL_MIN);
  Console.print  ("-");
  Console.print  (BLINK_INTERVAL_MAX);
  Console.println(")");
  Console.print  ("  Cu:<Value>                    Set update interval in mSec (");
  Console.print  (UPDATE_INTERVAL_MIN);
  Console.print  ("-");
  Console.print  (UPDATE_INTERVAL_MAX);
  Console.println(")");
  Console.print  ("  Ca:<Value>                    Set animate-interval in msec (");
  Console.print  (ANIMATE_INTERVAL_MIN);
  Console.print  ("-");
  Console.print  (ANIMATE_INTERVAL_MAX);
  Console.println(")");
  Console.println("  Ci:<Value>                    Set brightness intensity (0-255)");
  Console.println("  Cf:<anim>:<fade-in>:<fade-out> Configure animation + 2-step fade-in/out (0-255)");
  Console.println("  Cc:<state>:<Value>            Set color for state in HEX RGB order (state 1-9, Value: RRGGBB)");
  Console.println("  Cp:<state>:<pattern>          Set display-pattern for state in (state: 0-9, pattern 0-9) [for colorblind assist]");
  Console.println("  Cz:<order>                    Set channel order (N=standard 12345678, or custom like 43215678)");
  Console.println("  C4:<yes/true/no/false>        Set RGBW leds (4bytes) instead of RGB (3bytes) (False/True)");
  Console.print  ("  Cx:<channel>:<cpio-pin>       Set CPIO pin (");
  Console.print  (GPIO_PIN_MIN);
  Console.print  ("-");
  Console.print  (GPIO_PIN_MAX);
  Console.println(") per channel (1-8)");
  Console.println("  Cd:                           Reset all settings to factory defaults");
  Console.println("\n");
  Console.println("  L                             Load stored configuration from EEPROM/FLASH");
  Console.println("  S                             Save configuration to EEPROM/FLASH");
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

    int32_t step = (abs(difference) * percent) / 100;
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
 * @param group Group number (0-47)
 * @param state Display state (0-9)
 * @param Pct Percentage fill (0-100)
 */
void setLEDGroup(uint8_t group, uint8_t state, uint8_t Pct) {
  int pattern = 0, channelIndex = 0, groupIndex = 0, startLEDIndex = 0, groupWidth = 0;

  pattern = LedConfig.state_pattern[state];
  channelIndex = floor(group / LedConfig.numGroupsPerChannel);

  // Use manual channel order mapping
  if (channelIndex >= 0 && channelIndex < 8) {
    channelIndex = LedConfig.channelOrder[channelIndex] - 1; // Convert to 0-based index
  }

  groupIndex = group % LedConfig.numGroupsPerChannel;
  startLEDIndex = groupIndex * round(LedConfig.numLedsPerChannel / LedConfig.numGroupsPerChannel) + LedConfig.startOffset ;
  groupWidth = (LedConfig.numLedsPerChannel / LedConfig.numGroupsPerChannel) - LedConfig.spacerWidth;
  const int fullGroupWidth = groupWidth;

  // Clear entire group only when no fade is configured (legacy behavior)
  if (pattern < 8 && LedConfig.fading2StepOut == 0) {
    for(int i = 0; i < fullGroupWidth; i++) {
      leds[channelIndex][startLEDIndex + i] = CRGB::Black;
      ZERO_W(leds[channelIndex][startLEDIndex + i]);
    }
  }

  // for percentage
  bool isPartialFill = false;
  if (Pct < 100) {
    float factor = float(Pct) / 100.0f;
    groupWidth = round(float(groupWidth) * factor);
    if (groupWidth == 0 && Pct > 0) {
      groupWidth = 1;  // only no leds on 0%
    }
  }

  if (groupWidth < fullGroupWidth) {
    isPartialFill = true;
  }

  switch (pattern) {
      case 0: animate_Step[pattern]=animate_Step[pattern]%1;
              // 0 solid no blink    [########]
              //                     [########]
              for(int i = 0; i < groupWidth; i++) {
                applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], true, LedConfig.state_color[state]);
              }
              break;

      case 1:	animate_Step[pattern]=animate_Step[pattern]%2;
              // 1 solid blink       [########]
              //                     [        ]
              for(int i = 0; i < groupWidth; i++) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], blinkState, LedConfig.state_color[state]);
              }
              break;

      case 2:	animate_Step[pattern]=animate_Step[pattern]%2;
              // 1 solid blink inv.  [        ]
              //                     [########]
              for(int i = 0; i < groupWidth; i++) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], !blinkState, LedConfig.state_color[state]);
              }
              break;

      case 3: animate_Step[pattern]=animate_Step[pattern]%2;
              // 2 Alternate L/R     [####    ] Count up c=0->1  s,s+(n/2) *c
              //                     [    ####]                  (n/2),n   *!c
              for(int i = 0; i < (groupWidth/2); i++) {
                  int activeIndex = startLEDIndex + i + ( blinkState * (groupWidth/2));
                  int inactiveIndex = startLEDIndex + i + (!blinkState * (groupWidth/2));
                  applyTwoStepPixel(leds[channelIndex][activeIndex], true, LedConfig.state_color[state]);
                  applyTwoStepPixel(leds[channelIndex][inactiveIndex], false, LedConfig.state_color[state]);
              }
              break;

      case 4: animate_Step[pattern]=animate_Step[pattern]%2;
              // 3 Alternate in/out  [##      ##] Count up c=0->1  s,s+(n/4) + n-(n/4),n
              //                     [  ##  ##  ]                  s+(n/4)-(n/2)+(n/4)
              for(int i = 0; i < (groupWidth); i++) {
                bool turnOn = (i < (groupWidth/4) || i >= (groupWidth-(groupWidth/4))) ? blinkState : !blinkState;
                applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], turnOn, LedConfig.state_color[state]);
              }
              break;

      case 5: animate_Step[pattern]=animate_Step[pattern]%groupWidth;
              // 4 odd/even          [# # # # ]
              //                     [ # # # #]
              for(int i = 0; i < (groupWidth); i++) {
                  bool turnOn = (i % 2) ? blinkState : !blinkState;
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], turnOn, LedConfig.state_color[state]);
              }
              break;

      case 6: animate_Step[pattern]=animate_Step[pattern]%1;
              // 10 1/3 gated blink    [###  ###]
              for(int i = 0; i < groupWidth; i++) {
                if ( i <= (groupWidth/3) or i >= (groupWidth-(groupWidth/3)-1) ) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], true, LedConfig.state_color[state]);
                }
              }
              break;
      case 7: animate_Step[pattern]=animate_Step[pattern]%2;
              // gated blink    [###  ###]
              //                [        ]
              for(int i = 0; i < groupWidth; i++) {
                if ( i <= (groupWidth/3) or i >= (groupWidth-(groupWidth/3)-1) ) {
                  applyTwoStepPixel(leds[channelIndex][startLEDIndex + i], blinkState, LedConfig.state_color[state]);
                }
              }
              break;

      case 8: animate_Step[pattern]=animate_Step[pattern]%(groupWidth);
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
              leds[channelIndex][startLEDIndex + animate_Step[pattern] ] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + animate_Step[pattern]]);
              break;
              // if ( i/(groupWidth/2) == 0 )
              //   leds[channelIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              // break;

      case 9: animate_Step[pattern]=animate_Step[pattern]%(groupWidth);
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
              leds[channelIndex][startLEDIndex + groupWidth - animate_Step[pattern] - 1] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + groupWidth - animate_Step[pattern] - 1]);
              break;

      case 10: animate_Step[pattern]=animate_Step[pattern]%((groupWidth*2));
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

              if (animate_Step[pattern] < groupWidth ) {
                leds[channelIndex][startLEDIndex + animate_Step[pattern]] = LedConfig.state_color[state];
                ZERO_W(leds[channelIndex][startLEDIndex + animate_Step[pattern]]);
              } else {
                leds[channelIndex][startLEDIndex + (groupWidth - (animate_Step[pattern] - groupWidth)) - 1] = LedConfig.state_color[state];
                ZERO_W(leds[channelIndex][startLEDIndex + (groupWidth - (animate_Step[pattern] - groupWidth)) - 1]);
              }
              break;

      case 11: animate_Step[pattern]=animate_Step[pattern]%(groupWidth/2);
              // 8 Animate ><        [#      #]
              //                     [ #    # ]
              //                     [  #  #  ]
              //                     [   ##   ]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              leds[channelIndex][startLEDIndex + animate_Step[pattern]] = LedConfig.state_color[state];
              leds[channelIndex][startLEDIndex+groupWidth - animate_Step[pattern] -1] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + animate_Step[pattern]]);
              ZERO_W(leds[channelIndex][startLEDIndex+groupWidth - animate_Step[pattern] -1]);
              break;

      case 12: animate_Step[pattern]=animate_Step[pattern]%(groupWidth/2);

              // 9 Animate ><        [   ##   ]
              //                     [  #  #  ]
              //                     [ #    # ]
              //                     [#      #]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fadingAnimation);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              leds[channelIndex][startLEDIndex + (groupWidth/2) - animate_Step[pattern] - 1] = LedConfig.state_color[state];
              leds[channelIndex][startLEDIndex + (groupWidth/2) + animate_Step[pattern]] = LedConfig.state_color[state];
              ZERO_W(leds[channelIndex][startLEDIndex + (groupWidth/2) - animate_Step[pattern] - 1]);
              ZERO_W(leds[channelIndex][startLEDIndex + (groupWidth/2) + animate_Step[pattern]]);
              break;

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
  return (groupID >= MIN_GROUP_ID && groupID <= MAX_GROUPS);
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

  // Apply the color with current brightness
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
  char *comma = NULL; // Declare outside switch to avoid jump errors
  if (Data[1] == ':') {
    switch (configItem) {
      // set Name-Idenitfier
      case 'n':
        // Check is Value is not bigger than size of LedConfig.identifier
        if (strlen(Value) < IDENTIFIER_MAX_LENGTH ) {
          strncpy(LedConfig.identifier, Value, IDENTIFIER_MAX_LENGTH - 1);
          LedConfig.identifier[IDENTIFIER_MAX_LENGTH - 1] = '\0'; // Ensure null-termination
          Console.print("Controller Name (ID)   : " );
          Console.println(LedConfig.identifier);
        } else {
          Console.println("Identifier too long, use 16 characters max.");
        }
        break;
      // set led per channel
      case 'l':
        ValueInt = atoi(Value);
        if (ValueInt >= NUM_LEDS_PER_CHANNEL_MIN and ValueInt <= NUM_LEDS_PER_CHANNEL_MAX) {
          Console.print("LEDs per channel      : " );
          LedConfig.numLedsPerChannel = ValueInt;
          Console.println(LedConfig.numLedsPerChannel);
          FastLED.clearData();
        } else {
          Console.print("Invalid number of leds per channel(");
          Console.print(NUM_LEDS_PER_CHANNEL_MIN);
          Console.print("-");
          Console.print(NUM_LEDS_PER_CHANNEL_MAX);
          Console.println(")");
        }
        break;
      // set groups per channel
      case 't':
        ValueInt = atoi(Value);
        if (ValueInt >= 1 and ValueInt <= NUM_GROUPS_PER_CHANNEL_MAX) {
          // Check if total groups would exceed MAX_GROUPS
          if (LedConfig.numChannels * ValueInt > MAX_GROUPS) {
            Console.print("ERROR: Channels (");
            Console.print(LedConfig.numChannels);
            Console.print(") * Groups Per Channel (");
            Console.print(ValueInt);
            Console.print(") = ");
            Console.print(LedConfig.numChannels * ValueInt);
            Console.print(" exceeds MAX_GROUPS (");
            Console.print(MAX_GROUPS);
            Console.println(")!");
          } else {
            Console.print("Groups per channel : " );
            LedConfig.numGroupsPerChannel = ValueInt;
            Console.println(LedConfig.numGroupsPerChannel);
            FastLED.clearData();
          }
        } else {
          Console.print("Invalid number of groups per channel (1-");
          Console.print(NUM_GROUPS_PER_CHANNEL_MAX);
          Console.println(")");
        }
        break;
      // set number of channels
      case 's':
        ValueInt = atoi(Value);
        if (ValueInt >= 1 and ValueInt <= NUM_CHANNELS_MAX) {
          // Check if total groups would exceed MAX_GROUPS
          if (ValueInt * LedConfig.numGroupsPerChannel > MAX_GROUPS) {
            Console.print("ERROR: Channels (");
            Console.print(ValueInt);
            Console.print(") * Groups Per Channel (");
            Console.print(LedConfig.numGroupsPerChannel);
            Console.print(") = ");
            Console.print(ValueInt * LedConfig.numGroupsPerChannel);
            Console.print(" exceeds MAX_GROUPS (");
            Console.print(MAX_GROUPS);
            Console.println(")!");
          } else {
            Console.print("Amount of channels    : " );
            LedConfig.numChannels = ValueInt;
            Console.println(LedConfig.numChannels);
            FastLED.clearData();
          }
        } else {
          Console.print("Invalid number of channels (1-");
          Console.print(NUM_CHANNELS_MAX);
          Console.println(")");

        }
        break;
      // set spacer width
      case 'w':
        ValueInt = atoi(Value);
        if (ValueInt >= 0 and ValueInt <= SPACER_WIDTH_MAX) {
          Console.print("Spacer width         : " );
          LedConfig.spacerWidth = ValueInt;
          Console.println(LedConfig.spacerWidth);
          FastLED.clearData();
        } else {
          Console.print("Invalid space width(0-");
          Console.print(SPACER_WIDTH_MAX);
          Console.println(")");
        }
        break;
      case 'o':
        ValueInt = atoi(Value);
        if (ValueInt >= 0 and ValueInt <= START_OFFSET_MAX) {
          Console.print("Start offset         : " );
          LedConfig.startOffset = ValueInt;
          Console.println(LedConfig.startOffset);
          FastLED.clearData();
        } else {
          Console.print("Invalid start offset (0-");
          Console.print(START_OFFSET_MAX);
          Console.println(")");
        }
        break;
      // Set animate interval
      case 'a':
        ValueInt = atoi(Value);
        if (ValueInt >= ANIMATE_INTERVAL_MIN and ValueInt <= ANIMATE_INTERVAL_MAX) {
          Console.print("Animation interval   : " );
          LedConfig.animateinterval = ValueInt;
          Console.println(LedConfig.animateinterval);
          animate_Timer.detach();
          animate_Timer.attach_ms(LedConfig.animateinterval, &animateStep);
          FastLED.clearData();
        } else {
          Console.print("Invalid animate interval (");
          Console.print(ANIMATE_INTERVAL_MIN);
          Console.print("-");
          Console.print(ANIMATE_INTERVAL_MAX);
          Console.println(" msec)");
        }
        break;
      // Set blink interval
      case 'b':
        ValueInt = atoi(Value);
        if (ValueInt >= BLINK_INTERVAL_MIN and ValueInt <= BLINK_INTERVAL_MAX) {
          if (ValueInt > LedConfig.updateinterval) {
            Console.print("Blinking interval    : " );
            LedConfig.blinkinterval = ValueInt;
            Console.println(LedConfig.blinkinterval);
            blink_Timer.detach();
            blink_Timer.attach_ms(LedConfig.blinkinterval, &setBlinkState);
            FastLED.clearData();
          } else {
            Console.print("Invalid blinking-interval, needs to be bigger than current update-interval (");
            Console.print(LedConfig.updateinterval);
            Console.println(")");
          }
        } else {
          Console.print("Invalid blink interval (");
          Console.print(BLINK_INTERVAL_MIN);
          Console.print("-");
          Console.print(BLINK_INTERVAL_MAX);
          Console.println(" msec)");
        }
          break;

      // Set update interval
      case 'u':
        ValueInt = atoi(Value);
        if (ValueInt >= UPDATE_INTERVAL_MIN and ValueInt <= UPDATE_INTERVAL_MAX) {
          if (ValueInt < LedConfig.blinkinterval) {
            Console.print("Update interval      : ");
            LedConfig.updateinterval = ValueInt;
            Console.println(LedConfig.updateinterval);
            setgroup_Timer.detach();
            setgroup_Timer.attach_ms(LedConfig.updateinterval*2, &setGroupState);
            update_Timer.detach();
            update_Timer.attach_ms(LedConfig.updateinterval, &writeChannelData);

            // Correct fade times
            // LedConfig.fadingAnimation = LedConfig.fadingAnimation * (LedConfig.updateinterval/UPDATE_INTERVAL_DEFAULT);
            // LedConfig.fading2Step = LedConfig.fading2Step * (LedConfig.updateinterval/UPDATE_INTERVAL_DEFAULT);
            //  32     64    128    160
            //  10     20    40     50

            FastLED.clearData();
          } else {
            Console.print("Invalid update-interval, needs to be smaller than current blink-interval (");
            Console.print(LedConfig.blinkinterval);
            Console.println(")");
          }
        } else {
          Console.print("Invalid update interval (");
          Console.print(UPDATE_INTERVAL_MIN);
          Console.print("-");
          Console.print(UPDATE_INTERVAL_MAX);
          Console.println(" msec)");
        }
        break;

      // Set Brightness Inetensity
      case 'i':
        ValueInt = atoi(Value);
        if (ValueInt >= BRIGHTNESS_MIN and ValueInt <= BRIGHTNESS_MAX) {
          Console.print("Brightness intensity   : " );
          LedConfig.brightness = ValueInt;
          Console.println(LedConfig.brightness);
          FastLED.setBrightness(LedConfig.brightness);
          FastLED.clearData();

          // Warn if brightness is very high
          if (ValueInt > BRIGHTNESS_WARNING_THRESHOLD) {
            Console.println("WARNING: High brightness may cause overheating or exceed power supply capacity!");
          }
        } else {
          Console.print("Invalid brightness intensity (");
          Console.print(BRIGHTNESS_MIN);
          Console.print("-");
          Console.print(BRIGHTNESS_MAX);
          Console.println(")");
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
            int parsed = atoi(token);
            if (parsed < 0 || parsed > 255) {
              valid = false;
              break;
            }
            values[idx++] = parsed;
            token = strtok(NULL, ":");
          }

          if (!valid) {
            Console.println("Invalid fade values (0-255)");
          } else if (idx == 0) {
            Console.println("Usage: Cf:<anim>:<fade-in>:<fade-out>");
          } else if (idx == 1) {
            // Backwards compatibility: Cf:<Value>
            LedConfig.fadingAnimation = values[0];
            LedConfig.fading2StepIn = values[0] / 2;
            LedConfig.fading2StepOut = values[0] / 2;
            Console.print("Fading factor          : ");
            Console.print(LedConfig.fadingAnimation);
            Console.print(" (2-step fade in/out: ");
            Console.print(LedConfig.fading2StepIn);
            Console.println(")");
            FastLED.clearData();
          } else if (idx == 2) {
            LedConfig.fadingAnimation = values[0];
            LedConfig.fading2StepIn = values[1];
            LedConfig.fading2StepOut = values[1];
            Console.print("Animation fading factor: ");
            Console.print(LedConfig.fadingAnimation);
            Console.print(", 2-step fade (in/out): ");
            Console.println(LedConfig.fading2StepIn);
            FastLED.clearData();
          } else if (idx == 3) {
            LedConfig.fadingAnimation = values[0];
            LedConfig.fading2StepIn = values[1];
            LedConfig.fading2StepOut = values[2];
            Console.print("Animation fading factor: ");
            Console.print(LedConfig.fadingAnimation);
            Console.print(", 2-step fade in/out: ");
            Console.print(LedConfig.fading2StepIn);
            Console.print(" / ");
            Console.println(LedConfig.fading2StepOut);
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
          Console.println("Channel order set to: 12345678 (standard)");
          FastLED.clearData();
        } else {
          // Try to parse as custom channel order
          uint8_t tempOrder[8];
          if (validateChannelOrder(Value, tempOrder, LedConfig.numChannels)) {
            // Copy validated order to config
            for (int i = 0; i < 8; i++) {
              LedConfig.channelOrder[i] = tempOrder[i];
            }
            Console.print("Channel order set to: ");
            for (int i = 0; i < 8; i++) {
              Console.print(LedConfig.channelOrder[i]);
            }
            Console.println();
            FastLED.clearData();
          } else {
            Console.println("Invalid channel order. Use: N (standard 12345678) or custom like 43215678");
          }
        }
        break;

      // Toggle startup animation
      case 'g':
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.startupAnimation = false;
          Console.println("Startup animation    : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.startupAnimation = true;
          Console.println("Startup animation    : Enabled");
        } else {
          Console.println("Invalid Value, use Y/N or 1/0");
        }
        break;

      // Toggle Command echo
      case 'e':
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.CommandEcho = false;
          Console.println("Command echo         : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.CommandEcho = true;
          Console.println("Command echo         : Enabled");
        } else {
          Console.println("Invalid Value, use Y/N or 1/0");
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
        Console.println("Configuration reset to defaults\n");
        FastLED.clearData();
        break;
      default:
        Console.print("SYNTAX ERROR: Configuration item '");
        Console.print(configItem);
        Console.println("' unknown. Use H for help.");
        break;
    }
  } else {
    Console.print("SYNTAX ERROR: Invalid configuration format. Use C<item>:<value> format, H for help.\n");
  }
}

/**
 * Set GPIO pins for LED channels from Command input
 * @param Value Comma-separated list of GPIO pin numbers (e.g., "2,3,4,5,6,7,8,9")
 */
void setLedStripGPIO(char *Value) {
  if (Value[1] == ':') {
    uint8_t channel = Value[0] - '0' -1;

    if (channel >= 0 && channel < NUM_CHANNELS_DEFAULT) {
      char *GPIO_RAW = Value + 2;
      uint8_t GPIO_PIN = strtoul(GPIO_RAW, NULL, 16);
      if ( GPIO_PIN > GPIO_PIN_MIN && GPIO_PIN <= GPIO_PIN_MAX) {

        // Test if GPIO pin is not assigned already
        for (uint8_t CHANNEL=0; CHANNEL<NUM_CHANNELS_DEFAULT ; CHANNEL++) {
          if (GPIO_PIN == LedConfig.channelGPIOpin[CHANNEL]) {
            Console.print("ERROR: GPIO-PIN ");
            Console.print(GPIO_PIN);
            Console.print(" is already used for channel ");
            Console.print(CHANNEL);
            Console.print(" !");
            return;
          }
        }
        if (GPIO_PIN == CPULED_GPIO) {
          Console.print("ERROR: GPIO-PIN ");
          Console.print(GPIO_PIN);
          Console.print(" is already used for CPULED !");
          return;
        }

        LedConfig.channelGPIOpin[channel] = GPIO_PIN;
        Console.print("GPIO-PIN for channel ");
        Console.print(channel);
        Console.print(" is set to : ");
        Console.print(LedConfig.channelGPIOpin[channel]);
        Console.println();
        Console.println("Please note a MCU reboot is required to activate a change in GPIO pin assignments");
      } else {
        Console.println("Invalid GPIO-PIN number.");
      }
    } else {
      Console.print  ("Invalid channel, 1-");
      Console.print  (NUM_CHANNELS_MAX);
      Console.println(" only.");
    }
  } else {
      Console.println("Syntax error: use Cx:<channel>:<GPIO-PIN>     (<channel: 1-");
      Console.print  (NUM_CHANNELS_MAX);
      Console.print  (", <GPIO-PIN>: ");
      Console.print  (GPIO_PIN_MIN);
      Console.print  ("-");
      Console.print  (GPIO_PIN_MAX);
      Console.println(")\n");
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
      uint32_t RGB = strtoul(Color, NULL, 16);
      if (RGB > 0 && RGB < 0xFFFFFFFF) {
        LedConfig.state_color[state] = RGB + 0xFF000000; // Add brightness
        sprintf(buffer, "%06X", (int)RGB);
        Console.print("Color for state ");
        Console.print(state);
        Console.print(" is set to : ");
        snprintf(buffer, strlen(buffer), "%02X%02X%02X", LedConfig.state_color[state].red, LedConfig.state_color[state].green, LedConfig.state_color[state].blue);
        Console.print(buffer);
        Console.println(" (RR GG BB)");
      } else {
        Console.println("Invalid color");
      }
    } else {
      Console.println("Invalid state, 1-9 only");
    }
  } else {
    Console.println("Syntax error: use Cc:<STATE>:<BBGGRR>   (<state>: 1-9, <BBGGRR>: Color in Hex))\n");
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
    Value[4]=0; // No more that 2 chars (digits) allowed
    uint8_t state = Value[0] - '0';
    if (state > 0 && state <= 9) {
      int pattern=atoi(Value+2);
      if ( pattern <= 12) {
        LedConfig.state_pattern[state] = pattern;
        Console.print("Pattern for state ");
        Console.print(state);
        Console.print(" is set to : ");
        Console.print(LedConfig.state_pattern[state]);
        Console.println();
      } else {
        Console.println("Invalid pattern");
      }
    } else {
      Console.println("Invalid state, 1-12 only");
    }
  } else {
    Console.println("Syntax error: use Cp:<state>:<pattern>     (<state>: 0-9, <pattern>: 0-9)\n");
  }
}

/**
 * Reset all configuration parameters to default Values
 * Restores factory settings and clears any custom configurations
 */
void resetToDefaults() {
  CPULED(0x00,0x00,0x80);

  strcpy(LedConfig.identifier, IDENTIFIER_DEFAULT);
  LedConfig.numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
  LedConfig.numChannels = NUM_CHANNELS_DEFAULT;
  LedConfig.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
  LedConfig.spacerWidth = SPACER_WIDTH_DEFAULT;
  LedConfig.startOffset = START_OFFSET;
  LedConfig.blinkinterval = BLINK_INTERVAL;
  LedConfig.updateinterval = UPDATE_INTERVAL;
  LedConfig.brightness = BRIGHTNESS;
  LedConfig.fadingAnimation = FADING;
  LedConfig.fading2StepIn = FADING_2STEP_IN;
  LedConfig.fading2StepOut = FADING_2STEP_OUT;
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

  Console.print("Identifier           : ");
  Console.println(LedConfig.identifier);

  Console.print("LEDs per channel     : ");
  Console.println(LedConfig.numLedsPerChannel);

  Console.print("Groups per channel   : ");
  Console.println(LedConfig.numGroupsPerChannel);

  Console.print("Amount of channels   : ");
  Console.println(LedConfig.numChannels);

  Console.print("Spacer width         : ");
  Console.println(LedConfig.spacerWidth);

  Console.print("Start Offset         : ");
  Console.println(LedConfig.startOffset);

  Console.print("Blinking interval    : ");
  Console.println(LedConfig.blinkinterval);

  Console.print("Update interval      : ");
  Console.println(LedConfig.updateinterval);

  Console.print("Animate interval     : ");
  Console.println(LedConfig.animateinterval);

  Console.print("Animation fading     : ");
  Console.println(LedConfig.fadingAnimation);

  Console.print("2-step fade (in/out) : ");
  Console.print(LedConfig.fading2StepIn);
  Console.print(" / ");
  Console.println(LedConfig.fading2StepOut);

  Console.print("Channel order        : ");
  for (int i = 0; i < NUM_CHANNELS_MAX; i++) {
    Console.print(LedConfig.channelOrder[i]);
  }
  Console.println();

  Console.println("LED Mode             : RGB-only (W channel = 0)");

  Console.print("Overall brightness   : ");
  Console.println(LedConfig.brightness);

  Console.println();
  Console.print("Channel              : | ");
  for (uint8_t channel=0; channel<NUM_CHANNELS_MAX; channel++) {
    sprintf(output, "%2d | ", channel+1);
    Console.print(output);
  }
  Console.println();
  Console.print("GPIO-PIN             : | ");
  for (uint8_t channel=0; channel<NUM_CHANNELS_MAX; channel++) {
    sprintf(output, "%02d | ", LedConfig.channelGPIOpin[channel] );
    Console.print(output);
  }
  Console.println();

  Console.println();
  Console.println("Color state          : RRGGBB    Pattern:");
  Console.println("            0        : 000000       0 (fixed)");
  for (int state=1; state<=9; state++) {
    sprintf(output, "            %d        : %02X%02X%02X      %2d",
      state,
      LedConfig.state_color[state].red,
      LedConfig.state_color[state].green,
      LedConfig.state_color[state].blue,
      LedConfig.state_pattern[state]
    );
    Console.println(output);
  }

  Console.println();
}

/**
 * Read Data from a file on LittleFS
 * @param path File path to read from
 * @return Pointer to allocated buffer containing file Data, or nullptr if failed
 */
char* readFile(const char * path) {
  CPULED(0x00,0x00,0x80);
  File fileH = LittleFS.open(F(path), "r");
  if (!fileH) {
    Console.print("NOTE: Failed opening confgfile\n" );
    return 0;
  }

  long fileSize=fileH.size();
  if (fileSize > 0) {
    char *Data = new char[fileSize+1];
    if (Data == nullptr) {
      Console.println("NOTE: Memory allocation failed!");
      fileH.close();
      return 0;
    }
    fileH.readBytes(Data, fileSize);
    Data[fileSize] = '\0';

    fileH.close();
    return Data;
  } else {
      Console.println("NOTE: File is empty!");
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
    Console.print("* Opening Failed\n" );
    return false;
  }
  size_t bytesWritten = fileH.write((const uint8_t*)Data, DataSize);
  if (bytesWritten != DataSize) {
    Console.print("* Write Failed\n");
    fileH.close();
    return false;
  }
  fileH.close();
  return true;
}

/**
 * Load configuration from LittleFS file
 * Reads saved configuration and applies it to LedConfig struct
 * @return true if successful, false if file not found or invalid
 */
bool loadConfiguration() {
  CPULED(0x00,0x00,0x80);

  char *buffer;
  buffer = readFile(CONFIG_FILENAME);
  if (buffer != 0) {

    // Calculate sizes to separate struct and checksum in buffer
    int structSize=sizeof(LedData);
    int totalSize = structSize + 32;  // 32 for the saved binary checksum

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
      // Checksums match, proceed to load LedConfig
      // DeSerialize buffer into LedData Struct
      memcpy(&LedConfig, buffer, structSize);
      delete[] buffer;  // Free memory

      // Validate and clamp all Values to safe ranges
      bool needsCorrection = false;

      if (LedConfig.numLedsPerChannel < NUM_LEDS_PER_CHANNEL_MIN || LedConfig.numLedsPerChannel > NUM_LEDS_PER_CHANNEL_MAX) {
        LedConfig.numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
        needsCorrection = true;
      }

      if (LedConfig.numChannels < 1 || LedConfig.numChannels > NUM_CHANNELS_MAX) {
        LedConfig.numChannels = NUM_CHANNELS_DEFAULT;
        needsCorrection = true;
      }

      if (LedConfig.numGroupsPerChannel < 1 || LedConfig.numGroupsPerChannel > NUM_GROUPS_PER_CHANNEL_MAX) {
        LedConfig.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
        needsCorrection = true;
      }

      // Critical: Ensure total groups doesn't exceed MAX_GROUPS
      if (LedConfig.numChannels * LedConfig.numGroupsPerChannel > MAX_GROUPS) {
        Console.println("WARNING: numChannels * numGroupsPerChannel exceeds MAX_GROUPS!");
        LedConfig.numChannels = NUM_CHANNELS_DEFAULT;
        LedConfig.numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
        needsCorrection = true;
      }

      if (LedConfig.spacerWidth > SPACER_WIDTH_MAX) {
        LedConfig.spacerWidth = SPACER_WIDTH_DEFAULT;
        needsCorrection = true;
      }

      if (LedConfig.startOffset > START_OFFSET_MAX) {
        LedConfig.startOffset = START_OFFSET;
        needsCorrection = true;
      }

      if (LedConfig.blinkinterval < BLINK_INTERVAL_MIN || LedConfig.blinkinterval > BLINK_INTERVAL_MAX) {
        LedConfig.blinkinterval = BLINK_INTERVAL;
        needsCorrection = true;
      }

      if (LedConfig.animateinterval < ANIMATE_INTERVAL_MIN || LedConfig.animateinterval > ANIMATE_INTERVAL_MAX) {
        LedConfig.animateinterval = ANIMATE_INTERVAL;
        needsCorrection = true;
      }

      if (LedConfig.updateinterval < UPDATE_INTERVAL_MIN || LedConfig.updateinterval > UPDATE_INTERVAL_MAX) {
        LedConfig.updateinterval = UPDATE_INTERVAL;
        needsCorrection = true;
      }

      if (LedConfig.brightness < BRIGHTNESS_MIN || LedConfig.brightness > BRIGHTNESS_MAX) {
        LedConfig.brightness = BRIGHTNESS;
        needsCorrection = true;
      }

      Console.println("Checksum matches, configuration loaded.");
      if (needsCorrection) {
        Console.println("WARNING: Some Values were out of range and corrected to defaults.");
        Console.println("Use 'S' to save corrected configuration.\n");
      } else {
        Console.println();
      }
      return true;
    } else {
      Console.println("Checksum mismatch, configuration is corrupted!\n");
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
 * Writes LedConfig struct to flash memory for persistence
 * @return true if successful, false otherwise
 */
bool saveConfiguration() {
  CPULED(0x00,0x00,0x80);
  // Serialize LedData Struct into char-array so we can save it
  char buffer[sizeof(LedData)];
  SHA256 sha256;
  memcpy(buffer, &LedConfig, sizeof(LedData));

  // Calculate SHA-256 checksum
  sha256.reset();
  sha256.update((const uint8_t*)buffer, sizeof(buffer));
  uint8_t hash[32];
  sha256.finalize(hash, sizeof(hash));

  // Create a final buffer containing the serialized Data + checksum (in binary)
  char finalBuffer[sizeof(buffer) + 32];           // 32 bytes for the checksum
  memcpy(finalBuffer, buffer, sizeof(buffer));     // Copy serialized LedData
  memcpy(finalBuffer + sizeof(buffer), hash, 32);  // Copy binary checksum after the Data (append)

  // Write the combined buffer to the file
  return writeFile(CONFIG_FILENAME, finalBuffer, sizeof(finalBuffer));
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
  uint8_t DELAY (LedConfig.numLedsPerChannel / 10);
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
    delay(DELAY);
  }

  //  Flash [######]
  setAllLEDs(CRGB::Green);
  FastLED.show();
  delay(75);
  for(int i = 0; i < 12; i++) {
    FadeAll(0,LedConfig.numLedsPerChannel,1.5);
    FastLED.show();
    delay(65);
  }

  // All Off [      ]
  //SetAllLEDs(CRGB::Black);
  FastLED.clear(true);
}

//
// BitBang code for CPU led (can't use FastLED as all 8 PIO chanels are used for the ledstrips)
//

// WS2812B CPULED timing (125 MHz = 8ns per cycle)
// T0H: 400ns = 50 cycles, T0L: 850ns = 106 cycles
// T1H: 800ns = 100 cycles, T1L: 450ns = 56 cycles
// Subtract ~8 cycles for gpio_put() overhead
#define T1H  92   // 800ns (100 cycles - 8 overhead)
#define T1L  48   // 450ns (56 cycles - 8 overhead)
#define T0H  42   // 400ns (50 cycles - 8 overhead)
#define T0L  98   // 850ns (106 cycles - 8 overhead)
#define RESET_TIME 60  // >50us reset time

inline void delay_cycles(uint32_t cycles) {
    // Simple cycle delay - each iteration is ~4 cycles
    while (cycles >= 4) {
        __asm volatile ("nop");
        cycles -= 4;
    }
}

/**
 * Send a single byte to CPU LED via bit-banging
 * @param byte Byte Value to send to CPU LED
 */
void sendByte_CPULED(uint8_t byte) {
  // Disable interrupts for precise timing
  noInterrupts();
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
  interrupts();
}

/**
 * Set CPU LED color using 32-bit color Value
 * @param color 32-bit RGB color Value
 */
void CPULED(uint32_t color) {
    sendByte_CPULED((color & 0x00FF0000) >> 16);  // Send red byte
    sendByte_CPULED((color & 0x0000FF00) >> 8);   // Send green byte
    sendByte_CPULED((color & 0x000000FF));        // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}

/**
 * Set CPU LED color using CRGB color object
 * @param color CRGB color object
 */
void CPULED(CRGB color) {
    sendByte_CPULED(color.r);  // Send red byte
    sendByte_CPULED(color.g);  // Send green byte
    sendByte_CPULED(color.b);  // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}

/**
 * Set CPU LED color using individual RGB components
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void CPULED(uint8_t r, uint8_t g, uint8_t b) {
    sendByte_CPULED(r);  // Send red byte
    sendByte_CPULED(g);  // Send green byte
    sendByte_CPULED(b);  // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}
