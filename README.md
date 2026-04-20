# GPS 1-to-Target Speed Timer for Teensy 4.0

https://github.com/user-attachments/assets/f5da079f-45f5-49f8-aa21-9851be51bdff

This project uses a Teensy 4.0, a u-blox GPS module, and a small I2C OLED display to measure acceleration time from `1 km/h` to a configurable target speed such as `5 km/h` for testing or `50 km/h` for the real run.

The timer starts automatically at `1 km/h` instead of `0 km/h` because GPS speed near zero is noisy and unreliable. The system only arms after the vehicle has come to a full stop, which helps avoid false starts.

## Libraries

```cpp
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
```

These libraries add the functionality the program needs.

| Library | Purpose |
| --- | --- |
| `SPI` | Communication protocol support. It is not heavily used here but is commonly included with display projects. |
| `Wire` | I2C communication used by the OLED display. |
| `Adafruit_GFX` | Graphics library for text and drawing. |
| `Adafruit_SSD1306` | Controls the SSD1306 OLED display. |
| `TinyGPSPlus` | Parses GPS data such as speed and location. |
| `SoftwareSerial` | Creates a serial connection for the GPS module. |



## Hardware

- Teensy 4.0
- u-blox NEO-M8N-O-10 GPS module
- antenna
- 0.96 inch I2C OLED display, 128x64, 4-pin
- breadboard
- jumper wires
- micro USB cable for Teensy power and programming

## Main idea

The project continuously reads GPS speed and uses a simple state machine to decide when the system is:

- waiting for the vehicle to stop
- armed and ready to start
- actively timing a run

The most recent result stays on the OLED until a newer run replaces it.

## Full operation

```text
Power ON
|
Wait GPS fix
|
Vehicle stopped
|
System arms
|
Vehicle moves
|
1 km/h -> timer starts
|
50 km/h -> timer stops
|
Result displayed
|
Stop again to arm next run
```

The target speed is controlled by `FINISH_SPEED_KMPH` in [sketch_mar11b.ino](C:\Users\chengkuan\source\sideprojects\sketch_mar11b\sketch_mar11b.ino). For example, set it to `5.0f` for testing or `50.0f` for the full run.

## State machine

The sketch uses a small state machine because it makes the timing logic predictable and easy to follow.

### `WAIT_FOR_STOP`

In this state, the code waits until the GPS speed falls below the stop threshold.

Purpose:

- allow a new run only after a real stop
- prevent the timer from re-arming while the vehicle is still rolling

### `READY_TO_START`

In this state, the vehicle is stopped and the system is armed.

Purpose:

- wait for the vehicle to move
- start timing automatically as soon as speed reaches `1 km/h`

### `TIMING`

In this state, the timer is running.

Purpose:

- measure elapsed time from `1 km/h`
- stop the timer automatically at the configured target speed
- save the result for display

## Why start at 1 km/h instead of 0 km/h

GPS speed near zero can bounce slightly even when the vehicle is stopped. Because of that:

- `0 km/h` is not a reliable timer start point
- `1 km/h` gives a cleaner and more repeatable start

This is why the code uses:

- `STOP_SPEED_KMPH` for deciding when the vehicle is considered stopped
- `START_SPEED_KMPH = 1.0f` for starting the timer

## Configurable speed thresholds

Near the top of [sketch_mar11b.ino](C:\Users\chengkuan\source\sideprojects\sketch_mar11b\sketch_mar11b.ino), these constants control the test:

```cpp
static const float STOP_SPEED_KMPH = 0.5f;
static const float START_SPEED_KMPH = 1.0f;
static const float FINISH_SPEED_KMPH = 5.0f;
```

### What each one means

- `STOP_SPEED_KMPH`
  - vehicle is treated as stopped below this value
  - this helps filter out GPS noise at zero speed

- `START_SPEED_KMPH`
  - timer begins at this speed
  - this normally stays at `1.0`

- `FINISH_SPEED_KMPH`
  - timer stops at this target speed
  - change this from `5.0` to `50.0` when you want to switch from testing to the full run

## OLED layout and display design

The OLED shows three types of information:

- current speed
- current system status
- the live timer or the last completed result

The most recent completed result stays on the screen until another run finishes.

The screen uses fixed Y positions so each row has a clear role:

```cpp
static const int SPEED_LABEL_Y = 0;
static const int SPEED_VALUE_Y = 12;
static const int STATUS_Y = 42;
static const int RESULT_LABEL_Y = 50;
static const int RESULT_VALUE_Y = 54;
```

On a 128x64 OLED, `Y = 0` is the top of the screen and `Y = 63` is the bottom. In this design:

- `SPEED_LABEL_Y` places the `Speed` label at the top
- `SPEED_VALUE_Y` places the large live speed below it
- `STATUS_Y` places the current state message in the middle
- `RESULT_LABEL_Y` places the result label near the bottom
- `RESULT_VALUE_Y` places the measured time below the label

This gives the display a simple top-to-bottom structure:

```text
Y=0   Speed
Y=12  34.5 km/h

Y=42  Status text

Y=50  Last 1-50
Y=54  4.23 s
```

The design separates live information from stored information:

- the top area is for speed
- the middle area is for state and system feedback
- the bottom area is for the live timer or the last saved result

## Layout tuning

If you want to move text around on the OLED, the easiest values to edit are near the top of [sketch_mar11b.ino](C:\Users\chengkuan\source\sideprojects\sketch_mar11b\sketch_mar11b.ino):

```cpp
static const int SPEED_LABEL_Y = 0;
static const int SPEED_VALUE_Y = 12;
static const int STATUS_Y = 42;
static const int RESULT_LABEL_Y = 50;
static const int RESULT_VALUE_Y = 54;

static const uint8_t SPEED_VALUE_SIZE = 3;
static const uint8_t RESULT_VALUE_SIZE = 1;
```

- increase a `Y` value to move that text downward
- decrease a `Y` value to move that text upward
- increase a `SIZE` value to make text bigger
- decrease a `SIZE` value to make text smaller

The result number is centered by a helper function, so if you want to shift it left or right, adjust the X calculation in the centering helper.

## Centering helpers

The sketch uses two helper functions to center text horizontally:

- `drawCenteredFlashText()`
- `drawCenteredText()`

These functions measure the width of the text, calculate the middle of the screen, and then place the cursor so the text appears centered instead of starting from the far left.

`drawCenteredFlashText()` is used for fixed text stored with `F("...")`, such as titles.

`drawCenteredText()` is used for generated text such as the timer value.

This is separate from the `*_Y` layout constants:

- the `*_Y` values control vertical placement
- the centering helpers control horizontal placement

## Wiring notes

This project is assembled on a breadboard with jumper wires.

Typical connections:

- OLED `VCC` -> Teensy power
- OLED `GND` -> Teensy `GND`
- OLED `SCL` -> Teensy I2C clock pin
- OLED `SDA` -> Teensy I2C data pin
- GPS `VCC` -> Teensy power
- GPS `GND` -> Teensy `GND`
- GPS `TX` -> Teensy software serial RX pin
- GPS `RX` -> Teensy software serial TX pin

The GPS serial settings in the code mean:

| Variable | Meaning |
| --- | --- |
| `RXPin` | Teensy receives data from GPS |
| `TXPin` | Teensy sends data to GPS |
| `GPSBaud` | GPS communication speed |

The `Wire` library automatically uses the default I2C pins for the board.

For Teensy boards, the default pins are usually:

| Function | Pin |
| --- | --- |
| `SDA` (data) | `18` |
| `SCL` (clock) | `19` |

## References

- [Guide for OLED display with Arduino](https://randomnerdtutorials.com/guide-for-oled-display-with-arduino/)
- [TinyGPSPlus library](https://github.com/mikalhart/TinyGPSPlus/tree/master)


