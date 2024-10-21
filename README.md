# S.I.G.H.T
**S**helf **I**ndicators for **G**uided **H**andling **T**asks 

![SIGHT Installation](images/castlewall.jpg?raw=true "SIGHT instalation")

# What is S.I.G.H.T.

S.I.G.H.T. is an 8-Channel LED Strip Controller is designed for use in warehouse order picking or as shelf position indicators. 
By connecting to your system via USB, it identifies as a serial port, allowing for straightforward interaction and control.
The main setup is based on an RP2040 microcontroller from Waveshare, but it will work find on any RP2040 MCU.
For the indicated common WS28xx ledstrips can be used.


## RP2040 Zero
![RP2040 Zero](images/RP2040-Zero.png?raw=true "RP2040 Zero")

The model used in my PCB is this one: [https://www.waveshare.com/wiki/RP2040-Zero](https://www.waveshare.com/wiki/RP2040-Zero)
But it works just s fine on the much cheaper clones from AliExpress: [https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html](https://nl.aliexpress.com/w/wholesale-RP2040%2525252dZero.html)


## Ledstrips
It can drive all WS28xx based ledstrips although due to the redundant data lines I recoment the 5v based WS-2813 or the 12V based WS-2815 for longer lengths (over 1.5mtr).
For 12v strips you have to change the capacitor on the driver board for an 16v model. (An replacement schematics+PCB suitable for both will be added soon).

![LEdStrip models](images/ledstrip-models.png?raw=true "LedStrip models")

For more details about LEDstrips see [user-guide-for-ws2812b-ws2811-sk6812-and-ws2815](https://www.superlightingled.com/blog/a-user-guide-for-ws2812b-ws2811-sk6812-and-ws2815/)


## PCB's
There are ready for use PCB's in the PCB foler, you can import them into [EasyEDA](https://easyeda.com/) and from there send in the oder to [JLCPCB](https://jlcpcb.com/) to the the ready for use PCB.s


## Compile
The source code for the firmware con be compile using the userfriendly [Arduino IDE](https://www.arduino.cc/en/software), please make sure to install the libraries for:
 - RaspBerry Pi Pico / RP2040
 - FastLED
 - LittleFS
 - Crypto
 - Ticker

## Usage: 
Upon connection, the controller will undergo a startup sequence. When the serial port is inactive (not opened in any application), the onboard LED will pulse red slowly, and all eight outputs will sequentially pulse every 200 ms with a 1-second interval for testing purposes. Once the serial port is opened, a welcome animation will play on all LED strips, followed by loading configuration settings from flash memory if available.

The startup sequence output will look as follows:

```
-=[ Shelf Indicators for Guided Handling Tasks ]=-

SIGHT Version   : 1.6
MicroController : WAVESHARE_RP2040_ZERO
MCU-Serial      : E6632C85931E832C

Initializing...

LittleFS mounted successfully.
Checksum matches, configuration loaded.

Identifier           : SIGHT-1.6
LEDs per shelf       : 57
Groups per shelf     : 6
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

Shelve LedStrip      :  1 |  2 |  3 |  4 |  5 |  6 |  7 | 8
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

```
  T<TermID>:<State>             Set Group state. TermID: 1-48 and State: 0-9
  P<TermID>:<State>:<Pct>       Set Group state. TermID: 1-48, State: 0-9, PCt=0-100% progress
  M:<State><State>...           Set state for multiple Groups, sequentually listed, e.g: '113110'
  A:<State>                     Set state for all Groups, state (0-9)
  G                             Get states for all Groups
  X                             Set all states to off (same as sending'A:0'

  H                             This help
  R                             Reboot controller (Disconnects from serial!)
  W                             Shows welcome/startup loop

  D                             Show current configuration
  Cn:<string>                   Set Controller name (ID) (1-16 chars)
  Cl:<value>                    Set amount of LEDs per shelf (6-144)
  Ct:<value>                    Set amount of Groups per shelf (1-16)
  Cs:<value>                    Set amount of shelves (1-8)
  Cw:<value>                    Set spacer-width (LEDs between Groups, 0-20)
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

To set a status for a group, e.g. set group 1 to status 1,  a send a simple `T1:1`

To send the status for a serie of groups, e.g. send statsu for the first 8 groups: `M:14262435` 

To reset all groups send a `X`

There are 10 different statuses that can be used, but status 0 is hardcoded to 'off' (all LEDs off), the statuses 1-9 can be freely configured with RGB color and blinking/animations.

Set color for a state, eg. set state 2 to orange: send `Cc:2:FF6000`

Set pattern/animation for a state, eg. set state 2 to chase up/down: send `Cp:2:10`

The hardcode animations are:
```
 0.  Solid on                     [########]
 1.  Blinking                     [########]   [        ]
 2.  Blinking inverted            [        ]   [########]
 3.  ALternate left/right block   [####    ]   [    ####]
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
All setting for name, timing, colors and patterna can be saved on flash, those will automatically be loaded upon boot.
