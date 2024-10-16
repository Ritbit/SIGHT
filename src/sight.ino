/*
Name    : SIGHT  (Shelf Indicators for Guided Handling Tasks)
Version : 1.5
Date    : 2024-09-23
Author  : Bas van Ritbergen <bas.vanritbergen@adyen.com> / <bas@ritbit.com>

NOTE :
  
The RGB/RGBW switch does not work properly yet since I made the RGB/RGBW option selectable in the config.
Need to find a fix for this, in the mean time only RGB leds are supported !	

TODO:  
  Adding led-animations instead of patterns

*/


// Basic defaults 
#define CONFIG_IDENTIFIER "SIGHT-CONFIG"
#define IDENTIFIER_DEFAULT "SIGHT-1.5"
#define NUM_LEDS_PER_STRIP_DEFAULT 57
#define NUM_LEDS_PER_STRIP_MIN 6
#define NUM_LEDS_PER_STRIP_MAX 144

#define NUM_STRIPS_DEFAULT 8
#define NUM_STRIPS_MAX 8

#define NUM_POSITIONS_PER_STRIP_DEFAULT 6
#define NUM_POSITIONS_PER_STRIP_MAX 16

#define SPACER_WIDTH_DEFAULT 1
#define SPACER_WIDTH_MAX 20

#define START_OFFSET 1
#define START_OFFSET_MAX 9

#define MAX_PositionS 48

#define BLINK_INTERVAL 200
#define BLINK_INTERVAL_MIN 100
#define BLINK_INTERVAL_MAX 3000

#define SET_POSITIONSTATE_INTERVAL 200
#define SET_POSITIONSTATE_INTERVAL_MIN 50
#define SET_POSITIONSTATE_INTERVAL_MAX 1000

#define UPDATE_INTERVAL 10
#define UPDATE_INTERVAL_MIN 5
#define UPDATE_INTERVAL_MAX 1000

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
#define BRIGHTNESS 255

#define RGBW_LEDS false;

#define GPIO_PIN_MIN 2
#define GPIO_PIN_MAX 26

// Uncomment for RGBW strips:
//#define RGBW true

// Define the pin connected to WS2812B
#define CPULED_GPIO 16

// Where do we store the configuration
#define CONFIG_FILENAME "/config.bin"

// ###########################################################################
// No configurable items below

// This Version
#define VERSION "1.5"

// Define max length for Input/Ouput buffers:
#define MAX_INPUT_LEN MAX_PositionS+4
#define MAX_OUTPUT_LEN 60

// Reserve space for LittleFS disk 2MB
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

// We definitely need these libraries
#include <Arduino.h>
#include <Ticker.h>
#include <Crypto.h>
#include <SHA256.h>
#include "LittleFS.h"
#include <FastLED.h>
#include "hardware/watchdog.h"

// Figure MCU type/serial
#include <MicrocontrollerID.h>
char MCUid [41];

// Declare LedStrip control arrays
// PLEASE READ THE NOTE ABOUT RGBW LEDS IN THE HEADER!!!!
//
// We cannot dynamically change this without crashing... :-(
#ifdef RGBW
#include "FastLED_RGBW.h"
CRGBW leds[NUM_STRIPS_DEFAULT+1][NUM_LEDS_PER_STRIP_MAX];
#else
CRGB leds[NUM_STRIPS_DEFAULT+1][NUM_LEDS_PER_STRIP_MAX];
#endif

// Config data is conviently stored in a struct (to easy store and retrieve from EEPROM/Flash)
// Set defaults, they will be overwritten by load from EEPROM
struct LedData {
  char identifier[16] = IDENTIFIER_DEFAULT;
  uint16_t numLedsPerStrip = NUM_LEDS_PER_STRIP_DEFAULT;
  uint8_t  numStrips = NUM_STRIPS_DEFAULT;
  uint8_t  numPositionsPerStrip = NUM_POSITIONS_PER_STRIP_DEFAULT;
  uint8_t  spacerWidth = SPACER_WIDTH_DEFAULT;
  uint8_t  startOffset = START_OFFSET;
  uint16_t blinkinterval = BLINK_INTERVAL;
  uint16_t updateinterval = UPDATE_INTERVAL;
  bool     altShelfOrder = ALTERNATIVE_SHELFORDERING;
  bool     useRGBWleds = RGBW_LEDS;
  uint8_t  stripGPIOpin[8] = {2,3,4,5,6,7,8,9};
  uint8_t  state_pattern[10] = {0,0,0,0,0,1,1,1,1,0};
  CRGB     state_color[10] = {CRGB::Black, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, COLOR_STATE_1, COLOR_STATE_2, COLOR_STATE_3, COLOR_STATE_4, CRGB::White};
} LedConfig = {};


// Input data from serial is stores in an array for further processing.
char inputBuffer[MAX_INPUT_LEN + 1]; // +1 for null terminator
uint8_t bufferIndex = 0;


// For color/blinking-state feature we need some extra global parameters
uint8_t  PosState[MAX_PositionS] = {0};
uint8_t  PosPct[MAX_PositionS] = {0};
volatile bool blinkState = 0;
uint32_t lastToggleTimes=0;

// for LEDstrip update frequency
int      updateinterval = 100;
uint32_t lastLedUpdate=0;

// for status update
uint32_t lastStateUpdate;

// Flags for updates
volatile bool LedstripUpdate = false;
volatile bool SetPositionStateFlag = false;

// Timer objects for pico_sdk
Ticker update_Timer;
Ticker blink_Timer;
Ticker setposition_Timer;

uint8_t animatestep[10]={0,0,0,0,0,0,0,0,0,0};

// ##########################################################################################################

// Setup MCU
void setup() {

  // Setup USB-serial port
  Serial.begin(115200);
  //Serial.setTimeout(0);

  // Initialize status led, set to pusing RED to show we are waiting for input
  //
  // Due to having only 8 PIO registers on an RP20204 Fastled can only drive 8 led outputs.
  // As a result we have to control the the CPULED via old-fashioned bitbanging if we want to use it.
  pinMode(CPULED_GPIO, OUTPUT); 
  gpio_put(CPULED_GPIO, 0);
  CPULED(0x00,0x00,0x00);

  // Enable GPIO 2-9 for pin test.
  for (int PIN=0; PIN<NUM_STRIPS_MAX; PIN++){
    pinMode(PIN+GPIO_PIN_MIN, OUTPUT);
  }

  while (!Serial) {
    // wait for serial port to connect.
    // Run a slow sequential GPIO pintest over GPIO GPIO_PIN_MIN -> GPIO_PIN_MIN+8 while waiting
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

  // Show welcome to let us know the controller is booting.
  // Also show some details about the MCU and the codeversion
  
  // Show version
  Serial.println();
  Serial.print("-=[ Shelf Indicators for Guided Handling Tasks ]=-\n" );
  Serial.println();
  Serial.print("SIGHT Version  : " );
  Serial.println(VERSION);  // Why does this add a 0 to the string ?? 

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
    if (loadConfiguration() == false) 
      resetToDefaults();
  }

  int MAX_LEDS=0;
  MAX_LEDS=(LedConfig.useRGBWleds)?LedConfig.numLedsPerStrip:LedConfig.numLedsPerStrip*(4/3)+1;

  for (uint8_t STRIP=0; STRIP<NUM_STRIPS_DEFAULT ; STRIP++) {
    pinMode(LedConfig.stripGPIOpin[STRIP], OUTPUT);
    if (LedConfig.stripGPIOpin[STRIP] != CPULED_GPIO){
      switch (LedConfig.stripGPIOpin[STRIP]) {
        // case  0: FastLED.addLeds<WS2812B,  0, GRB>(leds[STRIP], MAX_LEDS); break; // UART0 TX (for programming)
        // case  1: FastLED.addLeds<WS2812B,  1, GRB>(leds[STRIP], MAX_LEDS); break; // UART0 RX (for programming)
        case  2: FastLED.addLeds<WS2812B,  2, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 1
        case  3: FastLED.addLeds<WS2812B,  3, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 2
        case  4: FastLED.addLeds<WS2812B,  4, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 3
        case  5: FastLED.addLeds<WS2812B,  5, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 4
        case  6: FastLED.addLeds<WS2812B,  6, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 5
        case  7: FastLED.addLeds<WS2812B,  7, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 6
        case  8: FastLED.addLeds<WS2812B,  8, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 7
        case  9: FastLED.addLeds<WS2812B,  9, GRB>(leds[STRIP], MAX_LEDS); break; // Default strip 8
        case 10: FastLED.addLeds<WS2812B, 10, GRB>(leds[STRIP], MAX_LEDS); break;
        case 11: FastLED.addLeds<WS2812B, 11, GRB>(leds[STRIP], MAX_LEDS); break;
        case 12: FastLED.addLeds<WS2812B, 12, GRB>(leds[STRIP], MAX_LEDS); break;
        case 13: FastLED.addLeds<WS2812B, 13, GRB>(leds[STRIP], MAX_LEDS); break;
        case 14: FastLED.addLeds<WS2812B, 14, GRB>(leds[STRIP], MAX_LEDS); break;
        case 15: FastLED.addLeds<WS2812B, 15, GRB>(leds[STRIP], MAX_LEDS); break;
        case 16: FastLED.addLeds<WS2812B, 15, GRB>(leds[STRIP], MAX_LEDS); break; // = CPU LED on RP2040 zero
        case 17: FastLED.addLeds<WS2812B, 17, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins
        case 18: FastLED.addLeds<WS2812B, 18, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins
        case 19: FastLED.addLeds<WS2812B, 19, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins
        case 20: FastLED.addLeds<WS2812B, 20, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins
        case 21: FastLED.addLeds<WS2812B, 21, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins
        case 22: FastLED.addLeds<WS2812B, 22, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins
        case 23: FastLED.addLeds<WS2812B, 23, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins  
        case 24: FastLED.addLeds<WS2812B, 24, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins  
        case 25: FastLED.addLeds<WS2812B, 25, GRB>(leds[STRIP], MAX_LEDS); break; // Not on header pins  
        case 26: FastLED.addLeds<WS2812B, 26, GRB>(leds[STRIP], MAX_LEDS); break;
        case 27: FastLED.addLeds<WS2812B, 27, GRB>(leds[STRIP], MAX_LEDS); break;
        case 28: FastLED.addLeds<WS2812B, 28, GRB>(leds[STRIP], MAX_LEDS); break;
        case 29: FastLED.addLeds<WS2812B, 29, GRB>(leds[STRIP], MAX_LEDS); break;
      }
    }
   	FastLED.setBrightness(BRIGHTNESS);
  }

 
  StartupLoop();

  // start timers for updating leds (x1000 so we set mSec)
  update_Timer.attach_ms(LedConfig.updateinterval,&WriteLedstripData); 
  blink_Timer.attach_ms(LedConfig.blinkinterval, &SetblinkState); 
  setposition_Timer.attach_ms(LedConfig.updateinterval*2,&SetPositionState); 

  // Show help & config:
  // ShowHelp();
  ShowConfiguration() ;

  Serial.println("Enter 'H' for help ");
  Serial.println();

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

void SetPositionState(){
  SetPositionStateFlag = true;
  return;
}

// ##########################################################################################################

// Main loop
void loop() {
  if (Serial.available() > 0) {
    handleSerialInput();
  }

  //updateLEDs();
  if (SetPositionStateFlag) {
     FastLED.show();
     SetPositionStateFlag=false;
  }

  if (LedstripUpdate) {
    LedstripUpdate=false;
    updatePositions();
  }

}



// Handle serial input efficiently
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
    if (bufferIndex < MAX_INPUT_LEN - 1 && isPrintable(c)) {
      inputBuffer[bufferIndex++] = c;
      Serial.print(c);
    }
  }
}

bool isPrintable(char c) {
  return c >= 0x20 && c < 0x7E && c != 0x5C && c != 0x60;
}

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
  int PositionID;
  int Strip;
  char Command = input[0];
  char *Data = input +1; 

  if (Command >= 'A' && Command <= 'Z') {
    switch(Command) {

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
        Serial.print("Save configuration: " );
        if (saveConfiguration()) Serial.println("Success." );
        else                     Serial.println("Failed..." );
        break;

      // Load configuration from Flash/EEPROM
      case 'L':
        Serial.print("Load configuration: " );
        loadConfiguration();
        break;

      // Set Position state 
      case 'T':
        test = sscanf(Data, "%2d:%d", &PositionID, &State);
        if ( test == 2) {
          if (PositionID >= 1 && PositionID <= MAX_PositionS) {
            if (State >= 0 && State <= 9) {
              // setPositionState(PositionID -1,State,100);
              PosState[PositionID -1] = State;
              PosPct[PositionID -1] = 100;
              sprintf(output,"Position %d state set to %d", PositionID, State);
              Serial.println(output);
            } else 
              Serial.print("Invalid state, use 0-9\n" );
          } else
            Serial.print("Invalid Position-ID, use 1-48\n" );
        } else
          Serial.print("Syntax error: Use T<Position-ID>:<STATE>\n" );
        break;
               
      // Set Position state 
      case 'P':
        test = sscanf(Data, "%2d:%d:%3d", &PositionID, &State, &Pct);
        //if ( test == 3) {
          if (PositionID >= 1 && PositionID <= MAX_PositionS) {
            if (State >= 0 && State <= 9) {
              if (Pct >= 0 && State <= 100) {
                // setPositionState(PositionID -1, State, uint8_t(Pct));
                PosState[PositionID -1] = State;
                PosPct[PositionID -1] = uint8_t(Pct);
                sprintf(output,"Position %d state set to %d with progress %d ", PositionID, State, Pct);
                Serial.println(output);
              } else
                Serial.print("Invalid percentage, use 0-100\n" );
            } else
              Serial.print("Invalid state, use 0-9\n" );
          } else
            Serial.print("Invalid Position-ID, use 1-48\n" );
        break;

      // Set state for all Positions 
      case 'A':
        test = sscanf(Data, ":%d", &State);
        if (test == 1) {
          if (State >= 0 && State <= 9) {
            Serial.print("All Positions set to state " );
            Serial.println(State);
            for(int PositionID=1; PositionID <= MAX_PositionS; PositionID++){
              // setPositionState(PositionID,State,100);
              PosState[PositionID -1 ] = State;
              PosPct[PositionID -1] = 100;
            }
          } else {
            Serial.print("Invalid state, use 0-9\n" );
          }
        } else 
          Serial.print("Syntax error: Use A:<STATE>\n" );
        break;

      // Get state for all Positions 
      case 'G':
            Serial.print("State of all Positions: " );
            Serial.println();
           
            for(int Index=0; Index<LedConfig.numStrips; Index++){
              // for alternative ordering
              if (LedConfig.altShelfOrder == true)
                // if (Index %2 == 0) Strip=Index/2;
                // else Strip=(input /2)+4;
                (Index %2 == 0) ? Strip=Index/2 : Strip=(Index/2)+4;
              else
                Strip = Index; 

              Serial.print("Shelve ");
              Serial.print(Strip +1);
              Serial.print(" : ");
 
              for(int pos=0; pos<LedConfig.numPositionsPerStrip; pos++){
                Serial.print(PosState[(Index*LedConfig.numPositionsPerStrip)+pos]);
                Serial.print(' ');
              }

              Serial.print(" (Position ");
              sprintf(output, "%2d",Index*LedConfig.numPositionsPerStrip+1);
              Serial.print(output);
              Serial.print(" - ");
              sprintf(output, "%2d",Index*LedConfig.numPositionsPerStrip+LedConfig.numPositionsPerStrip);
              Serial.print(output);
              Serial.println(")");
            }

        break;
      
      // Set mass state, a digit for each Position (48 max)
      case 'M':
        char ST;
        Serial.println();
        if ( Data[0] == ':') {
          int PositionID=1;
          while ( Data[PositionID] != 0 && PositionID<=MAX_PositionS ) {
            ST=Data[PositionID];
            State = atoi(&ST);
            if (State >= 0 && State <= 9) {
              PosState[PositionID -1]=State;
              PosPct[PositionID -1] = 100;
              // setPositionState(PositionID -1,State,100);
            }
            PositionID++;
          }
        } else
          Serial.print("Syntax error: Use M:<STATE><STATE<<STATE>...\n");
        break;

      // Reset all states to off
      case 'X':
        Serial.print("Reset all Position states.\n" );
        for(int PositionID=0; PositionID < MAX_PositionS; PositionID++){
          // setPositionState(PositionID,0,100);
          PosState[PositionID -1] = 0;
          PosPct[PositionID -1] = 100;
        }
        SetAllLEDs(CRGB::Black);
        break;

      // Show startup loop
      case 'W':
        Serial.print("Showing startup loop.\n" );
        StartupLoop();
        break;

      // Reboot controller
      case 'R':
        Serial.print("Rebooting controller...\n" );
        RebootMCU();
        break;

      // Show help
      case 'h':
        ShowHelp();
        break;
      case 'H':
        ShowHelp();
        break;

      default:
        Serial.print("ERROR: command unknown.\n" );
        break;
    }
  } else {
    Serial.print("SYNTAX ERROR: Use <A-Z>, H for help.\n" );
  }
}

void RebootMCU() {
  watchdog_enable(1, 1);  // Timeout is set to the minimum value, which is 1ms
  while (true) {
    delay(10);
  }
 }

void ShowHelp(){
  Serial.print ("  T<PositionID>:<State>             Set Position state. PositionID: 1-");
  Serial.print (MAX_PositionS);
  Serial.println(" and State: 0-9");
  Serial.println("  P<PositionID>:<State>:<Pct>       Set Position state. PositionID: 1-");
  Serial.print (MAX_PositionS);
  Serial.println(", State: 0-9, PCt=0-100% progress");
  Serial.println("  M:<State><State>...           Set state for multiple Positions, sequentually listed, e.g: '113110'");
  Serial.println("  A:<State>                     Set state for all Positions, state (0-9)");
  Serial.println("  G                             Get states for all Positions");
  Serial.println("  X                             Set all states to off (same as sending'A:0'");
  Serial.println();
  Serial.println("  H                             This help");
  Serial.println("  R                             Reboot controller (Disconnects from serial!)");
  Serial.println("  W                             Shows welcome/startup loop");
  Serial.println();
  Serial.println("  D                             Show current configuration");
  Serial.println("  Ci:<string>                   Set Identifier (1-16 chars)");
  Serial.print  ("  Cl:<value>                    Set amount of LEDs per shelf (");
  Serial.print  (NUM_LEDS_PER_STRIP_MIN);
  Serial.print  ("-");
  Serial.print (NUM_LEDS_PER_STRIP_MAX);
  Serial.println(")"); 
  Serial.print ("  Ct:<value>                    Set amount of Positions per shelf (1-");
  Serial.print (NUM_POSITIONS_PER_STRIP_MAX);
  Serial.println(")");
  Serial.println("  Cs:<value>                    Set amount of shelves (1-8)");
  Serial.print ("  Cw:<value>                    Set spacer-width (LEDs between Positions, 0-");
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
  Serial.println("  Cc:<state>:<value>            Set color for state in HEX RGB order (state 1-9, value: RRGGBB)");
  Serial.println("  Cp:<state>:<pattern>          Set display-pattern for state in (state: 0-9, pattern 0-9) [for colorblind assist]");
  Serial.println("  Cz:<yes/true/no/false>        Set Alternative shelf order pattern (False/True)");
  Serial.println("  C4:<yes/true/no/false>        Set RGBW leds (4bytes) instead of RGB (3bytes) (False/True)");
  Serial.print  ("  Cx:<shelve>:<cpio-pin>       Set CPIO pin (");
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


void updatePositions() {
  // Clean ALL on all strips first (so also the unselected ones)
  FastLED.clearData();

  // Loop over all positions and adapte state if PositionBlink is true
  for (int i = 0; i < LedConfig.numPositionsPerStrip * LedConfig.numStrips; i++) {

    // Set leds for each Position(=Position), check blinkstate too
    if ( LedConfig.state_pattern[PosState[i]]%2 == 1 && blinkState == 0) {
      SetLEDPostion(i,0,0);
    } else {
      SetLEDPostion(i, PosState[i],PosPct[i]);
    }
  }
}
 
void SetLEDPostion(uint8_t position, uint8_t state, uint8_t pct) {

  uint8_t pattern = LedConfig.state_pattern[state];
  int stripIndex = floor(position / LedConfig.numPositionsPerStrip);
  if (LedConfig.altShelfOrder == true) {
    (stripIndex %2 == 0) ? stripIndex=stripIndex/2 : stripIndex=(stripIndex/2)+4;
  }
  int positionIndex = position % LedConfig.numPositionsPerStrip;
  int startLEDIndex = positionIndex * round(LedConfig.numLedsPerStrip / LedConfig.numPositionsPerStrip) + LedConfig.startOffset ;
  int PositionWidth = (LedConfig.numLedsPerStrip / LedConfig.numPositionsPerStrip) - LedConfig.spacerWidth;

  // for percentage
  if (pct<100) {
    pattern=pattern%2; // Force pattern 0  or 1
    float factor = float(pct)/100;
    PositionWidth = round(float(PositionWidth) * factor);
    if (PositionWidth == 0 && pct>0) 
      PositionWidth=1;  // only no leds on 0%
  }
  // Paterns
  // 0 solid no blink    [########]
  // 1 solid blink
  // 2 odd no blink      [# # # # ]
  // 3 odd blink
  // 4 halve no blink    [####    ]
  // 5 halve blink
  // 6 break no blink    [###  ###]
  // 7 break blink
  // 8 interval no blink [## ## ##]
  // 9 interval blink

  for(int i=0; i<PositionWidth; i++){
    switch (pattern) {
      case 0:
      case 1:	if (i < PositionWidth)
                leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              break;
      case 2:
      case 3: if (i%2 == 0 ) 
                leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              break;
      case 4:
      case 5: if ( i/(PositionWidth/2) == 0 )
                leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              break;
      case 6:
      case 7: if ( i+1 <= PositionWidth/3 || PositionWidth-i <= PositionWidth/3 )
                leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
              break;
      case 8:
      case 9: { int left = PositionWidth / 3;
                int right = PositionWidth / 3;
                int middle = PositionWidth - (left + right);               
                if (i < left || i >= (left + middle)) {
                  leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
                } else 
                if (middle == 1 && i == left) {
                  leds[stripIndex][startLEDIndex+i] = LedConfig.state_color[state];;
                }
              }
              break;
    }
	}
}

void SetAllLEDs(CRGB color){
  for(int n = 0; n < LedConfig.numStrips; n++)
    for(int i = 0; i < LedConfig.numLedsPerStrip; i++){
		  leds[n][i] = color;
	}
  FastLED.show();
}


void SetConfigParameters(char *Data){
  char ConfigItem = Data[0];
  char *Value = Data +2; 
  int value_int=0;
  if (Data[1] == ':') {
    switch (ConfigItem) {
      // set Idenitfier
      case 'i':
        Serial.println();
        Serial.print("Identifier           : " );
        strcpy(LedConfig.identifier,Value);
        Serial.println(LedConfig.identifier);
        break;
      // set led per strip
      case 'l':
        Serial.println();
        value_int=atoi(Value);
        if (value_int >= NUM_LEDS_PER_STRIP_MIN and value_int <= NUM_LEDS_PER_STRIP_MAX){
          Serial.print("LEDs per shelf       : " );
          LedConfig.numLedsPerStrip = atoi(Value);
          Serial.println(LedConfig.numLedsPerStrip);
        } else {
          Serial.print("Invalid number of leds per strip(");
          Serial.print(NUM_LEDS_PER_STRIP_MIN);
          Serial.print("-");
          Serial.print(NUM_LEDS_PER_STRIP_MAX);
          Serial.println(")");
        }
        break;
      // set Positions per shelf
      case 't':
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=1 and value_int <= NUM_POSITIONS_PER_STRIP_MAX) {
          Serial.print("Positions per shelf  : " );
          LedConfig.numPositionsPerStrip = value_int;
          Serial.println(LedConfig.numPositionsPerStrip);
        } else {
          Serial.print("Invalid number of Position per shelf (1-");
          Serial.print(NUM_POSITIONS_PER_STRIP_MAX);
          Serial.println(")");
        }
        break;
      // set number of shelves
      case 's':
        Serial.println();
        value_int=atoi(Value);
        if (value_int>=1 and value_int <= NUM_STRIPS_MAX){
          Serial.print("Amount of shelves    : " );
          LedConfig.numStrips = value_int;
          Serial.println(LedConfig.numStrips);
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
        } else {
          Serial.print("Invalid start offset (0-");
          Serial.print(START_OFFSET_MAX);
          Serial.println(")");
          
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
            setposition_Timer.detach();
            setposition_Timer.attach_ms(LedConfig.updateinterval*2, &SetPositionState); 
            update_Timer.detach();
            update_Timer.attach_ms(LedConfig.updateinterval, &WriteLedstripData); 
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
      // Set shelf-order
      case 'z':
        Serial.println();
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f') {
          LedConfig.altShelfOrder = false;
          Serial.print("Alt. Shelf order     : False");
        } else if (*Value == 'A' or *Value == 'a' or *Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't') {
          LedConfig.altShelfOrder = true;
          Serial.println("Alt. Shelf order     : True");
         } else {
          Serial.println("Invalid pattern, choose between N (no) or A/Y (Alt/Yes)");
        }
        break;
      // Set color-patern (colorblind assist)
      case 'p':
        setLedStatePattern(Value);
        break;
      // set colors
      case 'c':
        setLedStateColor(Value);
        break;
      //Set RGB/RGBW leds 
      // PLEASE READ THE NOTE ABOUT RGBW LEDS IN THE HEADER!!!!
      case '4':
        Serial.println();
        if (*Value == 'N' or *Value == 'n' or *Value == 'F' or *Value == 'f') {
          LedConfig.useRGBWleds = false;
          Serial.println("Using RGBW leds      : False");
          Serial.println("Please note a MCU reboot is required to activate a change in RGB/RGBW mode");
        } else if (*Value == 'A' or *Value == 'a' or *Value == 'Y' or *Value == 'y' or *Value == 'T' or *Value == 't') {
          LedConfig.useRGBWleds = true;
          Serial.println("Using RGBW leds      : True");
          Serial.println("Please note a MCU reboot is required to activate a change in RGB/RGBW mode");
         } else {
          Serial.println("Invalid pattern, choose between N (no) or A/Y (Alt/Yes)");
        }
        break;
      // Set GPIO pins
      case 'x': 
        setLedStripGPIO(Value);
        break;
      // set defaults
      case 'd':
        Serial.println();
        resetToDefaults();
        Serial.println("Configuration reset to defaults\n");
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
        sprintf(buffer, "%02X%02X%02X", LedConfig.state_color[state].red, LedConfig.state_color[state].green, LedConfig.state_color[state].blue);
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
  if (Value[1] == ':') {
    uint8_t state = Value[0] - '0'; 
    if (state > 0 && state <= 9) {
      uint8_t pattern = Value[2] - '0';

      if ( pattern <= 9) {
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
      Serial.println("Invalid state, 1-9 only");
    }
  } else {
    Serial.println("Syntax error: use Cp:<state>:<pattern>     (<state>: 0-9, <pattern>: 0-9)\n");
  }
}

void resetToDefaults() {
  CPULED(0x00,0x00,0x80);

  strcpy(LedConfig.identifier, IDENTIFIER_DEFAULT);
  LedConfig.numLedsPerStrip = NUM_LEDS_PER_STRIP_DEFAULT;
  LedConfig.numStrips = NUM_STRIPS_DEFAULT;
  LedConfig.numPositionsPerStrip = NUM_POSITIONS_PER_STRIP_DEFAULT;
  LedConfig.spacerWidth = SPACER_WIDTH_DEFAULT;
  LedConfig.startOffset = START_OFFSET;
  LedConfig.blinkinterval = BLINK_INTERVAL;
  LedConfig.updateinterval = UPDATE_INTERVAL;
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
  LedConfig.state_pattern[9] = 0; // no blink
  LedConfig.state_color[1] = COLOR_STATE_1;
  LedConfig.state_color[2] = COLOR_STATE_2;
  LedConfig.state_color[3] = COLOR_STATE_3;
  LedConfig.state_color[4] = COLOR_STATE_4;
  LedConfig.state_color[5] = COLOR_STATE_1;
  LedConfig.state_color[6] = COLOR_STATE_2;
  LedConfig.state_color[7] = COLOR_STATE_3;
  LedConfig.state_color[8] = COLOR_STATE_4;
  LedConfig.state_color[9] = CRGB::White;
  LedConfig.useRGBWleds = RGBW_LEDS;
  LedConfig.stripGPIOpin[0] = 2;
  LedConfig.stripGPIOpin[1] = 3;
  LedConfig.stripGPIOpin[2] = 4;
  LedConfig.stripGPIOpin[3] = 5;
  LedConfig.stripGPIOpin[4] = 6;
  LedConfig.stripGPIOpin[5] = 7;
  LedConfig.stripGPIOpin[6] = 8;
  LedConfig.stripGPIOpin[7] = 9; 
}

void ShowConfiguration() {
  char output[MAX_OUTPUT_LEN];
  memset(output, '\0', sizeof(output));

  Serial.print("Identifier           : ");
  Serial.println(LedConfig.identifier); 

  Serial.print("LEDs per shelf       : ");
  Serial.println(LedConfig.numLedsPerStrip); 

  Serial.print("Positions per shelf  : ");
  Serial.println(LedConfig.numPositionsPerStrip); 

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
  
  Serial.print("Alt. shelf order     : ");
  if (LedConfig.altShelfOrder == true )
    Serial.println("True");      
  else
    Serial.println("False");      
  
  Serial.print("Using RGBW leds      : ");
  if (LedConfig.useRGBWleds == true )
    Serial.println("True");      
  else
    Serial.println("False");      

  Serial.println(); 
  Serial.print("Shelve LedStrip      : "); 
  for (uint8_t strip=0; strip<NUM_STRIPS_DEFAULT; strip++) {
    sprintf(output, "%2d  ", strip+1);
    Serial.print(output);    
  }
  Serial.println(); 
  Serial.print("GPIO-PIN             : "); 
  for (uint8_t strip=0; strip<NUM_STRIPS_DEFAULT; strip++) {
    sprintf(output, " %02d ", LedConfig.stripGPIOpin[strip] );
    Serial.print(output);    
  }
  Serial.println(); 

  Serial.println(); 
  for (int state=1; state<=9; state++) {
    sprintf(output, "Color state %d        : %02X%02X%02X (RRGGBB) Pattern: %d", 
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
    Serial.println("NOTE: Failed opening confgfile\n" );
    return 0;
  }

  long fileSize=fileH.size();

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
      Serial.println("Checksum matches, configuration loaded.\n");
      return true;
    } else {
      Serial.println("Checksum mismatch, configuration is corrupted!\n");
      delete[] buffer;  // Free memory
      return false;
    }
  } else {
    // No data, set defaults
    return false;
  }
}

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
      #ifdef RGBW
      leds[n][i].w=leds[n][i].w/fadeFactor; 
      #endif
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
      leds[n][i] = color; 
      leds[n][LedConfig.numLedsPerStrip -i -1] = color;
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
