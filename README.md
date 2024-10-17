# S.I.G.H.T
**S**helf **I**ndicators for **G**uided **H**andling **T**asks 


This 8-Channel LED Strip Controller is designed for use in warehouse order picking or as shelf position indicators. By connecting to your system via USB, it identifies as a serial port, allowing for straightforward interaction and control.
The main setup is based on an RP2040 microcontroller from Waveshare, but it will work find on any RP2040 MCU.

The model used in my PCB is this one: [https://www.waveshare.com/wiki/RP2040-Zero](https://www.waveshare.com/wiki/RP2040-Zero)
But it works just s fine on the much cheaper clones from AliExpress: [https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html](https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html)

It can drive all WS28xx based ledstrips although I recoment the 5v based WS-2813 (for the redundant data) or the 12V WS-2815 (for longer lengths, over 1.5mtr)
The controller and driver board are both suitable for 5v and 12v strips.

Upon connection, the controller will undergo a startup sequence. When the serial port is inactive (no open), the onboard LED will pulse red slowly, and all eight outputs will sequentially pulse every 200 ms with a 1-second interval for testing purposes. Once the seial por tis opened, a welcome animation will play on all LED strips, followed by loading configuration settings from flash memory if available.

The startup sequence output will look as follows:

```
-=[ Shelf Indicators for Guided Handling Tasks  ]=-

SIGHT Version   : 1.5
MicroController : WAVESHARE_RP2040_ZERO
MCU-Serial      : E6632891E351A926

Initializing...

LittleFS mounted successfully.
Checksum matches, configuration loaded.

Identifier           : SIGHT-1.5
LEDs per shelf       : 57
Positions per shelf  : 9
Amount of shelves    : 8
Spacer width         : 1
Start Offset         : 1
Blinking interval    : 200
Update interval      : 10
Alt. shelf order     : False
Using RGBW leds      : False

Shelve LedStrip      :  1   2   3   4   5   6   7   8
GPIO-PIN             :  02  03  04  05  06  07  08  09

Color state 1        : 008000 (RRGGBB) Pattern: 0
Color state 2        : FF8C00 (RRGGBB) Pattern: 0
Color state 3        : FF0000 (RRGGBB) Pattern: 0
Color state 4        : 0000FF (RRGGBB) Pattern: 0
Color state 5        : 008000 (RRGGBB) Pattern: 1
Color state 6        : FF8C00 (RRGGBB) Pattern: 1
Color state 7        : FF0000 (RRGGBB) Pattern: 1
Color state 8        : 0000FF (RRGGBB) Pattern: 1
Color state 9        : FFFFFF (RRGGBB) Pattern: 0

Enter 'H' for help
>
```


After the initialization, a command prompt (>) will appear, indicating that the controller is ready to accept commands. Below is a list of available commands:

```
Copy code
  T<PositionID>:<State>         Set Position state. PositionID: 1-48 and State: 0-9
  P<PositionID>:<State>:<Pct>   Set Position state. PositionID: 1-48, State: 0-9 and Pct=0-100 (% progress)
  M:<State><State>...           Set state for multiple Positions, sequentially listed, e.g: '113110'
  A:<State>                     Set state for all Positions, state (0-9)
  G                             Get states for all Positions
  X                             Set all states to off (same as sending 'A:0')

  H                             This help
  R                             Reboot controller (Disconnects from serial!)
  W                             Shows welcome/startup loop

  D                             Show current configuration
  Ci:<string>                   Set Identifier (1-16 chars)
  Cl:<value>                    Set amount of LEDs per shelf (6-144)
  Ct:<value>                    Set amount of positions per shelf (1-16)
  Cs:<value>                    Set amount of shelves (1-8)
  Cw:<value>                    Set spacer-width (LEDs between positions, 0-20)
  Co:<value>                    Set starting offset (skipping LEDs at start of strip, 0-9)
  Cb:<value>                    Set blink-interval in msec (100-3000)
  Cu:<value>                    Set update interval in mSec (5-1000)
  Cc:<state>:<value>            Set color for state in HEX RGB order (state 1-9, value: RRGGBB)
  Cp:<state>:<pattern>          Set display-pattern for state in (state: 0-9, pattern 0-9) [for colorblind assist]
  Cz:<yes/true/no/false>        Set Alternative shelf order pattern (False/True)
  C4:<yes/true/no/false>        Set RGBW LEDs (4 bytes) instead of RGB (3 bytes) (False/True) (* Requires reboot)
  Cx:<shelve>:<cpio-pin>        Set CPIO pin (2-26) per shelve-ledstrip (1-8) (* Requires reboot)
  Cd:                           Reset all settings to factory defaults

  L                             Load stored configuration from EEPROM/FLASH
  S                             Save configuration to EEPROM/FLASH
```

