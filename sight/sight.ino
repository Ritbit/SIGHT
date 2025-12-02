/*
Name        : SIGHT (Shelf Indicators for Guided Handling Tasks)
Version     : 1.9
Date        : 2025-12-02
Author      : Bas van Ritbergen <bas.vanritbergen@adyen.com> / bas@ritbit.com
Description : LED strip controller with animations, RGBW support, and comprehensive safety features.

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

              v1.9 improvements:
              - Renamed shelf/strip/output to channel to make it more generic
              - Added support for 2-step fading apart from the regular fading
              - Renamed Fading to FadingAnimation to distinguish between the two
              - Fixed all codeStyle issues, made it more readable and consistent
              - Added more comments and documentation

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
#define BLINK_INTERVAL 200
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

// Enable/disable command echo for debugging
#define COMMAND_ECHO false

// System status LED configuration (dimmed brightness at 64 for visibility)
#define CPULED_STATUS_BRIGHTNESS 64  // Dimmed brightness for status indicators
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
#define FADING_2STEP 24

// *** IMPORTANT: Set this to match your LED strip type ***
// Uncomment ONE of these lines:

// For WS2812B RGB strips (3 bytes per LED)
#define USE_RGB_LEDS

// For WS2813B-RGBW, SK6812 RGBW strips (4 bytes per LED)
// #define USE_RGBW_LEDS


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
#include <Arduino.h>
#include <Ticker.h>
#include <Crypto.h>
#include <SHA256.h>
#include "LittleFS.h"
#include <FastLED.h>
#include "FastLED_RGBW.h"  // Always include for RGBW support
#include "hardware/watchdog.h"

// Figure MCU type/serial
#include <MicrocontrollerID.h>
char mcuId[41];

// Declare LedStrip control arrays
#if LED_TYPE == 4
  // RGBW strips: Use CRGBW arrays (4 bytes per LED)
  CRGBW leds[NUM_CHANNELS_DEFAULT+1][NUM_LEDS_PER_CHANNEL_MAX];
  #define ZERO_W(led) led.w = 0  // Zero out W channel for RGBW mode
#else
  // RGB strips: Use CRGB arrays (3 bytes per LED)
  CRGB leds[NUM_CHANNELS_DEFAULT+1][NUM_LEDS_PER_CHANNEL_MAX];
  #define ZERO_W(led)  // Do nothing for RGB mode (no W channel)
#endif

// Config Data is conviently stored in a struct (to easy store and retrieve from EEPROM/Flash)
// Set defaults, they will be overwritten by load from EEPROM
struct LedData {
  char identifier[IDENTIFIER_MAX_LENGTH] = IDENTIFIER_DEFAULT;
  uint16_t             numLedsPerChannel = NUM_LEDS_PER_CHANNEL_DEFAULT;
  uint8_t                    numChannels = NUM_CHANNELS_DEFAULT;
  uint8_t            numGroupsPerChannel = NUM_GROUPS_PER_CHANNEL_DEFAULT;
  uint8_t                    spacerWidth = SPACER_WIDTH_DEFAULT;
  uint8_t                    startOffset = START_OFFSET;
  uint16_t                 blinkinterval = BLINK_INTERVAL;
  uint16_t               animateinterval = ANIMATE_INTERVAL;
  uint16_t                updateinterval = UPDATE_INTERVAL;
  uint16_t                    brightness = BRIGHTNESS;
  uint16_t               fadingAnimation = FADING;
  uint16_t                   fading2Step = FADING_2STEP;
  bool                  startupAnimation = STARTUP_ANIMATION;
  bool                       CommandEcho = COMMAND_ECHO;
  uint8_t                channelGPIOpin[8] = {2,3,4,5,6,7,8,9};
  uint8_t              state_pattern[12] = {0,0,0,0,0,1,1,1,1,0};
  CRGB                   state_color[10] = {CRGB::Black, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, CRGB::White};
  uint8_t               channelOrder[8] = {1,2,3,4,5,6,7,8};
} LedConfig = {};


// Input Data from serial is stores in an array for further processing.
char inputBuffer[MAX_INPUT_LEN + 1]; // +1 for null terminator
uint8_t bufferIndex = 0;


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

  // Enable watchdog timer (8 seconds timeout)
  // System will auto-reboot if watchdog is not fed within this time
  watchdog_enable(8000, 1);

  // Setup USB-serial port
  Serial.begin(115200);
  //Serial.setTimeout(0);

   // Initialize status led, set to blue to show we are waiting for input
  //
  // We have to disable theCPU led as Fastled can only drive 8 led channels ! (due to 8 PIO registers)
  // So we have to bitbang if we want to use it...
  pinMode(CPULED_GPIO, OUTPUT);
  gpio_put(CPULED_GPIO, 0);
  CPULED(0x00,0x00,0x00);

  // Enable GPIO 2-9 for pin test.
  for (int PIN=0; PIN<NUM_CHANNELS_MAX; PIN++) {
    pinMode(PIN+GPIO_PIN_MIN, OUTPUT);
  }

  while (!Serial) {
    // wait for serial port to connect.
    // Run a slow GPIO pintest while wating
    // Flash the status led green to indicate we are ready to start.

    for (int PIN=0; PIN<NUM_CHANNELS_MAX; PIN++) {
      CPULED(0x00,PIN*0x10,0x00);

      digitalWrite(PIN+GPIO_PIN_MIN, HIGH);
      delay(100);
      digitalWrite(PIN+GPIO_PIN_MIN, LOW);
    }
    delay(100);
    CPULED(0x00,0x00,0x00);
    delay(100);
  }

  // Set status led to blue to show we are busy
  CPULED(0x00,0x00,0x80);

  //   Reset all to low
  for (int PIN=2; PIN<=9; PIN++) {
    digitalWrite(PIN, LOW);
  }

  // Set status led to blue to show we are busy
  CPULED(0x00,0x00,0x00);
  CPULED(0x00,0x00,0x80);
  // Show welcome to let us know the controller is booting.
  // Also show some details about the MCU and the codeversion

  // Show version
  // Serial.print("\x1b[2J\x1b[H"); // Clear screen, cursor home
  Serial.println();
  Serial.print("-=[ Shelf Indicators for Guided Handling Tasks ]=-\n" );
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
    Serial.println("LittleFS mount failed!");

    // Attempt to format the filesystem
    Serial.println("Formatting LittleFS...");
    if (LittleFS.format()) {
      Serial.println("LittleFS formatting successful!");

      // Try to mount again after formatting
      if (LittleFS.begin()) {
        Serial.println("LittleFS mounted successfully after formatting.");
      } else {
        Serial.println("WARNING !!!\nLittleFS mount failed after formatting --> Load/Saving configuration not possible...\n" );
        // delay(2000);
        // rebootMCU();
      }
    } else {
        Serial.println("WARNING !!!\nLittleFS formatting failed --> Load/Saving configuration not possible...\n" );
      // delay(2000);
      // rebootMCU();
    }
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
  Serial.println("System ready - Status LED: Blue glow");

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
  // ShowHelp();
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
  // just count up here, the clipping happens in the UpdateLED function as the lenght varies on the effect and group-size.
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
  while (Serial.available() > 0) {

    char c = Serial.read();

    if (c == '\e' || c == 0x03 ) {
      Serial.println("\nCANCELLED");
      bufferIndex = 0;
      Serial.print("> ");
    }
    else
    if (c == '\n' || c == '\r') {
      // Only process if buffer has content (avoid double-processing \r\n pairs)
      if (bufferIndex > 0 || inputBuffer[0] != '\0') {
        inputBuffer[bufferIndex] = '\0'; // Null-terminate the string
        checkInput(inputBuffer);
        Serial.print("> ");
      }
      bufferIndex = 0;
      inputBuffer[0] = '\0'; // Clear buffer marker
    }
    else
    if (c == 0x08 && bufferIndex > 0) { // Backspace
      bufferIndex--;
      Serial.print(c);
      Serial.print(' ');
      Serial.print(c);
    }
    else
    if (isPrintable(c)) {
      if (bufferIndex < MAX_INPUT_LEN - 1) {
        inputBuffer[bufferIndex++] = c;
        Serial.print(c);
      } else {
        // Buffer full - reject character and warn user
        Serial.print('\a');  // Bell character
        if (bufferIndex == MAX_INPUT_LEN - 1) {
          Serial.println("\nERROR: Input too long! Max length is ");
          Serial.print(MAX_INPUT_LEN - 1);
          Serial.println(" characters.");
          Serial.print("> ");
          inputBuffer[bufferIndex] = '\0';
          bufferIndex = 0;
        }
      }
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
 * Parse and execute serial Commands
 * @param input Command string from serial input
 */
void checkInput(char input[MAX_INPUT_LEN]) {
  CPULED(0x00,0x00,0x80);

  // Move to new line after user input
  Serial.println();

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
    Serial.print("CMD> ");
    Serial.println(input);
  }

  // Increment Command counter
  CommandCount++;

  if (Command >= MIN_COMMAND_CHAR && Command <= MAX_COMMAND_CHAR) {
    switch(Command) {

      // Help Command
      case 'H':
      case '?':
        Serial.println("\n=== SIGHT Command Reference ===");
        Serial.println("V           - Show version information");
        Serial.println("H or ?      - Show this help");
        Serial.println("D           - Display current configuration");
        Serial.println("I           - Show system info (RAM/Flash usage)");
        Serial.println("S           - Save configuration to flash");
        Serial.println("Se          - Save/Export configuration as hex (backup)");
        Serial.println("L           - Load configuration from flash");
        Serial.println("Li:CONFIG:  - Load/Import configuration from hex (restore)");
        Serial.println("R           - Reboot controller");

        Serial.println("\nGroup Control:");
        Serial.println("T:n:s       - Set group n to state s (0-9)");
        Serial.println("M:ssssss    - Set multiple groups at once my providing a list of states (e.g. M:12345)");
        Serial.println("A:s         - Set all groups to state s (0-9)");
        Serial.println("Pgg:s:ppp   - Set group state (gg=group 1-48, s=state 0-9, ppp=percent 0-100)");
        Serial.println("X           - Clear all groups (set to state 0)");
        Serial.println("Q           - Quick status summary (active groups per state)");
        Serial.println("G           - Get detailed state of all groups");

        Serial.println("\nConfiguration (C prefix):");
        Serial.println("Cn:name     - Set controller name/identifier (max. 16 chars)");
        Serial.println("Cl:n        - Set LEDs per channel (6-300)");
        Serial.println("Cs:n        - Set number of active channels (1-8)");
        Serial.println("Ct:n        - Set groups per channel");
        Serial.println("Cw:n        - Set spacer width (0-20)");
        Serial.println("Co:n        - Set start offset (0-9)");
        Serial.println("Ca:n        - Set animate interval (10-1000 ms)");
        Serial.println("Cb:n        - Set blink interval (50-3000 ms)");
        Serial.println("Cu:n        - Set update interval (5-500 ms)");
        Serial.println("Ci:n        - Set brightness (10-255)");
        Serial.println("Cf:n        - Set fading factor (0-255) or Cf:anim,2step for separate Values");
        Serial.println("Cz:order    - Set channel order (N=standard 12345678, or custom like 43215678)");
        Serial.println("Cg:Y/N      - Enable/disable startup animation");
        Serial.println("Ce:Y/N      - Enable/disable Command echo");
        Serial.println("Cx:p1,p2... - Set GPIO pins for channels");
        Serial.println("Cp:s:n      - Set pattern for state s (0-9)");
        Serial.println("Cc:s:RRGGBB - Set color for state s (hex)");
        Serial.println("Cd          - Reset to default configuration");
        Serial.println("================================\n");
        break;

      // Version information
      case 'V':
        Serial.println("\n=== SIGHT Version Information ===");
        Serial.print("Version          : ");
        Serial.println(VERSION);
        Serial.print("Build Date       : ");
        Serial.println(__DATE__ " " __TIME__);
        Serial.print("LED Type       : ");
        #ifdef USE_RGBW_LEDS
          Serial.println("RGBW (4 bytes/LED)");
          Serial.println("Chipset          : SK6812");
          Serial.println("Color Order      : RGB");
        #else
          Serial.println("RGB (3 bytes/LED)");
          Serial.println("Chipset          : WS2812B");
          Serial.println("Color Order      : GRB");
        #endif
        Serial.print("MCU ID           : ");
        Serial.println(mcuId);
        Serial.println("=================================\n");
        break;

      // Display curren configuration
      case 'D':
        Serial.print("Display configuration:\n" );
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
          Serial.println("\n=== Configuration Export ===");
          Serial.print("CONFIG:");
          uint8_t* configBytes = (uint8_t*)&LedConfig;
          for (size_t i = 0; i < sizeof(LedConfig); i++) {
            if (configBytes[i] < 16) Serial.print("0");
            Serial.print(configBytes[i], HEX);
          }
          Serial.println();
          Serial.println("============================");
          Serial.println("Copy the CONFIG: line to backup this configuration.");
          Serial.println("Use 'Li:CONFIG:<hex>' to restore it.\n");
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
            size_t expectedLen = sizeof(LedConfig) * 2;

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
                  break;
                }
              }

              if (validHex) {
                uint8_t* configBytes = (uint8_t*)&LedConfig;

                for (size_t i = 0; i < sizeof(LedConfig); i++) {
                  char byteStr[3] = {hexData[i*2], hexData[i*2+1], '\0'};
                  configBytes[i] = (uint8_t)strtol(byteStr, NULL, 16);
                }

                Serial.println("Configuration imported successfully!");
                Serial.println("Use 'S' to save to flash, or 'R' to reboot and discard.");
              }
            } else {
              Serial.print("ERROR: Invalid hex length. Expected ");
              Serial.print(expectedLen);
              Serial.print(" chars, got ");
              Serial.println(hexLen);
            }
          } else {
            Serial.println("ERROR: Format must be Li:CONFIG:<hex_string>");
          }
        } else {
          // L - Load from flash
          Serial.print("Load configuration: " );
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
              Serial.println(output);
            } else {
              Serial.print("ERROR: Invalid state '");
              Serial.print(state);
              Serial.println("', use 0-9");
            }
          } else {
            Serial.print("ERROR: Invalid Group-ID '");
            Serial.print(groupID);
            Serial.println("', use 1-48");
          }
        } else
          Serial.print("Syntax error: Use T<Group-ID>:<STATE>\n" );
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
                Serial.println(output);
              } else {
                Serial.print("ERROR: Invalid percentage '");
                Serial.print(Pct);
                Serial.println("', use 0-100");
              }
            } else {
              Serial.print("ERROR: Invalid state '");
              Serial.print(state);
              Serial.println("', use 0-9");
            }
          } else {
            Serial.print("ERROR: Invalid Group-ID '");
            Serial.print(groupID);
            Serial.println("', use 1-48");
          }
        } else {
          Serial.print("Syntax error: Use Pgg:s:ppp (gg=group 1-48, s=state 0-9, ppp=percent 0-100)\n");
        }
        break;

      // Set state for all Group
      case 'A':
        test = sscanf(Data, ":%d", &state);
        if (test == 1) {
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
          }
        } else
          Serial.print("ERROR: Syntax error, use A:<STATE>\n" );
        break;

      // Quick status summary
      case 'Q':
        {
          Serial.println("\n=== Quick Status ===");

          // Count groups in each state
          int stateCounts[10] = {0};
          int activeGroups = 0;

          for (int i = 0; i < MAX_GROUPS; i++) {
            if (TermState[i] >= 0 && TermState[i] <= 9) {
              stateCounts[TermState[i]]++;
              if (TermState[i] > 0) activeGroups++;
            }
          }

          Serial.print("Active groups    : ");
          Serial.print(activeGroups);
          Serial.print(" / ");
          Serial.println(MAX_GROUPS);

          Serial.println("\nGroups per state:");
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
          Serial.println("====================\n");
        }
        break;

      // Get state for all Groups
      case 'G':
            Serial.println("\n=== Group states ===");

            for(int Index = 0; Index < LedConfig.numChannels; Index++) {
              // Use manual channel order mapping
              strip = LedConfig.channelOrder[Index] - 1;

              Serial.print("Channel ");
              Serial.print(strip + 1);
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
            Serial.print("WARNING: ");
            Serial.print(totalChars - MAX_GROUPS);
            Serial.print(" surplus Values ignored (max ");
            Serial.print(MAX_GROUPS);
            Serial.println(" groups)");
          }

          Serial.print("Set ");
          Serial.print(min(totalChars, MAX_GROUPS));
          Serial.println(" group states");
        } else
          Serial.print("Syntax error: Use M:<STATE><STATE<<STATE>...\n");
        break;

      // Reset all states to off
      case 'X':
        Serial.print("Reset all Group states.\n" );
        for(int groupID = 0; groupID < MAX_GROUPS; groupID++) {
          TermState[groupID] = 0;
          TermPct[groupID] = 100;
        }
        setAllLEDs(CRGB::Black);
        break;

      // Show startup loop
      case 'W':
        Serial.print("Showing startup loop.\n" );
        StartupLoop();
        break;

      // Memory usage report
      case 'I':
        {
          Serial.println("\n=== System Information ===");
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
          Serial.println("==========================\n");
        }
        break;

      // Reboot controller
      case 'R':
        Serial.print("Rebooting controller...\n" );
        rebootMCU();
        break;

      default:
        Serial.print("ERROR: Command '");
        Serial.print(Command);
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
  Serial.print  ("  T<groupID>:<state>             Set Group state. groupID: 1-");
  Serial.print  (MAX_GROUPS);
  Serial.println(" and state: 0-9");
  Serial.print  ("  P<groupID>:<state>:<Pct>       Set Group state. groupID: 1-");
  Serial.print  (MAX_GROUPS);
  Serial.println(", state: 0-9, PCt=0-100% progress");
  Serial.println("  M:<state><state>...           Set state for multiple Group, sequentially listed, e.g: '113110'");
  Serial.println("  A:<state>                     Set state for all Groups, state (0-9)");
  Serial.println("  G                             Get states for all Groups");
  Serial.println("  X                             Set all states to off (same as sending'A:0'");
  Serial.println();
  Serial.println("  H                             This help");
  Serial.println("  R                             Reboot controller (Disconnects from serial!)");
  Serial.println("  W                             Shows welcome/startup loop");
  Serial.println();
  Serial.println("  D                             Show current configuration");
  Serial.println("  Cn:<string>                   Set Controller name (ID) (1-16 chars)");
  Serial.print  ("  Cl:<Value>                    Set amount of LEDs per channel (");
  Serial.print  (NUM_LEDS_PER_CHANNEL_MIN);
  Serial.print  ("-");
  Serial.print  (NUM_LEDS_PER_CHANNEL_MAX);
  Serial.println(")");
  Serial.print  ("  Ct:<Value>                    Set amount of groups per channel (1-");
  Serial.print  (NUM_GROUPS_PER_CHANNEL_MAX);
  Serial.println(")");
  Serial.println("  Cs:<Value>                    Set amount of active channels (1-8)");
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
  Serial.println("  Cf:<Value>                    Set fading factor (0-255) or Cf:anim,2step for separate Values");
  Serial.println("  Cc:<state>:<Value>            Set color for state in HEX RGB order (state 1-9, Value: RRGGBB)");
  Serial.println("  Cp:<state>:<pattern>          Set display-pattern for state in (state: 0-9, pattern 0-9) [for colorblind assist]");
  Serial.println("  Cz:<order>                    Set channel order (N=standard 12345678, or custom like 43215678)");
  Serial.println("  C4:<yes/true/no/false>        Set RGBW leds (4bytes) instead of RGB (3bytes) (False/True)");
  Serial.print  ("  Cx:<channel>:<cpio-pin>        Set CPIO pin (");
  Serial.print  (GPIO_PIN_MIN);
  Serial.print  ("-");
  Serial.print  (GPIO_PIN_MAX);
  Serial.println(") per channel (1-8)");
  Serial.println("  Cd:                           Reset all settings to factory defaults");
  Serial.println();
  Serial.println("  L                             Load stored configuration from EEPROM/FLASH");
  Serial.println("  S                             Save configuration to EEPROM/FLASH");
  Serial.println("\n");
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

  // Clear entire group first to prevent color overlap when state/percentage changes
  if (pattern < 8) {
    int fullgroupWidth = (LedConfig.numLedsPerChannel / LedConfig.numGroupsPerChannel) - LedConfig.spacerWidth;
    for(int i = 0; i < fullgroupWidth; i++) {
      leds[channelIndex][startLEDIndex + i] = CRGB::Black;
      ZERO_W(leds[channelIndex][startLEDIndex + i]);
    }
  }

  // for percentage
  if (Pct<100) {
    pattern=0;  // Force pattern 0 (solid)
    float factor = float(Pct)/100;
    groupWidth = round(float(groupWidth) * factor);
    if (groupWidth == 0 && Pct>0)
      groupWidth = 1;  // only no leds on 0%
  }

  switch (pattern) {
      case 0: animate_Step[pattern]=animate_Step[pattern]%1;
              // 0 solid no blink    [########]
              //                     [########]
              for(int i = 0; i < groupWidth; i++) {
                leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state];
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              break;

      case 1:	animate_Step[pattern]=animate_Step[pattern]%2;
              // 1 solid blink       [########]
              //                     [        ]
              for(int i = 0; i < groupWidth; i++) {
                  leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state]*blinkState;
                  leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                  ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              break;

      case 2:	animate_Step[pattern]=animate_Step[pattern]%2;
              // 1 solid blink inv.  [        ]
              //                     [########]
              for(int i = 0; i < groupWidth; i++) {
                  leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state]*!blinkState;
                  leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                  ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              break;

      case 3: animate_Step[pattern]=animate_Step[pattern]%2;
              // 2 Alternate L/R     [####    ] Count up c=0->1  s,s+(n/2) *c
              //                     [    ####]                  (n/2),n   *!c
              for(int i = 0; i < (groupWidth/2); i++) {
                  leds[channelIndex][startLEDIndex + i + ( blinkState * (groupWidth/2)) ] = LedConfig.state_color[state];
                  leds[channelIndex][startLEDIndex + i + ( blinkState * (groupWidth/2)) ].fadeLightBy(LedConfig.fading2Step);
                  leds[channelIndex][startLEDIndex + i + (!blinkState * (groupWidth/2)) ] = 0;
                  ZERO_W(leds[channelIndex][startLEDIndex + i + ( blinkState * (groupWidth/2))]);
                  ZERO_W(leds[channelIndex][startLEDIndex + i + (!blinkState * (groupWidth/2))]);
              }
              break;

      case 4: animate_Step[pattern]=animate_Step[pattern]%2;
              // 3 Alternate in/out  [##      ##] Count up c=0->1  s,s+(n/4) + n-(n/4),n
              //                     [  ##  ##  ]                  s+(n/4)-(n/2)+(n/4)
              for(int i = 0; i < (groupWidth); i++) {
                if (i < (groupWidth/4) or i >= (groupWidth-(groupWidth/4)) )
                  leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state] * blinkState;
                else
                  leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state] * !blinkState;
                leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              break;

      case 5: animate_Step[pattern]=animate_Step[pattern]%groupWidth;
              // 4 odd/even          [# # # # ]
              //                     [ # # # #]
              for(int i = 0; i < (groupWidth); i++) {
                  if (i%2)
                    leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state] * blinkState;
                  else
                    leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state] * !blinkState;
                  leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                  ZERO_W(leds[channelIndex][startLEDIndex + i]);
              }
              break;

      case 6: animate_Step[pattern]=animate_Step[pattern]%1;
              // 10 1/3 gated blink    [###  ###]
              for(int i = 0; i < groupWidth; i++) {
                if ( i <= (groupWidth/3) or i >= (groupWidth-(groupWidth/3)-1) ) {
                  leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state];
                  leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                  ZERO_W(leds[channelIndex][startLEDIndex + i]);
                }
              }
              break;
      case 7: animate_Step[pattern]=animate_Step[pattern]%2;
              // gated blink    [###  ###]
              //                [        ]
              for(int i = 0; i < groupWidth; i++) {
                if ( i <= (groupWidth/3) or i >= (groupWidth-(groupWidth/3)-1) ) {
                  leds[channelIndex][startLEDIndex + i] = LedConfig.state_color[state] * blinkState;
                  leds[channelIndex][startLEDIndex + i].fadeLightBy(LedConfig.fading2Step);
                  ZERO_W(leds[channelIndex][startLEDIndex + i]);
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
      ledColor = CRGB::Green;
      // Pulsing green effect
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
      ledColor = CRGB::Blue;
      // Slow glowing blue effect
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
        cpuLedBrightness = (cpuLedBrightness == 0) ? CPULED_STATUS_BRIGHTNESS : 0;
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
          Serial.print("Controller Name (ID)   : " );
          Serial.println(LedConfig.identifier);
        } else {
          Serial.println("Identifier too long, use 16 characters max.");
        }
        break;
      // set led per channel
      case 'l':
        ValueInt = atoi(Value);
        if (ValueInt >= NUM_LEDS_PER_CHANNEL_MIN and ValueInt <= NUM_LEDS_PER_CHANNEL_MAX) {
          Serial.print("LEDs per channel      : " );
          LedConfig.numLedsPerChannel = ValueInt;
          Serial.println(LedConfig.numLedsPerChannel);
          FastLED.clearData();
        } else {
          Serial.print("Invalid number of leds per channel(");
          Serial.print(NUM_LEDS_PER_CHANNEL_MIN);
          Serial.print("-");
          Serial.print(NUM_LEDS_PER_CHANNEL_MAX);
          Serial.println(")");
        }
        break;
      // set groups per channel
      case 't':
        ValueInt = atoi(Value);
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
        }
        break;
      // set number of channels
      case 's':
        ValueInt = atoi(Value);
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

        }
        break;
      // set spacer width
      case 'w':
        ValueInt = atoi(Value);
        if (ValueInt >= 0 and ValueInt <= SPACER_WIDTH_MAX) {
          Serial.print("Spacer width         : " );
          LedConfig.spacerWidth = ValueInt;
          Serial.println(LedConfig.spacerWidth);
          FastLED.clearData();
        } else {
          Serial.print("Invalid space width(0-");
          Serial.print(SPACER_WIDTH_MAX);
          Serial.println(")");
        }
        break;
      case 'o':
        ValueInt = atoi(Value);
        if (ValueInt >= 0 and ValueInt <= START_OFFSET_MAX) {
          Serial.print("Start offset         : " );
          LedConfig.startOffset = ValueInt;
          Serial.println(LedConfig.startOffset);
          FastLED.clearData();
        } else {
          Serial.print("Invalid start offset (0-");
          Serial.print(START_OFFSET_MAX);
          Serial.println(")");
        }
        break;
      // Set animate interval
      case 'a':
        ValueInt = atoi(Value);
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
        }
        break;
      // Set blink interval
      case 'b':
        ValueInt = atoi(Value);
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
          }
        } else {
          Serial.print("Invalid blink interval (");
          Serial.print(BLINK_INTERVAL_MIN);
          Serial.print("-");
          Serial.print(BLINK_INTERVAL_MAX);
          Serial.println(" msec)");
        }
          break;

      // Set update interval
      case 'u':
        ValueInt = atoi(Value);
        if (ValueInt >= UPDATE_INTERVAL_MIN and ValueInt <= UPDATE_INTERVAL_MAX) {
          if (ValueInt < LedConfig.blinkinterval) {
            Serial.print("Update interval      : ");
            LedConfig.updateinterval = ValueInt;
            Serial.println(LedConfig.updateinterval);
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
            Serial.print("Invalid update-interval, needs to be smaller than current blink-interval (");
            Serial.print(LedConfig.blinkinterval);
            Serial.println(")");
          }
        } else {
          Serial.print("Invalid update interval (");
          Serial.print(UPDATE_INTERVAL_MIN);
          Serial.print("-");
          Serial.print(UPDATE_INTERVAL_MAX);
          Serial.println(" msec)");
        }
        break;

      // Set Brightness Inetensity
      case 'i':
        ValueInt = atoi(Value);
        if (ValueInt >= BRIGHTNESS_MIN and ValueInt <= BRIGHTNESS_MAX) {
          Serial.print("Brightness intensity   : " );
          LedConfig.brightness = ValueInt;
          Serial.println(LedConfig.brightness);
          FastLED.setBrightness(LedConfig.brightness);
          FastLED.clearData();

          // Warn if brightness is very high
          if (ValueInt > BRIGHTNESS_WARNING_THRESHOLD) {
            Serial.println("WARNING: High brightness may cause overheating or exceed power supply capacity!");
          }
        } else {
          Serial.print("Invalid brightness intensity (");
          Serial.print(BRIGHTNESS_MIN);
          Serial.print("-");
          Serial.print(BRIGHTNESS_MAX);
          Serial.println(")");
        }
        break;

      // Set fade factor
      case 'f':
        // Check if two Values are provided (animation-fade,2step-fade)
        comma = strchr(Value, ',');
        if (comma != NULL) {
          // Two Values provided
          *comma = '\0'; // Split the string
          int animation_fade = atoi(Value);
          int twostep_fade = atoi(comma + 1);

          if (animation_fade >= 0 && animation_fade <= 255 && twostep_fade >= 0 && twostep_fade <= 255) {
            LedConfig.fadingAnimation = animation_fade;
            LedConfig.fading2Step = twostep_fade;
            Serial.print("Animation fading factor: ");
            Serial.print(LedConfig.fadingAnimation);
            Serial.print(", 2-step fading factor: ");
            Serial.println(LedConfig.fading2Step);
            FastLED.clearData();
          } else {
            Serial.println("Invalid fade Values (0-255 for both Values)");
          }
        } else {
          // Single Value provided (backward compatibility)
          ValueInt = atoi(Value);
          if (ValueInt >= 0 && ValueInt <= 255) {
            Serial.print("Fading factor          : ");
            LedConfig.fadingAnimation = ValueInt;
            LedConfig.fading2Step = ValueInt / 2; // Default 2-step fade is half of animation fade
            Serial.print(LedConfig.fadingAnimation);
            Serial.print(" (2-step fade: ");
            Serial.print(LedConfig.fading2Step);
            Serial.println(")");
            FastLED.clearData();
          } else {
            Serial.println("Invalid fade Value (0-255)");
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
        }
        break;

      // Toggle Command echo
      case 'e':
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.CommandEcho = false;
          Serial.println("Command echo         : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.CommandEcho = true;
          Serial.println("Command echo         : Enabled");
        } else {
          Serial.println("Invalid Value, use Y/N or 1/0");
        }
        break;
      // Set color-patern (colorblind assist)
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
        Serial.println("Configuration reset to defaults\n");
        FastLED.clearData();
        break;
      default:
        Serial.print("SYNTAX ERROR: Configuration item '");
        Serial.print(configItem);
        Serial.println("' unknown. Use H for help.");
        break;
    }
  } else {
    Serial.print("SYNTAX ERROR: Invalid configuration format. Use C<item>:<value> format, H for help.\n");
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
            Serial.print("ERROR: GPIO-PIN ");
            Serial.print(GPIO_PIN);
            Serial.print(" is already used for channel ");
            Serial.print(CHANNEL);
            Serial.print(" !");
            return;
          }
        }
        if (GPIO_PIN == CPULED_GPIO) {
          Serial.print("ERROR: GPIO-PIN ");
          Serial.print(GPIO_PIN);
          Serial.print(" is already used for CPULED !");
          return;
        }

        LedConfig.channelGPIOpin[channel] = GPIO_PIN;
        Serial.print("GPIO-PIN for channel ");
        Serial.print(channel);
        Serial.print(" is set to : ");
        Serial.print(LedConfig.channelGPIOpin[channel]);
        Serial.println();
        Serial.println("Please note a MCU reboot is required to activate a change in GPIO pin assignments");
      } else {
        Serial.println("Invalid GPIO-PIN number.");
      }
    } else {
      Serial.print  ("Invalid channel, 1-");
      Serial.print  (NUM_CHANNELS_MAX);
      Serial.println(" only.");
    }
  } else {
      Serial.println("Syntax error: use Cx:<channel>:<GPIO-PIN>     (<channel: 1-");
      Serial.print  (NUM_CHANNELS_MAX);
      Serial.print  (", <GPIO-PIN>: ");
      Serial.print  (GPIO_PIN_MIN);
      Serial.print  ("-");
      Serial.print  (GPIO_PIN_MAX);
      Serial.println(")\n");
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
        Serial.print("Color for state ");
        Serial.print(state);
        Serial.print(" is set to : ");
        snprintf(buffer, strlen(buffer), "%02X%02X%02X", LedConfig.state_color[state].red, LedConfig.state_color[state].green, LedConfig.state_color[state].blue);
        Serial.print(buffer);
        Serial.println(" (RR GG BB)");
      } else {
        Serial.println("Invalid color");
      }
    } else {
      Serial.println("Invalid state, 1-9 only");
    }
  } else {
    Serial.println("Syntax error: use Cc:<STATE>:<BBGGRR>   (<state>: 1-9, <BBGGRR>: Color in Hex))\n");
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
        Serial.print("Pattern for state ");
        Serial.print(state);
        Serial.print(" is set to : ");
        Serial.print(LedConfig.state_pattern[state]);
        Serial.println();
      } else {
        Serial.println("Invalid pattern");
      }
    } else {
      Serial.println("Invalid state, 1-12 only");
    }
  } else {
    Serial.println("Syntax error: use Cp:<state>:<pattern>     (<state>: 0-9, <pattern>: 0-9)\n");
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
  LedConfig.fading2Step = FADING_2STEP;
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

  Serial.print("Identifier           : ");
  Serial.println(LedConfig.identifier);

  Serial.print("LEDs per channel      : ");
  Serial.println(LedConfig.numLedsPerChannel);

  Serial.print("Groups per channel    : ");
  Serial.println(LedConfig.numGroupsPerChannel);

  Serial.print("Amount of channels    : ");
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

  Serial.print("2-step fading        : ");
  Serial.println(LedConfig.fading2Step);

  Serial.print("Channel order        : ");
  for (int i = 0; i < 8; i++) {
    Serial.print(LedConfig.channelOrder[i]);
  }
  Serial.println();

  Serial.println("LED Mode             : RGB-only (W channel = 0)");

  Serial.print("Overall brightness   : ");
  Serial.println(LedConfig.brightness);

  Serial.println();
  Serial.print("Channel               : | ");
  for (uint8_t channel=0; channel<NUM_CHANNELS_DEFAULT; channel++) {
    sprintf(output, "%2d | ", channel+1);
    Serial.print(output);
  }
  Serial.println();
  Serial.print("GPIO-PIN             : | ");
  for (uint8_t channel=0; channel<NUM_CHANNELS_DEFAULT; channel++) {
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
char* readFile(const char * path) {
  CPULED(0x00,0x00,0x80);
  File fileH = LittleFS.open(F(path), "r");
  if (!fileH) {
    Serial.print("NOTE: Failed opening confgfile\n" );
    return 0;
  }

  long fileSize=fileH.size();
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
    Serial.print("* Opening Failed\n" );
    return false;
  }
  size_t bytesWritten = fileH.write((const uint8_t*)Data, DataSize);
  if (bytesWritten != DataSize) {
    Serial.print("* Write Failed\n");
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

    // Calulate sizes to separate struct and checksum in buffer
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
      // DeSeriallize buffer into LedData Struct
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
        Serial.println("WARNING: numChannels * numGroupsPerChannel exceeds MAX_GROUPS!");
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

      Serial.println("Checksum matches, configuration loaded.");
      if (needsCorrection) {
        Serial.println("WARNING: Some Values were out of range and corrected to defaults.");
        Serial.println("Use 'S' to save corrected configuration.\n");
      } else {
        Serial.println();
      }
      return true;
    } else {
      Serial.println("Checksum mismatch, configuration is corrupted!\n");
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
  // Seriallize LedData Struct into char-array so we can save it
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
    sendByte_CPULED((color & 0x0000FF00) >> 8);  // Send green byte
    sendByte_CPULED((color & 0x00FF0000) >> 16);  // Send red byte
    sendByte_CPULED((color & 0x000000FF));        // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}
/**
 * Set CPU LED color using CRGB color object
 * @param color CRGB color object
 */
void CPULED(CRGB color) {
    sendByte_CPULED(color.g);  // Send green byte
    sendByte_CPULED(color.r);  // Send red byte
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
    sendByte_CPULED(g);  // Send green byte
    sendByte_CPULED(r);  // Send red byte
    sendByte_CPULED(b);  // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}
