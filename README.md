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
- `#define USE_RGB_LEDS` for WS2812B (3 bytes per LED)
- `#define USE_RGBW_LEDS` for WS2813B-RGBW, SK6812 (4 bytes per LED)

![LEdStrip models](images/ledstrip-models.png?raw=true "LedStrip models")

For more details about LED strips see [user-guide-for-ws2812b-ws2811-sk6812-and-ws2815](https://www.superlightingled.com/blog/a-user-guide-for-ws2812b-ws2811-sk6812-and-ws2815/)


## PCB's
There are ready for use PCB's in the PCB folder, you can import them into [EasyEDA](https://easyeda.com/) and from there send in the order to [JLCPCB](https://jlcpcb.com/) to get the ready for use PCBs.


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


## Quick Start

Get your SIGHT controller running in just 4 simple steps:

1. **Connect Hardware** 
   - Plug SIGHT into your USB port
   - Connect LED strips to the configured GPIO pins
   - Power on the system

2. **Open Serial Monitor**
   - Open Arduino IDE Serial Monitor
   - Set baud rate to **115200**
   - Wait for startup sequence to complete

3. **Basic Commands**
   - Type `H` and press Enter to see help
   - Try `T:1:1` to set first group to green
   - Try `X` to clear all groups
   - Try `Q` for quick status summary

4. **Save Configuration**
   - Type `S` to save your settings
   - Configuration will auto-load on next boot

**Example First Session:**
```
> H              # Show help
> T:1:1          # Set group 1 to green (state 1)
> T:2:2          # Set group 2 to orange (state 2) 
> Q              # Check status
> S              # Save configuration
```




## Usage: 
Upon connection, the controller will undergo a startup sequence. When the serial port is inactive (not opened in any application), the onboard LED will pulse red slowly, and all eight outputs will sequentially pulse every 200 ms with a 1-second interval for testing purposes. Once the serial port is opened, a welcome animation will play on all LED strips, followed by loading configuration settings from flash memory if available.

The startup sequence output will look as follows:

```
-=[ Shelf Indicators for Guided Handling Tasks ]=-

SIGHT Version  : 1.9
MicroController : WAVESHARE_RP2040_ZERO
MCU-Serial      : E6632C85931E832C

Initializing...

LittleFS mounted successfully.
Configuration loaded successfully.
Initialization done..,

Identifier           : SIGHT v1.9
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
Cl:n        - Set LEDs per strip (6-600)
Cs:n        - Set number of strips/channels (1-8)
Ct:n        - Set groups per strip (1-100)
Cw:n        - Set spacer width (0-20)
Co:n        - Set start offset (0-9)
Ca:n        - Set animate interval (10-1000 ms)
Cb:n        - Set blink interval (50-3000 ms)
Cu:n        - Set update interval (5-500 ms)
Ci:n        - Set brightness (10-255)
Cf:n        - Set fading factor
Cf2:n       - Set 2-step fading factor
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

## Version 1.9 Features

Version 1.9 builds upon the solid foundation of v1.8 with additional enhancements:

- **Enhanced LED capacity**: Support for up to 600 LEDs per channel and 100 groups per channel
- **Dual fading system**: Separate fading factors for regular and 2-step animations
- **Improved terminology**: Changed "shelf/strip/output" to "channel" for better clarity
- **Code quality improvements**: Professional variable naming (camelCase conventions)
- **Enhanced documentation**: Comprehensive comments and better code organization
- **Improved error messages**: More specific and helpful error reporting
- **Magic number elimination**: All hardcoded values replaced with named constants
- **Buffer size increases**: Input buffer expanded to 512 chars for larger configurations
- **Configuration identifier updates**: More robust config validation with version-specific IDs

## Version 1.8 Features

Version 1.8 included significant improvements:

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

## Troubleshooting

### Common Issues and Solutions

**LED Strips Not Working**
- **Problem**: No LEDs light up when sending commands
- **Solutions**: 
  - Check GPIO pin configuration with `D` command
  - Verify LED strip type (RGB vs RGBW) in firmware
  - Ensure power supply is adequate (5V/12V depending on strip type)
  - Check data line connections and polarity

**Configuration Not Saving**
- **Problem**: Settings lost after power cycle
- **Solutions**:
  - Ensure LittleFS space is reserved (8-64KB) in Arduino IDE
  - Check flash memory integrity with `I` command
  - Try `S` command to manually save configuration
  - Use `Se` to export backup before making changes

**LED Strips Flickering or Random Colors**
- **Problem**: LEDs show incorrect colors or flicker
- **Solutions**:
  - Check power supply capacity (under-voltage causes issues)
  - Verify data line length (keep < 3m without signal booster)
  - Add capacitor (1000µF) near strip power input
  - Check for electromagnetic interference

**Serial Communication Issues**
- **Problem**: No response to commands
- **Solutions**:
  - Verify baud rate is 115200
  - Check USB cable and port
  - Ensure line endings set to "Newline" in serial monitor
  - Try `R` command to reboot controller

**Memory or Performance Issues**
- **Problem**: Slow response or crashes
- **Solutions**:
  - Check system info with `I` command for memory usage
  - Reduce number of LEDs/groups if approaching limits
  - Ensure watchdog timer isn't triggering (8-second timeout)
  - Check for command buffer overflow with long commands

### Error Messages Reference

| Error Message | Cause | Solution |
|---------------|-------|----------|
| `Command 'X' unknown` | Invalid command | Use `H` for valid commands |
| `Invalid state` | State not 0-9 | Use states 0-9 only |
| `Group ID out of range` | Invalid group number | Use groups 1-MAX_GROUPS |
| `GPIO pin already used` | Duplicate pin assignment | Choose unused GPIO pin |
| `Configuration load failed` | Corrupted config | Use `Cd` to reset defaults |

### Getting Help

If you encounter issues not covered here:
1. Use `H` command for built-in help
2. Use `I` command to check system status
3. Export configuration with `Se` for analysis
4. Check GitHub issues for known problems
5. Contact support with system info and error details

## Application Examples

### Example 1: Warehouse Order Picking
**Setup**: 4 zones, 8 groups each, 57 LEDs per group
```
Cs:8                    # 8 channels/zones
Ct:8                    # 8 groups per zone
Cl:57                   # 57 LEDs per group
Cw:1                    # 1 LED spacer between groups
```
**Command Sequence**:
```
> T:1:1                 # Zone 1 - Item ready (green)
> T:2:2                 # Zone 2 - In progress (orange)
> T:3:3                 # Zone 3 - Priority (red)
> T:4:1                 # Zone 4 - Complete (green)
> S                     # Save configuration
```

### Example 2: Assembly Line Progress
**Setup**: 6 stations, progress indicators
```
Cs:6                    # 6 stations
Ct:4                    # 4 groups per station
Cl:30                   # 30 LEDs per group
Cp:2:10                 # Set state 2 to chase pattern
```
**Command Sequence**:
```
> P01:1:025             # Station 1, state 1, 25% complete
> P02:1:050             # Station 2, state 1, 50% complete
> P03:1:075             # Station 3, state 1, 75% complete
> P04:1:100             # Station 4, state 1, 100% complete
```

### Example 3: Storage Facility Status
**Setup**: 12 aisles, status lighting
```
Cs:8                    # 8 channels (multiple aisles per channel)
Ct:12                   # 12 groups (aisles)
Cl:20                   # 20 LEDs per aisle
Cc:1:00FF00             # Green for available
Cc:2:FFA500             # Orange for partial
Cc:3:FF0000             # Red for full
```
**Command Sequence**:
```
> M:132131213121        # Set aisle status pattern
> Q                     # Quick status summary
> G                     # Detailed aisle status
```

### Example 4: Multi-Stage Process Control
**Setup**: 5 process stages with animations
```
Cs:5                    # 5 stages
Ct:6                    # 6 indicators per stage
Cl:25                   # 25 LEDs per indicator
Cp:1:8                  # State 1: chase up
Cp:2:10                 # State 2: chase up/down
Cp:3:1                  # State 3: blinking
Cb:500                  # 500ms blink interval
```
**Command Sequence**:
```
> A:0                   # Clear all stages
> T:1:1                 # Start stage 1
> T:2:2                 # Start stage 2
> T:3:3                 # Start stage 3
> X                     # Emergency stop/clear
```

### Example 5: KPI Dashboard Display
**Setup**: Visual performance indicators
```
Cs:8                    # 8 KPI categories
Ct:4                    # 4 performance levels per KPI
Cl:40                   # 40 LEDs for visual impact
Cf:24                   # Smooth fading
Ca:200                 # 200ms animation speed
```
**Color Mapping**:
```
> Cc:1:00FF00           # Excellent - Green
> Cc:2:FFFF00           # Good - Yellow
> Cc:3:FFA500           # Warning - Orange
> Cc:4:FF0000           # Critical - Red
```
**Real-time Updates**:
```
> T:1:1                 # KPI 1: Excellent
> T:2:2                 # KPI 2: Good
> T:3:3                 # KPI 3: Warning
> T:4:4                 # KPI 4: Critical
```

### Configuration Backup Examples

**Export Current Setup**:
```
> Se                    # Export configuration as hex
OUTPUT: 53494748542D434... (copy this string)
```

**Import Saved Setup**:
```
> Li:53494748542D434...  # Paste the hex string
Configuration imported successfully
> S                     # Save to flash
```
