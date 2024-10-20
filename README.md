# S.I.G.H.T
**S**helf **I**ndicators for **G**uided **H**andling **T**asks 

![SIGHT Installation](images/castlewall.jpg?raw=true "SIGHT instalation")


This 8-Channel LED Strip Controller is designed for use in warehouse order picking or as shelf position indicators. By connecting to your system via USB, it identifies as a serial port, allowing for straightforward interaction and control.
The main setup is based on an RP2040 microcontroller from Waveshare, but it will work find on any RP2040 MCU.

![RP2040 Zero](images/RP2040-Zero.png?raw=true "RP2040 Zero")

The model used in my PCB is this one: [https://www.waveshare.com/wiki/RP2040-Zero](https://www.waveshare.com/wiki/RP2040-Zero)
But it works just s fine on the much cheaper clones from AliExpress: [https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html](https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html)

It can drive all WS28xx based ledstrips although I recoment the 5v based WS-2813 (for the redundant data) or the 12V WS-2815 (for longer lengths, over 1.5mtr)
The controller and driver board are both suitable for 5v and 12v strips. (For more details abotu LEDstrips see [user-guide-for-ws2812b-ws2811-sk6812-and-ws2815](https://www.superlightingled.com/blog/a-user-guide-for-ws2812b-ws2811-sk6812-and-ws2815/)
![LEdStrip models](images/ledstrip-models.png?raw=true "LedStrip models")


Upon connection, the controller will undergo a startup sequence. When the serial port is inactive (not opened in any application), the onboard LED will pulse red slowly, and all eight outputs will sequentially pulse every 200 ms with a 1-second interval for testing purposes. Once the serial port is opened, a welcome animation will play on all LED strips, followed by loading configuration settings from flash memory if available.

The startup sequence output will look as follows:

```
-=[ Shelf Indicators for Guided Handling Tasks ]=-

SIGHT Version  : 1.6
MicroController : WAVESHARE_RP2040_ZERO
MCU-Serial      : E6632C85931E832C

Initializing...

LittleFS mounted successfully.
Checksum matches, configuration loaded.

Identifier           : SIGHT-1.6
LEDs per shelf       : 57
Terminals per shelf  : 6
Amount of shelves    : 8
Spacer width         : 1
Start Offset         : 1
Blinking interval    : 200
Update interval      : 25
Animate interval     : 125
Fading factor        : 64
Alt. shelf order     : False
Using RGBW leds      : False
Overall brightness   : 255

Shelve LedStrip      :  1   2   3   4   5   6   7   8
GPIO-PIN             : 02 03 04 05 06 07 08 09

Color state 1        : 008000 (RRGGBB) Pattern: 0
Color state 2        : FF8C00 (RRGGBB) Pattern: 10
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
  T<TermID>:<State>             Set Terminal state. TermID: 1-48 and State: 0-9
  P<TermID>:<State>:<Pct>       Set Terminal state. TermID: 1-48, State: 0-9, PCt=0-100% progress
  M:<State><State>...           Set state for multiple Terminals, sequentually listed, e.g: '113110'
  A:<State>                     Set state for all Terminals, state (0-9)
  G                             Get states for all Terminals
  X                             Set all states to off (same as sending'A:0'

  H                             This help
  R                             Reboot controller (Disconnects from serial!)
  W                             Shows welcome/startup loop

  D                             Show current configuration
  Cn:<string>                   Set Controller name (ID) (1-16 chars)
  Cl:<value>                    Set amount of LEDs per shelf (6-144)
  Ct:<value>                    Set amount of terminals per shelf (1-16)
  Cs:<value>                    Set amount of shelves (1-8)
  Cw:<value>                    Set spacer-width (LEDs between terminals, 0-20)
  Co:<value>                    Set starting offset (skipping leds at start of strip, 0-9)
  Cb:<value>                    Set blink-interval in msec (50-3000)
  Cu:<value>                    Set update interfal in mSec (5-500)
  Ca:<value>                    Set animate-interval in msec (10-1000)
  Ci:<value>                    Set brightness intensity (0-255)
  Cf:<value>                    Set fading factor (0-255)
  Cc:<state>:<value>            Set color for state in HEX RGB order (state 1-9, value: RRGGBB)
  Cp:<state>:<pattern>          Set display-pattern for state in (state: 0-9, pattern 0-9) [for colorblind assist]
  Cz:<yes/true/no/false>        Set Alternative shelf order pattern (False/True)
  C4:<yes/true/no/false>        Set RGBW leds (4bytes) instead of RGB (3bytes) (False/True)
  Cx:<shelve>:<cpio-pin>        Set CPIO pin (2-26) per shelve-ledstrip (1-8)
  Cd:                           Reset all settings to factory defaults

  L                             Load stored configuration from EEPROM/FLASH
  S                             Save configuration to EEPROM/FLASH
```

