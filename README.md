# S.I.G.H.T
**S**helf **I**ndicators for **G**uided **H**andling **T**asks 

![SIGHT Installation](images/castlewall.jpg?raw=true "SIGHT instalation")

# What is S.I.G.H.T.

S.I.G.H.T. is an 8-Channel LED Strip Controller designed for use in warehouse order picking or as shelf position indicators. 
By connecting to your system via USB, it identifies as a serial port, allowing for straightforward interaction and control.
The main setup is based on an RP2040 microcontroller from Waveshare, but it will work fine on any RP2040 MCU.
For the indicators, common WS28xx LED strips can be used, including both RGB (WS2812B) and RGBW (WS2813B/SK6812) variants.


## RP2040 Zero
![RP2040 Zero](images/RP2040-Zero.png?raw=true "RP2040 Zero")

The model used in my PCB is this one: [https://www.waveshare.com/wiki/RP2040-Zero](https://www.waveshare.com/wiki/RP2040-Zero)
But it works just s fine on the much cheaper clones from AliExpress: [https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html](https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html)


## LED Strips
It can drive all WS28xx based LED strips, supporting both RGB (WS2812B) and RGBW (WS2813B/SK6812) variants. 
Due to the redundant data lines, I recommend the 5V based WS-2813 or the 12V based WS-2815 for longer lengths (over 1.5m).
For 12V strips you have to change the capacitor on the driver board for a 16V model. (A replacement schematics+PCB suitable for both will be added soon).

**Important:** Configure your LED strip type in the firmware by uncommenting the appropriate define:
- `#define USE_RGB_STRIPS` for WS2812B (3 bytes per LED)
- `#define USE_RGBW_STRIPS` for WS2813B-RGBW, SK6812 (4 bytes per LED)

![LEdStrip models](images/ledstrip-models.png?raw=true "LedStrip models")

For more details about LED strips see [user-guide-for-ws2812b-ws2811-sk6812-and-ws2815](https://www.superlightingled.com/blog/a-user-guide-for-ws2812b-ws2811-sk6812-and-ws2815/)


## PCB's
There are ready for use PCB's in the PCB foler, you can import them into [EasyEDA](https://easyeda.com/) and from there send in the oder to [JLCPCB](https://jlcpcb.com/) to the the ready for use PCB.s


## Compile
The source code for the firmware can be compiled using the user-friendly [Arduino IDE](https://www.arduino.cc/en/software). 
To add support for the Waveshare RP2040 Zero, please follow the instructions here [https://www.waveshare.com/wiki/RP2040-Zero](https://www.waveshare.com/wiki/RP2040-Zero) to add the [Waveshare RP2040-repository](https://www.waveshare.com/wiki/RP2040-Zero) to the Arduino IDE.
And then install support for this board:
 - Raspberry Pi Pico / RP2040

Please make sure to install the libraries for:
 - FastLED
 - FastLED_RGBW (for RGBW strip support)
 - LittleFS
 - Crypto
 - Ticker
 - MicroControllerID

**Important:** When compiling, make sure to reserve a little space for LittleFS (8-64KB) in the Arduino IDE board settings to enable configuration storage.




## Usage: 
Upon connection, the controller will undergo a startup sequence. When the serial port is inactive (not opened in any application), the onboard LED will pulse red slowly, and all eight outputs will sequentially pulse every 200 ms with a 1-second interval for testing purposes. Once the serial port is opened, a welcome animation will play on all LED strips, followed by loading configuration settings from flash memory if available.

The startup sequence output will look as follows:

```
-=[ Shelf Indicators for Guided Handling Tasks ]=-

SIGHT Version  : 1.8
MicroController : WAVESHARE_RP2040_ZERO
MCU-Serial      : E6632C85931E832C

Initializing...

LittleFS mounted successfully.
Configuration loaded successfully.
Initialization done..,

Identifier           : SIGHT v1.8
LEDs per shelf       : 57
Groups per shelf     : 6
Amount of shelves    : 8
Spacer width         : 1
Start Offset         : 1
Blinking interval    : 200
Update interval      : 25
Animate interval     : 150
Fading factor        : 48
Alt. shelf order     : False
Startup animation    : True
Command echo         : False
Overall brightness   : 255

Shelve LedStrip      :  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8
GPIO-PIN             : 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09

Color state 1        : 008000 (RRGGBB) Pattern: 0
Color state 2        : FF8C00 (RRGGBB) Pattern: 0
Color state 3        : FF0000 (RRGGBB) Pattern: 0
Color state 4        : 0000FF (RRGGBB) Pattern: 0
Color state 5        : 008000 (RRGGBB) Pattern: 1
Color state 6        : FF8C00 (RRGGBB) Pattern: 1
Color state 7        : FF0000 (RRGGBB) Pattern: 1
Color state 8        : 0000FF (RRGGBB) Pattern: 1
Color state 9        : FFFFFF (RRGGBB) Pattern: 10

Enter 'H' for help

>
```


After the initialization, a command prompt (>) will appear, indicating that the controller is ready to accept commands. Below is a list of available commands:

## Command Reference

### System Commands
```
V           - Show version information
H or ?      - Show help
D           - Display current configuration
I           - Show system info (RAM/Flash usage, uptime, statistics)
S           - Save configuration to flash
Se          - Save/Export configuration as hex (backup)
L           - Load configuration from flash
Li:CONFIG:  - Load/Import configuration from hex (restore)
R           - Reboot controller
W           - Shows welcome/startup loop
```

### Group Control Commands
```
T:n:s       - Set group n to state s (0-9)
M:ssssss    - Set multiple groups at once by providing a list of states (e.g. M:12345)
A:s         - Set all groups to state s (0-9)
Pgg:s:ppp   - Set group state (gg=group 1-48, s=state 0-9, ppp=percent 0-100)
X           - Clear all groups (set to state 0)
Q           - Quick status summary (active groups per state)
G           - Get detailed state of all groups
```

### Configuration Commands (C prefix)
```
Cn:name     - Set controller name/identifier (max 16 chars)
Cl:n        - Set LEDs per strip (6-300)
Cs:n        - Set number of strips/shelves (1-8)
Ct:n        - Set groups per strip (1-16)
Cw:n        - Set spacer width (0-20)
Co:n        - Set start offset (0-9)
Ca:n        - Set animate interval (10-1000 ms)
Cb:n        - Set blink interval (50-3000 ms)
Cu:n        - Set update interval (5-500 ms)
Ci:n        - Set brightness (10-255)
Cf:n        - Set fading factor
Cz:Y/N      - Alternative shelf ordering
Cg:Y/N      - Enable/disable startup animation
Ce:Y/N      - Enable/disable command echo
Cx:p1,p2... - Set GPIO pins for strips
Cp:s:n      - Set pattern for state s (0-9)
Cc:s:RRGGBB - Set color for state s (hex)
Cd          - Reset to default configuration
```

## Command Examples

**Set a status for a group:**
- Set group 1 to status 1: `T:1:1`

**Set status for multiple groups:**
- Send status for the first 8 groups: `M:14262435`

**Set group with progress percentage:**
- Set group 5 to state 2 with 75% progress: `P05:2:075`

**Reset all groups:**
- Clear all: `X`

**Check status:**
- Quick summary: `Q`
- Detailed group states: `G`

**Configuration backup/restore:**
- Export config: `Se`
- Import config: `Li:CONFIG:<hex_string>`

There are 10 different statuses that can be used, but status 0 is hardcoded to 'off' (all LEDs off), the statuses 1-9 can be freely configured with RGB color and blinking/animations.

**Set color for a state:**
- Set state 2 to orange: `Cc:2:FF6000`

**Set pattern/animation for a state:**
- Set state 2 to chase up/down: `Cp:2:10`

## Animation Patterns

The hardcoded animations are:
```
 0.  Solid on                     [########]
 1.  Blinking                     [########]   [        ]
 2.  Blinking inverted            [        ]   [########]
 3.  Alternate left/right block   [####    ]   [    ####]
 4.  Alternate 4-part pattern     [##    ##]   [  ####  ]
 5.  Alternate odd/even LEDs      [# # # # ]   [ # # # #]
 6.  Gated solid                  [###  ###]
 7.  Gated blink                  [###  ###]   [        ]
 8.  Chase up                     [>>>>>>>>]
 9.  Chase down                   [<<<<<<<<]           
 10. Chase up/down                [>>>>>>>><<<<<<<<]
 11. Dual chase in                [>>>><<<<]
 12. Dual chase out               [<<<<>>>>]
```

All settings for name, timing, colors and patterns can be saved to flash, and will automatically be loaded upon boot.

## Version 1.8 Features

Version 1.8 includes significant improvements:

- **Comprehensive config validation** on load and runtime
- **Watchdog timer** for system stability (8-second timeout with auto-reboot)
- **Command echo** support for debugging/logging (Ce command)
- **Version info** command (V) with build date and strip type
- **System info** command (I) showing RAM/Flash usage and uptime
- **Config backup/restore** via hex export/import (Se/Li commands)
- **Statistics tracking** (uptime, command count, error count)
- **Quick status summary** (Q command) showing active groups per state
- **Improved group state display** (G command) with percentage indicators
- **Brightness safety limits** (10-255) with warnings above 200
- **Buffer overflow protection** and comprehensive input validation
- **Improved error messages** showing actual values
- **Function documentation** and const correctness throughout
- **Fixed group LED clearing** to prevent color overlap
- **RGBW support** for SK6812 and WS2813B-RGBW strips
