/*
Name        : SIGHT (Shelf Indicators for Guided Handling Tasks)
Version     : 1.8
Date        : 2025-11-25
Author      : Bas van Ritbergen <bas.vanritbergen@adyen.com> / bas@ritbit.com
Description : LED strip controller with animations, RGBW support, and comprehensive safety features.
              
              v1.8 improvements:
              - Added comprehensive config validation on load and runtime
              - Added watchdog timer for system stability
              - Added command echo, version info, help, and system info commands
              - Added config backup/restore via hex export/import (Se/Li commands)
              - Added statistics tracking (uptime, command count, error count)
              - Added quick status summary (Q command) and improved group state display (G command)
              - Added brightness safety limits and warnings
              - Added buffer overflow protection and input validation
              - Improved error messages with actual values shown
              - Added function documentation and const correctness
              - Fixed group LED clearing to prevent color overlap
              
Notes       : When compiling make sure to reserve a little space for littleFS (8-64k)
              Supports both RGB (WS2812B) and RGBW (WS2813B/SK6812) LED strips.
              Switch between modes using USE_RGB_STRIPS or USE_RGBW_STRIPS define.	

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


// Basic defaults 
#define CONFIG_IDENTIFIER "SIGHT-CONFIG"
#define IDENTIFIER_DEFAULT "SIGHT v1.8"
#define IDENTIFIER_MAX_LENGTH 16

#define NUM_LEDS_PER_STRIP_DEFAULT 57
#define NUM_LEDS_PER_STRIP_MIN 6
#define NUM_LEDS_PER_STRIP_MAX 300

#define NUM_STRIPS_DEFAULT 8
#define NUM_STRIPS_MAX 8

#define NUM_GROUPS_PER_STRIP_DEFAULT 6
#define NUM_GROUPS_PER_STRIP_MAX 16

#define SPACER_WIDTH_DEFAULT 1
#define SPACER_WIDTH_MAX 20

#define START_OFFSET 1
#define START_OFFSET_MAX 9

#define MAX_GROUPS 48

#define BLINK_INTERVAL 200
#define BLINK_INTERVAL_MIN 50
#define BLINK_INTERVAL_MAX 3000

#define ANIMATE_INTERVAL 150
#define ANIMATE_INTERVAL_MIN 10
#define ANIMATE_INTERVAL_MAX 1000

#define SET_GROUPSTATE_INTERVAL 200
#define SET_GROUPSTATE_INTERVAL_MIN 50
#define SET_GROUPSTATE_INTERVAL_MAX 1000

#define UPDATE_INTERVAL 25
#define UPDATE_INTERVAL_MIN 5
#define UPDATE_INTERVAL_MAX 500

#define COLOR_STATE_1 CRGB::Green
#define COLOR_STATE_2 CRGB::DarkOrange
#define COLOR_STATE_3 CRGB::Red
#define COLOR_STATE_4 CRGB::Blue
#define COLOR_STATE_5 CRGB::Green
#define COLOR_STATE_6 CRGB::DarkOrange
#define COLOR_STATE_7 CRGB::Red
#define COLOR_STATE_8 CRGB::Blue
#define COLOR_STATE_9 CRGB::White

#define ALTERNATIVE_SHELFORDERING false
#define STARTUP_ANIMATION true
#define COMMAND_ECHO false  // Set to true to echo received commands
#define BRIGHTNESS 255
#define BRIGHTNESS_MIN 10   // Minimum brightness (prevent invisible LEDs)
#define BRIGHTNESS_MAX 255  // Maximum brightness (prevent overheating/overcurrent)
#define BRIGHTNESS_WARNING_THRESHOLD 200  // Warn user above this brightness level
#define FADING 48

// *** IMPORTANT: Set this to match your LED strip type ***
// Uncomment ONE of these lines:
#define USE_RGB_STRIPS    // For WS2812B RGB strips (3 bytes per LED)
// #define USE_RGBW_STRIPS  // For WS2813B-RGBW, SK6812 RGBW strips (4 bytes per LED)

// Auto-configure based on strip type
#if defined(USE_RGB_STRIPS) && defined(USE_RGBW_STRIPS)
  #error "ERROR: Both USE_RGB_STRIPS and USE_RGBW_STRIPS are defined! Uncomment only ONE."
#endif

#ifdef USE_RGBW_STRIPS
  #define STRIP_TYPE 4
  #define LED_CHIPSET SK6812
  #define LED_COLOR_ORDER RGB
  #pragma message "Compiling for RGBW strips (SK6812, 4 bytes/LED, RGB order)"
#else
  #define STRIP_TYPE 3
  #define LED_CHIPSET WS2812B
  #define LED_COLOR_ORDER GRB
  #pragma message "Compiling for RGB strips (WS2812B, 3 bytes/LED, GRB order)"
#endif

#define GPIO_PIN_MIN 2
#define GPIO_PIN_MAX 26

// Define the pin connected to WS2812B
#define CPULED_GPIO 16

// Where do we store the configuration
#define CONFIG_FILENAME "/config.bin"

// ###########################################################################
// No configurable items below

// This Version
#define VERSION "1.8"

// Define max length for Input/Ouput buffers:
#define MAX_INPUT_LEN 256  // Increased to support Li config import (~180 chars)
#define MAX_OUTPUT_LEN 60

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
char MCUid [41];

// Declare LedStrip control arrays
#if STRIP_TYPE == 4
  // RGBW strips: Use CRGBW arrays (4 bytes per LED)
  CRGBW leds[NUM_STRIPS_DEFAULT+1][NUM_LEDS_PER_STRIP_MAX];
  #define ZERO_W(led) led.w = 0  // Zero out W channel for RGBW mode
#else
  // RGB strips: Use CRGB arrays (3 bytes per LED)
  CRGB leds[NUM_STRIPS_DEFAULT+1][NUM_LEDS_PER_STRIP_MAX];
  #define ZERO_W(led)  // Do nothing for RGB mode (no W channel)
#endif

// Config data is conviently stored in a struct (to easy store and retrieve from EEPROM/Flash)
// Set defaults, they will be overwritten by load from EEPROM
struct LedData {
  char identifier[IDENTIFIER_MAX_LENGTH] = IDENTIFIER_DEFAULT;
  uint16_t               numLedsPerStrip = NUM_LEDS_PER_STRIP_DEFAULT;
  uint8_t                      numStrips = NUM_STRIPS_DEFAULT;
  uint8_t              numGroupsPerStrip = NUM_GROUPS_PER_STRIP_DEFAULT;
  uint8_t                    spacerWidth = SPACER_WIDTH_DEFAULT;
  uint8_t                    startOffset = START_OFFSET;
  uint16_t                 blinkinterval = BLINK_INTERVAL;
  uint16_t               animateinterval = ANIMATE_INTERVAL;
  uint16_t                updateinterval = UPDATE_INTERVAL;
  uint16_t                    brightness = BRIGHTNESS;
  uint16_t                        fading = FADING;
  bool                     altShelfOrder = ALTERNATIVE_SHELFORDERING;
  bool                  startupAnimation = STARTUP_ANIMATION;
  bool                      commandEcho = COMMAND_ECHO;
  uint8_t                stripGPIOpin[8] = {2,3,4,5,6,7,8,9};
  uint8_t              state_pattern[12] = {0,0,0,0,0,1,1,1,1,0};
  CRGB                   state_color[10] = {CRGB::Black, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, CRGB::White};
} LedConfig = {};


// Input data from serial is stores in an array for further processing.
char inputBuffer[MAX_INPUT_LEN + 1]; // +1 for null terminator
uint8_t bufferIndex = 0;


// For color/blinking-state feature we need some extra global parameters
uint8_t  TermState[MAX_GROUPS] = {0};
uint8_t    TermPct[MAX_GROUPS] = {0};
volatile bool blinkState = 0;
uint32_t lastToggleTimes = 0;

// for LEDstrip update frequency
int      updateinterval = 100;
uint32_t lastLedUpdate=0;

// for status update
uint32_t lastStateUpdate;

// Statistics tracking
uint32_t bootTime = 0;
uint32_t commandCount = 0;
uint32_t errorCount = 0;

// Forward declarations
void checkInput(char input[MAX_INPUT_LEN]);

// Flags for updates
volatile bool LedstripUpdate = false;
volatile bool SetGroupStateFlag = false;

// Timer objects for pico_sdk
Ticker update_Timer;
Ticker blink_Timer;
Ticker setgroup_Timer;
Ticker animate_Timer;
uint8_t animate_Step[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// ##########################################################################################################

// Setup MCU
void setup() {

  // Enable watchdog timer (8 seconds timeout)
  // System will auto-reboot if watchdog is not fed within this time
  watchdog_enable(8000, 1);

  // Setup USB-serial port
  Serial.begin(115200);
  //Serial.setTimeout(0);

   // Initialize status led, set to blue to show we are waiting for input
  //
  // We have to disable theCPU led as Fastled can only drive 8 led outputs ! (due to 8 PIO registers)
  // So we have to bitbang if we want to use it...
  pinMode(CPULED_GPIO, OUTPUT); 
  gpio_put(CPULED_GPIO, 0);
  CPULED(0x00,0x00,0x00);

  // Enable GPIO 2-9 for pin test.
  for (int PIN=0; PIN<NUM_STRIPS_MAX; PIN++){
    pinMode(PIN+GPIO_PIN_MIN, OUTPUT);
  }

  while (!Serial) {
    // wait for serial port to connect.
    // Run a slow GPIO pintest while wating
    // Flash the status led green to indicate we are ready to start.

    for (int PIN=0; PIN<NUM_STRIPS_MAX; PIN++){
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
  for (int PIN=2; PIN<=9; PIN++){
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
  MicroID.getUniqueIDString(MCUid,8);
  Serial.println(MCUid);

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
        // RebootMCU();
      }
    } else {
        Serial.println("WARNING !!!\nLittleFS formatting failed --> Load/Saving configuration not possible...\n" );
      // delay(2000);
      // RebootMCU();
    }
  } else {
    Serial.println("LittleFS mounted successfully.");
    // Load or set defaults
    if (loadConfiguration() == false) {
      Serial.println("Setting default configuration");
      resetToDefaults();
    }
  }

  // Calculate buffer size for FastLED
  int MAX_LEDS;
  #if STRIP_TYPE == 4
    // RGBW: We use CRGBW arrays (4 bytes per LED) for proper alignment
    // FastLED thinks they're RGB (3 bytes), so we tell it about more "RGB LEDs"
    // to cover all 4 bytes per physical LED (W channel always set to 0)
    MAX_LEDS = (LedConfig.numLedsPerStrip * 4 + 2) / 3;  // +2 for rounding up
  #else
    // RGB: Standard 3 bytes per LED, 1:1 mapping
    MAX_LEDS = LedConfig.numLedsPerStrip;
  #endif
  
  for (uint8_t STRIP=0; STRIP<NUM_STRIPS_DEFAULT ; STRIP++) {
    // Serial.print("Strip: ");
    // Serial.print(STRIP);
    pinMode(LedConfig.stripGPIOpin[STRIP], OUTPUT);
    if (LedConfig.stripGPIOpin[STRIP] != CPULED_GPIO){
      // Serial.print(" -> GPIO: ");
      // Serial.print(LedConfig.stripGPIOpin[STRIP]);
      // Serial.println();
      #if STRIP_TYPE == 4
        // RGBW mode: Cast CRGBW* to CRGB* to trick FastLED into sending 4 bytes per LED
        switch (LedConfig.stripGPIOpin[STRIP]) {
          case  0: FastLED.addLeds<LED_CHIPSET,  0, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  1: FastLED.addLeds<LED_CHIPSET,  1, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  2: FastLED.addLeds<LED_CHIPSET,  2, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  3: FastLED.addLeds<LED_CHIPSET,  3, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  4: FastLED.addLeds<LED_CHIPSET,  4, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  5: FastLED.addLeds<LED_CHIPSET,  5, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  6: FastLED.addLeds<LED_CHIPSET,  6, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  7: FastLED.addLeds<LED_CHIPSET,  7, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  8: FastLED.addLeds<LED_CHIPSET,  8, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case  9: FastLED.addLeds<LED_CHIPSET,  9, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 10: FastLED.addLeds<LED_CHIPSET, 10, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 11: FastLED.addLeds<LED_CHIPSET, 11, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 12: FastLED.addLeds<LED_CHIPSET, 12, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 13: FastLED.addLeds<LED_CHIPSET, 13, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 14: FastLED.addLeds<LED_CHIPSET, 14, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 15: FastLED.addLeds<LED_CHIPSET, 15, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 17: FastLED.addLeds<LED_CHIPSET, 17, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 18: FastLED.addLeds<LED_CHIPSET, 18, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 19: FastLED.addLeds<LED_CHIPSET, 19, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 20: FastLED.addLeds<LED_CHIPSET, 20, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 21: FastLED.addLeds<LED_CHIPSET, 21, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 22: FastLED.addLeds<LED_CHIPSET, 22, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 23: FastLED.addLeds<LED_CHIPSET, 23, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 24: FastLED.addLeds<LED_CHIPSET, 24, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 25: FastLED.addLeds<LED_CHIPSET, 25, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 26: FastLED.addLeds<LED_CHIPSET, 26, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 27: FastLED.addLeds<LED_CHIPSET, 27, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 28: FastLED.addLeds<LED_CHIPSET, 28, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
          case 29: FastLED.addLeds<LED_CHIPSET, 29, LED_COLOR_ORDER>((CRGB*)leds[STRIP], MAX_LEDS); break;
        }
      #else
        // RGB mode: Use leds directly (already CRGB*), no cast needed
        switch (LedConfig.stripGPIOpin[STRIP]) {
          case  0: FastLED.addLeds<LED_CHIPSET,  0, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  1: FastLED.addLeds<LED_CHIPSET,  1, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  2: FastLED.addLeds<LED_CHIPSET,  2, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  3: FastLED.addLeds<LED_CHIPSET,  3, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  4: FastLED.addLeds<LED_CHIPSET,  4, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  5: FastLED.addLeds<LED_CHIPSET,  5, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  6: FastLED.addLeds<LED_CHIPSET,  6, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  7: FastLED.addLeds<LED_CHIPSET,  7, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  8: FastLED.addLeds<LED_CHIPSET,  8, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case  9: FastLED.addLeds<LED_CHIPSET,  9, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 10: FastLED.addLeds<LED_CHIPSET, 10, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 11: FastLED.addLeds<LED_CHIPSET, 11, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 12: FastLED.addLeds<LED_CHIPSET, 12, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 13: FastLED.addLeds<LED_CHIPSET, 13, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 14: FastLED.addLeds<LED_CHIPSET, 14, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 15: FastLED.addLeds<LED_CHIPSET, 15, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 17: FastLED.addLeds<LED_CHIPSET, 17, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 18: FastLED.addLeds<LED_CHIPSET, 18, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 19: FastLED.addLeds<LED_CHIPSET, 19, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 20: FastLED.addLeds<LED_CHIPSET, 20, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 21: FastLED.addLeds<LED_CHIPSET, 21, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 22: FastLED.addLeds<LED_CHIPSET, 22, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 23: FastLED.addLeds<LED_CHIPSET, 23, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 24: FastLED.addLeds<LED_CHIPSET, 24, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 25: FastLED.addLeds<LED_CHIPSET, 25, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 26: FastLED.addLeds<LED_CHIPSET, 26, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 27: FastLED.addLeds<LED_CHIPSET, 27, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 28: FastLED.addLeds<LED_CHIPSET, 28, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
          case 29: FastLED.addLeds<LED_CHIPSET, 29, LED_COLOR_ORDER>(leds[STRIP], MAX_LEDS); break;
        }
      #endif
    }
  }
 
  Serial.println("Initialization done..,");
  
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
  update_Timer.attach_ms(LedConfig.updateinterval,&WriteLedstripData); 
  blink_Timer.attach_ms(LedConfig.blinkinterval, &SetblinkState); 
  animate_Timer.attach_ms(LedConfig.animateinterval, &AnimateStep); 
  setgroup_Timer.attach_ms(LedConfig.updateinterval*2,&SetGroupState); 

  // Show help & config:
  // ShowHelp();
  ShowConfiguration() ;

  Serial.println("Enter 'H' for help ");
  Serial.println();

  // Initialize boot time for uptime tracking
  bootTime = millis();

  // Ready to go, show prompt to show we are ready for input
  Serial.print("> ");
}
 
void WriteLedstripData(){
  LedstripUpdate = true;
  return;     
}

void SetblinkState(){
  blinkState = !blinkState;
  (blinkState == true)?CPULED(0x40,0x00,0x00):CPULED(0x00,0x00,0x00); 
  return;     
}

void AnimateStep(){
  // just count up here, the clipping happens in the UpdateLED function as the lenght varies on the effect and group-size.
  for (int i=0; i<sizeof(animate_Step); i++){
    animate_Step[i]++;
  }
  return;     
}

void SetGroupState(){
  SetGroupStateFlag = true;
  return;     
}

// ##########################################################################################################

// Main loop
void loop() {
  // Feed the watchdog to prevent timeout
  watchdog_update();
  
  if (Serial.available() > 0) {
    handleSerialInput();
  }
  
  //updateLEDs();
  if (SetGroupStateFlag) {
     FastLED.show();
     SetGroupStateFlag=false;
  }

  if (LedstripUpdate) {
    LedstripUpdate=false;
    updateGroups();
  }
}

/**
 * Handle serial input character by character
 * Supports backspace, cancel (ESC/Ctrl+C), and command execution on newline
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
      inputBuffer[bufferIndex] = '\0'; // Null-terminate the string
      Serial.println();
      checkInput(inputBuffer);
      Serial.print("> ");
      bufferIndex = 0;
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
 */
bool isPrintable(const char c) {
  return c >= 0x20 && c < 0x7E && c != 0x5C && c != 0x60;
}

/**
 * Parse and execute serial commands
 * @param input Command string from serial input
 */
void checkInput(char input[MAX_INPUT_LEN]) {
  CPULED(0x00,0x00,0x80);
  if (input[0] == 0) {
    Serial.println("");
    return;
  }

  char output[MAX_OUTPUT_LEN];
  memset(output, '\0', sizeof(output));

  int test=0;
  int State;
  int Pct;
  int GroupID;
  int Strip;
  char Command = input[0];
  char *Data = input +1; 

  // Echo command if enabled (useful for debugging/logging)
  if (LedConfig.commandEcho) {
    Serial.print("CMD> ");
    Serial.println(input);
  }

  // Increment command counter
  commandCount++;

  if (Command >= 'A' && Command <= 'Z') {
    switch(Command) {

      // Help command
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
        Serial.println("Cn:name     - Set controller name/identifier (max 16 chars)");
        Serial.println("Cl:n        - Set LEDs per strip (6-300)");
        Serial.println("Cs:n        - Set number of strips/shelves (1-8)");
        Serial.println("Ct:n        - Set groups per strip (1-16)");
        Serial.println("Cw:n        - Set spacer width (0-20)");
        Serial.println("Co:n        - Set start offset (0-9)");
        Serial.println("Ca:n        - Set animate interval (10-1000 ms)");
        Serial.println("Cb:n        - Set blink interval (50-3000 ms)");
        Serial.println("Cu:n        - Set update interval (5-500 ms)");
        Serial.println("Ci:n        - Set brightness (10-255)");
        Serial.println("Cf:n        - Set fading factor");
        Serial.println("Cz:Y/N      - Alternative shelf ordering");
        Serial.println("Cg:Y/N      - Enable/disable startup animation");
        Serial.println("Ce:Y/N      - Enable/disable command echo");
        Serial.println("Cx:p1,p2... - Set GPIO pins for strips");
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
        Serial.print("Strip Type       : ");
        #ifdef USE_RGBW_STRIPS
          Serial.println("RGBW (4 bytes/LED)");
          Serial.println("Chipset          : SK6812");
          Serial.println("Color Order      : RGB");
        #else
          Serial.println("RGB (3 bytes/LED)");
          Serial.println("Chipset          : WS2812B");
          Serial.println("Color Order      : GRB");
        #endif
        Serial.print("MCU ID           : ");
        Serial.println(MCUid);
        Serial.println("=================================\n");
        break;

      // Display curren configuration
      case 'D':
        Serial.print("Display configuration:\n" );
        ShowConfiguration();
        break;

      // Set configuration
      case 'C':
        SetConfigParameters(Data);
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
        test = sscanf(Data, "%2d:%d", &GroupID, &State);
        if ( test == 2) {
          if (GroupID >= 1 && GroupID <= MAX_GROUPS) {
            if (State >= 0 && State <= 9) {
              TermState[GroupID -1] = State;
              TermPct[GroupID -1] = 100;
              sprintf(output,"Group %d state set to %d", GroupID, State);
              Serial.println(output);
            } else {
              Serial.print("ERROR: Invalid state '");
              Serial.print(State);
              Serial.println("', use 0-9");
            }
          } else {
            Serial.print("ERROR: Invalid Group-ID '");
            Serial.print(GroupID);
            Serial.println("', use 1-48");
          }
        } else
          Serial.print("Syntax error: Use T<Group-ID>:<STATE>\n" );
        break;
               
      // Set Group state 
      case 'P':
        test = sscanf(Data, "%2d:%d:%3d", &GroupID, &State, &Pct);
        if (test == 3) {
          if (GroupID >= 1 && GroupID <= MAX_GROUPS) {
            if (State >= 0 && State <= 9) {
              if (Pct >= 0 && Pct <= 100) {
                TermState[GroupID -1] = State;
                TermPct[GroupID -1] = uint8_t(Pct);
                sprintf(output,"Group %d state set to %d with progress %d%%", GroupID, State, Pct);
                Serial.println(output);
              } else {
                Serial.print("ERROR: Invalid percentage '");
                Serial.print(Pct);
                Serial.println("', use 0-100");
              }
            } else {
              Serial.print("ERROR: Invalid state '");
              Serial.print(State);
              Serial.println("', use 0-9");
            }
          } else {
            Serial.print("ERROR: Invalid Group-ID '");
            Serial.print(GroupID);
            Serial.println("', use 1-48");
          }
        } else {
          Serial.print("Syntax error: Use Pgg:s:ppp (gg=group 1-48, s=state 0-9, ppp=percent 0-100)\n");
        }
        break;

      // Set state for all Group 
      case 'A':
        test = sscanf(Data, ":%d", &State);
        if (test == 1) {
          if (State >= 0 && State <= 9) {
            Serial.print("All groups set to state " );
            Serial.println(State);
            for(int GroupID=1; GroupID <= MAX_GROUPS; GroupID++){
              TermState[GroupID -1 ] = State;
              TermPct[GroupID -1] = 100;
            }
          } else {
            Serial.print("ERROR: Invalid state '");
            Serial.print(State);
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
          for (int state = 0; state <= 9; state++) {
            if (stateCounts[state] > 0) {
              Serial.print("  State ");
              Serial.print(state);
              Serial.print(": ");
              Serial.print(stateCounts[state]);
              Serial.print(" group");
              if (stateCounts[state] != 1) Serial.print("s");
              Serial.println();
            }
          }
          Serial.println("====================\n");
        }
        break;

      // Get state for all Groups 
      case 'G':
            Serial.println("\n=== Group States ===");
           
            for(int Index=0; Index<LedConfig.numStrips; Index++){
              // for alternative ordering
              if (LedConfig.altShelfOrder == true)
                (Index %2 == 0) ? Strip=Index/2 : Strip=(Index/2)+4;
              else
                Strip = Index; 

              Serial.print("Strip ");
              Serial.print(Strip +1);
              Serial.print(" (Groups ");
              sprintf(output, "%2d",Index*LedConfig.numGroupsPerStrip+1);
              Serial.print(output);
              Serial.print("-");
              sprintf(output, "%2d",Index*LedConfig.numGroupsPerStrip+LedConfig.numGroupsPerStrip);
              Serial.print(output);
              Serial.print("): ");
 
              for(int term=0; term<LedConfig.numGroupsPerStrip; term++){
                int groupIdx = (Index*LedConfig.numGroupsPerStrip)+term;
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
        Serial.println();
        if ( Data[0] == ':') {
          int GroupID=1;
          int totalChars = strlen(Data+1);
          
          while ( Data[GroupID] != 0 && GroupID<=MAX_GROUPS ) {
            ST=Data[GroupID];
            State = atoi(&ST);
            if (State >= 0 && State <= 9) {
              TermState[GroupID -1]=State;
              TermPct[GroupID -1] = 100;
            }
            GroupID++;
          }
          
          // Warn if there are surplus values
          if (totalChars > MAX_GROUPS) {
            Serial.print("WARNING: ");
            Serial.print(totalChars - MAX_GROUPS);
            Serial.print(" surplus values ignored (max ");
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
        for(int GroupID=0; GroupID < MAX_GROUPS; GroupID++){
          TermState[GroupID] = 0;
          TermPct[GroupID] = 100;
        }
        SetAllLEDs(CRGB::Black);
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
          Serial.println(commandCount);
          Serial.print("Errors           : ");
          Serial.println(errorCount);
          Serial.println("==========================\n");
        }
        break;

      // Reboot controller
      case 'R':
        Serial.print("Rebooting controller...\n" );
        RebootMCU();
        break;

      default:
        Serial.print("ERROR: command unknown.\n" );
        errorCount++;
        break;
    }
  } else {
    Serial.print("SYNTAX ERROR: Use <A-Z>, H for help.\n" );
    errorCount++;
  }
}

void RebootMCU() {
  watchdog_enable(1, 1);  // Timeout is set to the minimum value, which is 1ms
  while (true) {
    delay(10);
  }
}

void ShowHelp(){
  Serial.print  ("  T<GroupID>:<State>             Set Group state. GroupID: 1-");
  Serial.print  (MAX_GROUPS);
  Serial.println(" and State: 0-9");
  Serial.print  ("  P<GroupID>:<State>:<Pct>       Set Group state. GroupID: 1-");
  Serial.print  (MAX_GROUPS);
  Serial.println(", State: 0-9, PCt=0-100% progress");
  Serial.println("  M:<State><State>...           Set state for multiple Group, sequentually listed, e.g: '113110'");
  Serial.println("  A:<State>                     Set state for all Groups, state (0-9)");
  Serial.println("  G                             Get states for all Groups");
  Serial.println("  X                             Set all states to off (same as sending'A:0'");
  Serial.println();
  Serial.println("  H                             This help");
  Serial.println("  R                             Reboot controller (Disconnects from serial!)");
  Serial.println("  W                             Shows welcome/startup loop");
  Serial.println();
  Serial.println("  D                             Show current configuration");
  Serial.println("  Cn:<string>                   Set Controller name (ID) (1-16 chars)");
  Serial.print  ("  Cl:<value>                    Set amount of LEDs per shelf (");
  Serial.print  (NUM_LEDS_PER_STRIP_MIN);
  Serial.print  ("-");
  Serial.print  (NUM_LEDS_PER_STRIP_MAX);
  Serial.println(")"); 
  Serial.print  ("  Ct:<value>                    Set amount of groups per shelf (1-");
  Serial.print  (NUM_GROUPS_PER_STRIP_MAX);
  Serial.println(")");
  Serial.println("  Cs:<value>                    Set amount of shelves (1-8)");
  Serial.print  ("  Cw:<value>                    Set spacer-width (LEDs between groups, 0-");
  Serial.print  (SPACER_WIDTH_MAX);
  Serial.println(")");
  Serial.print  ("  Co:<value>                    Set starting offset (skipping leds at start of strip, 0-");
  Serial.print  (START_OFFSET_MAX);
  Serial.println(")");
  Serial.print  ("  Cb:<value>                    Set blink-interval in msec (");
  Serial.print  (BLINK_INTERVAL_MIN);
  Serial.print  ("-");
  Serial.print  (BLINK_INTERVAL_MAX);
  Serial.println(")");
  Serial.print  ("  Cu:<value>                    Set update interfal in mSec (");
  Serial.print  (UPDATE_INTERVAL_MIN);
  Serial.print  ("-");
  Serial.print  (UPDATE_INTERVAL_MAX);
  Serial.println(")");
  Serial.print  ("  Ca:<value>                    Set animate-interval in msec (");
  Serial.print  (ANIMATE_INTERVAL_MIN);
  Serial.print  ("-");
  Serial.print  (ANIMATE_INTERVAL_MAX);
  Serial.println(")");
  Serial.println("  Ci:<value>                    Set brightness intensity (0-255)");
  Serial.println("  Cf:<value>                    Set fading factor (0-255)");
  Serial.println("  Cc:<state>:<value>            Set color for state in HEX RGB order (state 1-9, value: RRGGBB)");
  Serial.println("  Cp:<state>:<pattern>          Set display-pattern for state in (state: 0-9, pattern 0-9) [for colorblind assist]");
  Serial.println("  Cz:<yes/true/no/false>        Set Alternative shelf order pattern (False/True)");
  Serial.println("  C4:<yes/true/no/false>        Set RGBW leds (4bytes) instead of RGB (3bytes) (False/True)");
  Serial.print  ("  Cx:<shelve>:<cpio-pin>        Set CPIO pin (");
  Serial.print  (GPIO_PIN_MIN);
  Serial.print  ("-");
  Serial.print  (GPIO_PIN_MAX);
  Serial.println(") per shelve-ledstrip (1-8)");
  Serial.println("  Cd:                           Reset all settings to factory defaults");
  Serial.println();
  Serial.println("  L                             Load stored configuration from EEPROM/FLASH");
  Serial.println("  S                             Save configuration to EEPROM/FLASH");
  Serial.println("\n");
}

/**
 * Update all LED groups based on their current state and percentage
 * Called periodically by timer to refresh LED display
 */
void updateGroups() {
  // Loop over all groups and adapt state
  for (int i = 0; i < LedConfig.numGroupsPerStrip * LedConfig.numStrips; i++) {
      SetLEDGroup(i, TermState[i],TermPct[i]);
  }
}

/**
 * Set LEDs for a specific group based on state, pattern, and percentage
 * @param group Group number (0-47)
 * @param state Display state (0-9)
 * @param pct Percentage fill (0-100)
 */
void SetLEDGroup(uint8_t group, uint8_t state, uint8_t pct) {
  int pattern=0, stripIndex=0, groupIndex=0, startLEDIndex=0, GroupWidth=0;

  pattern = LedConfig.state_pattern[state];
  stripIndex = floor(group / LedConfig.numGroupsPerStrip);

  // Switch odering to left=>right (14263748) instead of up=>down  (12345678)
  if (LedConfig.altShelfOrder == true) {
    (stripIndex %2 == 0) ? stripIndex=stripIndex/2 : stripIndex=(stripIndex/2)+4;
  }

  groupIndex = group % LedConfig.numGroupsPerStrip;
  startLEDIndex = groupIndex * round(LedConfig.numLedsPerStrip / LedConfig.numGroupsPerStrip) + LedConfig.startOffset ;
  GroupWidth = (LedConfig.numLedsPerStrip / LedConfig.numGroupsPerStrip) - LedConfig.spacerWidth;
  
  // Clear entire group first to prevent color overlap when state/percentage changes
  int fullGroupWidth = (LedConfig.numLedsPerStrip / LedConfig.numGroupsPerStrip) - LedConfig.spacerWidth;
  for(int i=0; i<fullGroupWidth; i++) {
    leds[stripIndex][startLEDIndex+i] = CRGB::Black;
    ZERO_W(leds[stripIndex][startLEDIndex+i]);
  }

  // for percentage
  if (pct<100) {
    pattern=0;  // Force pattern 0 (solid)
    float factor = float(pct)/100;
    GroupWidth = round(float(GroupWidth) * factor);
    if (GroupWidth == 0 && pct>0) 
      GroupWidth=1;  // only no leds on 0%
  }

  switch (pattern) {
      case 0: animate_Step[pattern]=animate_Step[pattern]%1;
              // 0 solid no blink    [########]
              //                     [########]
              for(int i=0; i<GroupWidth; i++) {
                leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];
              }
              break;
  
      case 1:	animate_Step[pattern]=animate_Step[pattern]%2;
              // 1 solid blink       [########]
              //                     [        ]
              for(int i=0; i<GroupWidth; i++) {
                  leds[stripIndex][startLEDIndex +i] = LedConfig.state_color[state]*blinkState;
                  ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              break;

      case 2:	animate_Step[pattern]=animate_Step[pattern]%2;
              // 1 solid blink inv.  [        ]
              //                     [########]
              for(int i=0; i<GroupWidth; i++) {
                  leds[stripIndex][startLEDIndex +i] = LedConfig.state_color[state]*!blinkState;
                  ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              break;

      case 3: animate_Step[pattern]=animate_Step[pattern]%2;
              // 2 Alternate L/R     [####    ] Count up c=0->1  s,s+(n/2) *c
              //                     [    ####]                  (n/2),n   *!c
              for(int i=0; i<(GroupWidth/2); i++) {
                  leds[stripIndex][startLEDIndex +i +( blinkState*(GroupWidth/2)) ] = LedConfig.state_color[state];
                  leds[stripIndex][startLEDIndex +i +(!blinkState*(GroupWidth/2)) ] = 0;
                  ZERO_W(leds[stripIndex][startLEDIndex +i +( blinkState*(GroupWidth/2))]);
                  ZERO_W(leds[stripIndex][startLEDIndex +i +(!blinkState*(GroupWidth/2))]);
              }
              break;

      case 4: animate_Step[pattern]=animate_Step[pattern]%2;
              // 3 Alternate in/out  [##      ##] Count up c=0->1  s,s+(n/4) + n-(n/4),n  
              //                     [  ##  ##  ]                  s+(n/4)-(n/2)+(n/4)
              for(int i=0; i<(GroupWidth); i++) {               
                if (i < (GroupWidth/4) or i >= (GroupWidth-(GroupWidth/4)) )
                  leds[stripIndex][startLEDIndex +i] = LedConfig.state_color[state] * blinkState;
                else
                  leds[stripIndex][startLEDIndex +i] = LedConfig.state_color[state] * !blinkState;
                ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              break;

      case 5: animate_Step[pattern]=animate_Step[pattern]%GroupWidth;
              // 4 odd/even          [# # # # ]
              //                     [ # # # #]
              for(int i=0; i<(GroupWidth); i++) {
                  if (i%2) 
                    leds[stripIndex][startLEDIndex +i] = LedConfig.state_color[state] * blinkState;
                  else 
                    leds[stripIndex][startLEDIndex +i] = LedConfig.state_color[state] * !blinkState;
                  ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              break;
  
      case 6: animate_Step[pattern]=animate_Step[pattern]%1;
              // 10 1/3 gated blink    [###  ###]
              for(int i=0; i<GroupWidth; i++) {
                if ( i <= (GroupWidth/3) or i >= (GroupWidth-(GroupWidth/3)-1) ) {
                  leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];
                  ZERO_W(leds[stripIndex][startLEDIndex+i]);
                }
              }
              break;
      case 7: animate_Step[pattern]=animate_Step[pattern]%2;
              // gated blink    [###  ###]
              //                [        ]
              for(int i=0; i<GroupWidth; i++) {
                if ( i <= (GroupWidth/3) or i >= (GroupWidth-(GroupWidth/3)-1) ) {
                  leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state] * blinkState;
                  ZERO_W(leds[stripIndex][startLEDIndex+i]);
                }
              }
              break;

      case 8: animate_Step[pattern]=animate_Step[pattern]%(GroupWidth);
                // 5 Animate >         [#       ]
                //                     [ #      ]
                //                     [  #     ] 
                //                     [   #    ]      
                //                     [    #   ]      
                //                     [     #  ] 
                //                     [      # ]
                //                     [       #]
              for(int i=0; i<GroupWidth; i++) {
                leds[stripIndex][startLEDIndex+i].fadeLightBy(LedConfig.fading);
                ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              leds[stripIndex][startLEDIndex + animate_Step[pattern] ] = LedConfig.state_color[state];
              ZERO_W(leds[stripIndex][startLEDIndex + animate_Step[pattern]]);
              break;
              // if ( i/(GroupWidth/2) == 0 )
              //   leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              // break;
              
      case 9: animate_Step[pattern]=animate_Step[pattern]%(GroupWidth);
              // 6 Animate <         [       #]
              //                     [      # ]
              //                     [     #  ] 
              //                     [    #   ]      
              //                     [   #    ]      
              //                     [  #     ] 
              //                     [ #      ]
              //                     [#       ]
              for(int i=0; i<GroupWidth; i++) {
                leds[stripIndex][startLEDIndex+i].fadeLightBy(LedConfig.fading);
                ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              leds[stripIndex][startLEDIndex + GroupWidth - animate_Step[pattern] -1 ] = LedConfig.state_color[state];
              ZERO_W(leds[stripIndex][startLEDIndex + GroupWidth - animate_Step[pattern] -1]);
              break;
      
      case 10: animate_Step[pattern]=animate_Step[pattern]%((GroupWidth*2));
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
              for(int i=0; i<GroupWidth; i++) {
                leds[stripIndex][startLEDIndex+i].fadeLightBy(LedConfig.fading);
                ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
                
              if (animate_Step[pattern] < GroupWidth ) {
                leds[stripIndex][startLEDIndex + animate_Step[pattern]] = LedConfig.state_color[state];
                ZERO_W(leds[stripIndex][startLEDIndex + animate_Step[pattern]]);
              } else {
                leds[stripIndex][startLEDIndex + (GroupWidth - (animate_Step[pattern] - GroupWidth)) -1] = LedConfig.state_color[state];
                ZERO_W(leds[stripIndex][startLEDIndex + (GroupWidth - (animate_Step[pattern] - GroupWidth)) -1]);
              } 
              break;

      case 11: animate_Step[pattern]=animate_Step[pattern]%(GroupWidth/2);
              // 8 Animate ><        [#      #]
              //                     [ #    # ]
              //                     [  #  #  ] 
              //                     [   ##   ]
              for(int i=0; i<GroupWidth; i++) {
                leds[stripIndex][startLEDIndex+i].fadeLightBy(LedConfig.fading);
                ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              leds[stripIndex][startLEDIndex + animate_Step[pattern]] = LedConfig.state_color[state];
              leds[stripIndex][startLEDIndex+GroupWidth - animate_Step[pattern] -1] = LedConfig.state_color[state];
              ZERO_W(leds[stripIndex][startLEDIndex + animate_Step[pattern]]);
              ZERO_W(leds[stripIndex][startLEDIndex+GroupWidth - animate_Step[pattern] -1]);
              break;

      case 12: animate_Step[pattern]=animate_Step[pattern]%(GroupWidth/2);
      
              // 9 Animate ><        [   ##   ]
              //                     [  #  #  ] 
              //                     [ #    # ]
              //                     [#      #]
              for(int i=0; i<GroupWidth; i++) {
                leds[stripIndex][startLEDIndex+i].fadeLightBy(LedConfig.fading);
                ZERO_W(leds[stripIndex][startLEDIndex+i]);
              }
              leds[stripIndex][startLEDIndex + (GroupWidth/2) - animate_Step[pattern] -1] = LedConfig.state_color[state];
              leds[stripIndex][startLEDIndex + (GroupWidth/2) + animate_Step[pattern]] = LedConfig.state_color[state];
              ZERO_W(leds[stripIndex][startLEDIndex + (GroupWidth/2) - animate_Step[pattern] -1]);
              ZERO_W(leds[stripIndex][startLEDIndex + (GroupWidth/2) + animate_Step[pattern]]);
              break;
 
    }
}

void SetAllLEDs(CRGB color){
  for(int n = 0; n < LedConfig.numStrips; n++)
    for(int i = 0; i < LedConfig.numLedsPerStrip; i++){
		  leds[n][i] = color;
		  ZERO_W(leds[n][i]);
	}
  FastLED.show();
}

void SetConfigParameters(char *Data){
   char ConfigItem = Data[0];
  char *Value = Data +2;
  int value_int=0;
  if (Data[1] == ':') {
    switch (ConfigItem) {
      // set Name-Idenitfier
      case 'n':
        // Check is Value is not bigger than size of LedConfig.identifier
        if (sizeof(Value) <= IDENTIFIER_MAX_LENGTH ){
          strncpy(LedConfig.identifier, Value, IDENTIFIER_MAX_LENGTH -1);
          for (int i=sizeof(Value); i<IDENTIFIER_MAX_LENGTH; i++) 
            LedConfig.identifier[i] = '\0'; // Ensure null-termination
          Serial.println();
          Serial.print("Controller Name (ID)   : " );
          Serial.println(LedConfig.identifier);
        } else {
          Serial.println("Identifier too long, use 16 characters max.");
        }
        break;
      // set led per strip
      case 'l':
        Serial.println();
        value_int=atoi(Value);
        if (value_int >= NUM_LEDS_PER_STRIP_MIN and value_int <= NUM_LEDS_PER_STRIP_MAX){
          Serial.print("LEDs per shelf       : " );
          LedConfig.numLedsPerStrip = atoi(Value);
          Serial.println(LedConfig.numLedsPerStrip);
          FastLED.clearData();
        } else {
          Serial.print("Invalid number of leds per strip(");
          Serial.print(NUM_LEDS_PER_STRIP_MIN);
          Serial.print("-");
          Serial.print(NUM_LEDS_PER_STRIP_MAX);
          Serial.println(")");
        }
        break;
      // set groups per shelf
      case 't':
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=1 and value_int <= NUM_GROUPS_PER_STRIP_MAX) {
          // Check if total groups would exceed MAX_GROUPS
          if (LedConfig.numStrips * value_int > MAX_GROUPS) {
            Serial.print("ERROR: numStrips (");
            Serial.print(LedConfig.numStrips);
            Serial.print(") * numGroupsPerStrip (");
            Serial.print(value_int);
            Serial.print(") = ");
            Serial.print(LedConfig.numStrips * value_int);
            Serial.print(" exceeds MAX_GROUPS (");
            Serial.print(MAX_GROUPS);
            Serial.println(")!");
          } else {
            Serial.print("Groups per shelf  : " );
            LedConfig.numGroupsPerStrip = value_int;
            Serial.println(LedConfig.numGroupsPerStrip);
            FastLED.clearData();
          }
        } else {
          Serial.print("Invalid number of groups per shelf (1-");
          Serial.print(NUM_GROUPS_PER_STRIP_MAX);
          Serial.println(")");
        }
        break;
      // set number of shelves
      case 's':
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=1 and value_int <= NUM_STRIPS_MAX){
          // Check if total groups would exceed MAX_GROUPS
          if (value_int * LedConfig.numGroupsPerStrip > MAX_GROUPS) {
            Serial.print("ERROR: numStrips (");
            Serial.print(value_int);
            Serial.print(") * numGroupsPerStrip (");
            Serial.print(LedConfig.numGroupsPerStrip);
            Serial.print(") = ");
            Serial.print(value_int * LedConfig.numGroupsPerStrip);
            Serial.print(" exceeds MAX_GROUPS (");
            Serial.print(MAX_GROUPS);
            Serial.println(")!");
          } else {
            Serial.print("Amount of shelves    : " );
            LedConfig.numStrips = value_int;
            Serial.println(LedConfig.numStrips);
            FastLED.clearData();
          }
        } else {
          Serial.print("Invalid number of shelves (1-");
          Serial.print(NUM_STRIPS_MAX);
          Serial.println(")");

        }
        break;
      // set spacer width
      case 'w':
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=0 and value_int <= SPACER_WIDTH_MAX){
          Serial.print("Spacer width         : " );
          LedConfig.spacerWidth = value_int;
          Serial.println(LedConfig.spacerWidth);
          FastLED.clearData();
        } else {
          Serial.print("Invalid space width(0-");
          Serial.print(SPACER_WIDTH_MAX);
          Serial.println(")");
        }
        break;
      case 'o':
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=0 and value_int <= START_OFFSET_MAX){
          Serial.print("Start offset         : " );
          LedConfig.startOffset = value_int;
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
        Serial.println();
        value_int=atoi(Value);
        if (value_int >= ANIMATE_INTERVAL_MIN and value_int <= ANIMATE_INTERVAL_MAX){
          Serial.print("Animation interval   : " );
          LedConfig.animateinterval = value_int;
          Serial.println(LedConfig.animateinterval);
          animate_Timer.detach();
          animate_Timer.attach_ms(LedConfig.animateinterval, &AnimateStep);
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
        Serial.println();
        value_int=atoi(Value);
        if (value_int >= BLINK_INTERVAL_MIN and value_int <= BLINK_INTERVAL_MAX){
          if (value_int > LedConfig.updateinterval) {
            Serial.print("Blinking interval    : " );
            LedConfig.blinkinterval = value_int;
            Serial.println(LedConfig.blinkinterval);
            blink_Timer.detach();
            blink_Timer.attach_ms(LedConfig.blinkinterval, &SetblinkState);
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
        Serial.println();
        value_int=atoi(Value);
        if (value_int >= UPDATE_INTERVAL_MIN and value_int <= UPDATE_INTERVAL_MAX){
          if (value_int < LedConfig.blinkinterval) {
            Serial.print("Update interval      : ");
            LedConfig.updateinterval = value_int;
            Serial.println(LedConfig.updateinterval);
            setgroup_Timer.detach();
            setgroup_Timer.attach_ms(LedConfig.updateinterval*2, &SetGroupState);
            update_Timer.detach();
            update_Timer.attach_ms(LedConfig.updateinterval, &WriteLedstripData);
            
            // Correct fade times
//            LedConfig.fading = LedConfig.fading * (LedConfig.updateinterval/);
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
        Serial.println();
        value_int=atoi(Value);
        if (value_int >= BRIGHTNESS_MIN and value_int <= BRIGHTNESS_MAX){
          Serial.print("Brightness intensity   : " );
          LedConfig.brightness = value_int;
          Serial.println(LedConfig.brightness);
          FastLED.setBrightness(LedConfig.brightness);
          FastLED.clearData();
          
          // Warn if brightness is very high
          if (value_int > BRIGHTNESS_WARNING_THRESHOLD) {
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
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=0 and value_int <= 255){
          Serial.print("Fading factor          : " );
          LedConfig.fading = value_int;
          Serial.println(LedConfig.fading);
          FastLED.clearData();
        } else {
          Serial.println("Invalid brightness intensity (0-255)");
        }
        break;

      // Set shelf-order
      case 'z':
        Serial.println();
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f') {
          LedConfig.altShelfOrder = false;
          Serial.print("Alt. Shelf order     : False");
        } else if (*Value == 'A' or *Value == 'a' or *Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't') {
          LedConfig.altShelfOrder = true;
          Serial.println("Alt. Shelf order     : True");
          FastLED.clearData();
         } else {
          Serial.println("Invalid pattern, choose between N (no) or A/Y (Alt/Yes)");
        }
        break;
      
      // Toggle startup animation
      case 'g':
        Serial.println();
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.startupAnimation = false;
          Serial.println("Startup animation    : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.startupAnimation = true;
          Serial.println("Startup animation    : Enabled");
        } else {
          Serial.println("Invalid value, use Y/N or 1/0");
        }
        break;
      
      // Toggle command echo
      case 'e':
        Serial.println();
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f' or *Value == '0') {
          LedConfig.commandEcho = false;
          Serial.println("Command echo         : Disabled");
        } else if (*Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't' or *Value == '1') {
          LedConfig.commandEcho = true;
          Serial.println("Command echo         : Enabled");
        } else {
          Serial.println("Invalid value, use Y/N or 1/0");
        }
        break;
      // Set color-patern (colorblind assist)
      case 'p':
        setLedStatePattern(Value);
        FastLED.clearData();
        break;
      // set colors
      case 'c':
        setLedStateColor(Value);
        FastLED.clearData();
        break;
      // Set GPIO pins
      case 'x':
        setLedStripGPIO(Value);
        FastLED.clearData();
        break;
      // set defaults
      case 'd':
        Serial.println();
        resetToDefaults();
        Serial.println("Configuration reset to defaults\n");
        FastLED.clearData();
        break;
      default:
        Serial.print("SYNTAX ERROR: Use <A-Z>, H for help.\n" );
        break;
    }
  } else {
    Serial.print("SYNTAX ERROR: Use <A-Z>, H for help.\n" );
  }
}

void setLedStripGPIO(char *Value) {
  if (Value[1] == ':') {
    uint8_t strip = Value[0] - '0' -1; 
    
    if (strip >= 0 && strip < NUM_STRIPS_DEFAULT) {
      char *GPIO_RAW = Value + 2; 
      uint8_t GPIO_PIN = strtoul(GPIO_RAW, NULL, 16);
      if ( GPIO_PIN > GPIO_PIN_MIN && GPIO_PIN <= GPIO_PIN_MAX) {

        // Test if GPIO pin is not assigned already
        for (uint8_t STRIP=0; STRIP<NUM_STRIPS_DEFAULT ; STRIP++) {    
          if (GPIO_PIN == LedConfig.stripGPIOpin[STRIP]) {
            Serial.print("ERROR: GPIO-PIN ");
            Serial.print(GPIO_PIN);
            Serial.print(" is already used for strip "); 
            Serial.print(STRIP);
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

        LedConfig.stripGPIOpin[strip] = GPIO_PIN;
        Serial.print("GPIO-PIN for shelve ");
        Serial.print(strip);
        Serial.print(" is set to : ");
        Serial.print(LedConfig.stripGPIOpin[strip]);
        Serial.println();
        Serial.println("Please note a MCU reboot is required to activate a change in GPIO pin assigments");
      } else {
        Serial.println("Invalid GPIO-PIN number.");
      }
    } else {
      Serial.print  ("Invalid shelve, 1-");
      Serial.print  (NUM_STRIPS_MAX);
      Serial.println(" only.");
    }
  } else {
      Serial.println("Syntax error: use Cx:<shelve>:<GPIO-PIN>     (<shelve: 1-");
      Serial.print  (NUM_STRIPS_MAX);
      Serial.print  (", <GPIO-PIN>: ");
      Serial.print  (GPIO_PIN_MIN);
      Serial.print  ("-");
      Serial.print  (GPIO_PIN_MAX);
      Serial.println(")\n");
  }
}

void setLedStateColor(char *Value) {
  char buffer[10];
  Serial.println();
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

void setLedStatePattern(char *Value) {
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
 * Reset all configuration parameters to default values
 */
void resetToDefaults() {
  CPULED(0x00,0x00,0x80);

  strcpy(LedConfig.identifier, IDENTIFIER_DEFAULT);
  LedConfig.numLedsPerStrip = NUM_LEDS_PER_STRIP_DEFAULT;
  LedConfig.numStrips = NUM_STRIPS_DEFAULT;
  LedConfig.numGroupsPerStrip = NUM_GROUPS_PER_STRIP_DEFAULT;
  LedConfig.spacerWidth = SPACER_WIDTH_DEFAULT;
  LedConfig.startOffset = START_OFFSET;
  LedConfig.blinkinterval = BLINK_INTERVAL;
  LedConfig.updateinterval = UPDATE_INTERVAL;
  LedConfig.brightness = BRIGHTNESS;
  LedConfig.fading = FADING;
  LedConfig.altShelfOrder = ALTERNATIVE_SHELFORDERING;
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
  LedConfig.stripGPIOpin[0] = 2;
  LedConfig.stripGPIOpin[1] = 3;
  LedConfig.stripGPIOpin[2] = 4;
  LedConfig.stripGPIOpin[3] = 5;
  LedConfig.stripGPIOpin[4] = 6;
  LedConfig.stripGPIOpin[5] = 7;
  LedConfig.stripGPIOpin[6] = 8;
  LedConfig.stripGPIOpin[7] = 9; 
}

/**
 * Display current configuration to serial console
 */
void ShowConfiguration() {
  char output[MAX_OUTPUT_LEN];
  memset(output, '\0', sizeof(output));

  Serial.print("Identifier           : ");
  Serial.println(LedConfig.identifier); 

  Serial.print("LEDs per shelf       : ");
  Serial.println(LedConfig.numLedsPerStrip); 

  Serial.print("Groups per shelf  : ");
  Serial.println(LedConfig.numGroupsPerStrip); 

  Serial.print("Amount of shelves    : ");
  Serial.println(LedConfig.numStrips); 

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

  Serial.print("Fading factor        : ");
  Serial.println(LedConfig.fading); 

  Serial.print("Alt. shelf order     : ");
  if (LedConfig.altShelfOrder == true )
    Serial.println("True");      
  else
    Serial.println("False");      
  
  Serial.println("LED Mode             : RGB-only (W channel = 0)");

  Serial.print("Overall brightness   : ");
  Serial.println(LedConfig.brightness); 

  Serial.println(); 
  Serial.print("Shelve LedStrip      : | "); 
  for (uint8_t strip=0; strip<NUM_STRIPS_DEFAULT; strip++) {
    sprintf(output, "%2d | ", strip+1);
    Serial.print(output);    
  }
  Serial.println(); 
  Serial.print("GPIO-PIN             : | "); 
  for (uint8_t strip=0; strip<NUM_STRIPS_DEFAULT; strip++) {
    sprintf(output, "%02d | ", LedConfig.stripGPIOpin[strip] );
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

char* readFile(const char * path) {
  CPULED(0x00,0x00,0x80);
  File fileH = LittleFS.open(F(path), "r");
  if (!fileH) {
    Serial.print("NOTE: Failed opening confgfile\n" );
    return 0;
  }

  long fileSize=fileH.size();
  if (fileSize > 0) {
    char *data = new char[fileSize+1];
    if (data == nullptr) {
      Serial.println("NOTE: Memory allocation failed!");
      fileH.close();
      return 0;
    }
    fileH.readBytes(data, fileSize);
    data[fileSize] = '\0'; 

    fileH.close();
    return data;
  } else {
      Serial.println("NOTE: File is empty!");
      return nullptr;  // Return nullptr if the file is empty
  }
}

bool writeFile(const char * path, const char * data, size_t dataSize) {
  CPULED(0x00,0x00,0x80);
  File fileH = LittleFS.open(F(path), "w");
  if (!fileH) {
    Serial.print("* Opening Failed\n" );
    return false;
  }
  size_t bytesWritten = fileH.write((const uint8_t*)data, dataSize);
  if (bytesWritten != dataSize) {
    Serial.print("* Write Failed\n");
    fileH.close();
    return false;
  }
  fileH.close();
  return true;
}

/**
 * Load configuration from flash storage
 * Validates checksum and all parameter ranges
 * @return true if config loaded successfully, false otherwise
 */
bool loadConfiguration() {
  CPULED(0x00,0x00,0x80);

  char *buffer;
  buffer = readFile(CONFIG_FILENAME);
  if (buffer != 0) {

    // Calulate sizes to separate struct and checksum in buffer
    int structSize=sizeof(LedData);
    int totalSize = structSize + 32;  // 32 for the saved binary checksum
 
    // Separate the data and the checksum
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

      // Validate and clamp all values to safe ranges
      bool needsCorrection = false;
      
      if (LedConfig.numLedsPerStrip < NUM_LEDS_PER_STRIP_MIN || LedConfig.numLedsPerStrip > NUM_LEDS_PER_STRIP_MAX) {
        LedConfig.numLedsPerStrip = NUM_LEDS_PER_STRIP_DEFAULT;
        needsCorrection = true;
      }
      
      if (LedConfig.numStrips < 1 || LedConfig.numStrips > NUM_STRIPS_MAX) {
        LedConfig.numStrips = NUM_STRIPS_DEFAULT;
        needsCorrection = true;
      }
      
      if (LedConfig.numGroupsPerStrip < 1 || LedConfig.numGroupsPerStrip > NUM_GROUPS_PER_STRIP_MAX) {
        LedConfig.numGroupsPerStrip = NUM_GROUPS_PER_STRIP_DEFAULT;
        needsCorrection = true;
      }
      
      // Critical: Ensure total groups doesn't exceed MAX_GROUPS
      if (LedConfig.numStrips * LedConfig.numGroupsPerStrip > MAX_GROUPS) {
        Serial.println("WARNING: numStrips * numGroupsPerStrip exceeds MAX_GROUPS!");
        LedConfig.numStrips = NUM_STRIPS_DEFAULT;
        LedConfig.numGroupsPerStrip = NUM_GROUPS_PER_STRIP_DEFAULT;
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
        Serial.println("WARNING: Some values were out of range and corrected to defaults.");
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
    // No data, set defaults
    delete[] buffer;   // Free memory
    return false;
  }
}

/**
 * Save current configuration to flash storage with SHA256 checksum
 * @return true if save successful, false otherwise
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

  // Create a final buffer containing the serialized data + checksum (in binary)
  char finalBuffer[sizeof(buffer) + 32];           // 32 bytes for the checksum
  memcpy(finalBuffer, buffer, sizeof(buffer));     // Copy serialized LedData
  memcpy(finalBuffer + sizeof(buffer), hash, 32);  // Copy binary checksum after the data (append)

  // Write the combined buffer to the file
  return writeFile(CONFIG_FILENAME, finalBuffer, sizeof(finalBuffer));
}

void FadeAll(int StartLed=1, int EndLed=LedConfig.numLedsPerStrip, float fadeFactor=1.4) {
  CPULED(0x00,0x00,0x80);
  for(int i = StartLed; i < EndLed; i++)
    for(int n = 0; n < LedConfig.numStrips; n++) {
      // leds[n][i].nscale8(200)
      leds[n][i].r=leds[n][i].r/fadeFactor;
      leds[n][i].g=leds[n][i].g/fadeFactor;
      leds[n][i].b=leds[n][i].b/fadeFactor;
      ZERO_W(leds[n][i]);
    }
}
 
void StartupLoop() {
  // Boot animation
  CPULED(0x00,0x00,0x80);

  // Green closing [->><<-]
  uint8_t DELAY (LedConfig.numLedsPerStrip / 10);
  CRGB color = 0x00FF00;
  for(int i = 0; i < LedConfig.numLedsPerStrip/2; i+=1) {
    for(int n = 0; n < LedConfig.numStrips; n++) {
      leds[n][i] = CRGB::Green;
      leds[n][LedConfig.numLedsPerStrip -i -1] = CRGB::Green;
      ZERO_W(leds[n][i]);
      ZERO_W(leds[n][LedConfig.numLedsPerStrip -i -1]);
    }
    FastLED.show();
    FadeAll(0,LedConfig.numLedsPerStrip,1.4);
    delay(DELAY);
  }
 
  //  Flash [######]
  SetAllLEDs(CRGB::Green);
  FastLED.show();
  delay(75);
  for(int i = 0; i < 12; i++) {
    FadeAll(0,LedConfig.numLedsPerStrip,1.5);
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

// WS2812B CPULED timing
#define T1H  7
#define T1L  1
#define T0H  1
#define T0L  7
#define RESET_TIME 100

inline void delay_ns(uint32_t ns) {
    uint32_t cycles = ns ; // was ns/8   //  8ns per cycle at 125 MHz
    for (volatile uint32_t i = 0; i < cycles; i++) {
        __asm volatile ("nop"); // Each nop takes one clock cycle
    }
}

void sendByte_CPULED(uint8_t byte) {
  for (int i = 7; i >= 0; i--) {
    if (byte & (1 << i)) {
      gpio_put(CPULED_GPIO, 1);
      delay_ns(T1H);
      gpio_put(CPULED_GPIO, 0);
      delay_ns(T1L);
    } else {
      gpio_put(CPULED_GPIO, 1);
      delay_ns(T0H);
      gpio_put(CPULED_GPIO, 0);
      delay_ns(T0L);
    }
  }
}

void CPULED(uint32_t color) {
    sendByte_CPULED((color && 0x0000FF00) >> 8);  // Send green byte
    sendByte_CPULED((color && 0x00FF0000) >>16);  // Send red byte
    sendByte_CPULED((color && 0x000000FF) >> 0);  // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}
void CPULED(CRGB color) {
    sendByte_CPULED((color && 0x00FF0000) >> 8);  // Send green byte
    sendByte_CPULED((color && 0xFF000000) >>16);  // Send red byte
    sendByte_CPULED((color && 0x0000FF00) >> 0);  // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}
void CPULED(uint8_t r, uint8_t g, uint8_t b) {
    sendByte_CPULED(g);  // Send green byte
    sendByte_CPULED(r);  // Send red byte
    sendByte_CPULED(b);  // Send blue byte
    busy_wait_us(RESET_TIME); // Reset time after sending color
}