#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

// OLED size for a typical 0.96 inch SSD1306 I2C display.
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// GPS serial pins used by this Teensy project.
static const int RXPin = 21;
static const int TXPin = 20;
static const uint32_t GPSBaud = 9600;

// =========================
// Speed thresholds
// =========================
// Edit these three values when you want to tune the test behavior.
// STOP_SPEED_KMPH is slightly above zero so small GPS noise does not block arming.
// START_SPEED_KMPH is 1 km/h because starting at 0 is unreliable with GPS.
// FINISH_SPEED_KMPH is the target speed where timing stops.
static const float STOP_SPEED_KMPH = 0.5f;
static const float START_SPEED_KMPH = 1.0f;
static const float FINISH_SPEED_KMPH = 50.0f;

// These layout values make the screen easy to tune later.
// If you want to move text up/down, change the Y values here.
static const int SPEED_LABEL_Y = 0;
static const int SPEED_VALUE_Y = 12;
static const int STATUS_Y = 42;
static const int RESULT_LABEL_Y = 50;
static const int RESULT_VALUE_Y = 54;

// Text sizes are also grouped here so you can adjust the layout quickly.
static const uint8_t SPEED_VALUE_SIZE = 3;
static const uint8_t RESULT_VALUE_SIZE = 1;

// Small text buffers let the screen text use the configured target speed
// instead of hard-coded labels such as "50".
char finishSpeedLabel[12];
char readyStatusText[32];
char timingStatusText[32];
char lastResultLabel[24];

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
TinyGPSPlus gps;
SoftwareSerial ss(RXPin, TXPin);

// The timer only needs three states:
// 1. WAIT_FOR_STOP: wait until the vehicle is fully stopped so a new run is allowed
// 2. READY_TO_START: stopped and armed, waiting for speed to reach 1 km/h
// 3. TIMING: timing is active until 50 km/h is reached
enum RunState {
  WAIT_FOR_STOP,
  READY_TO_START,
  TIMING
};

RunState runState = WAIT_FOR_STOP;

// runStartMillis stores the exact moment the timer started.
unsigned long runStartMillis = 0;

// lastResultSeconds stores the most recent finished 1-50 result.
// hasResult lets us know whether there is a valid result to show.
float lastResultSeconds = 0.0f;
bool hasResult = false;

static void smartDelay(unsigned long ms);
static void updateStateMachine(float speedKmph, bool gpsValid);
static void updateDisplay(float speedKmph, bool gpsValid);
static void drawCenteredFlashText(const __FlashStringHelper *text, int16_t y, uint8_t size);
static void drawCenteredText(const char *text, int16_t y, uint8_t size);

void setup() {
  ss.begin(GPSBaud);
  Serial.begin(9600);

  // Rotation 2 matches the way your screen is physically mounted.
  display.setRotation(2);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }

  // Build user-facing labels once during setup so later display code stays simple.
  dtostrf(FINISH_SPEED_KMPH, 0, 0, finishSpeedLabel);
  snprintf(readyStatusText, sizeof(readyStatusText), "Ready to start");
  snprintf(timingStatusText, sizeof(timingStatusText), "Accelerate: 1 -> %s", finishSpeedLabel);
  snprintf(lastResultLabel, sizeof(lastResultLabel), "Last 1-%s", finishSpeedLabel);

  display.clearDisplay();
  drawCenteredFlashText(F("Speed Test"), 8, 2);
  drawCenteredFlashText(F("Waiting GPS"), 36, 1);
  display.display();

  // Give the GPS a little time to start sending data.
  smartDelay(1500);
}

void loop() {
  // Keep feeding TinyGPSPlus before reading the current speed.
  smartDelay(100);

  // Only trust the speed once both speed and location are valid.
  const bool gpsValid = gps.speed.isValid() && gps.location.isValid();
  const float speedKmph = gpsValid ? gps.speed.kmph() : 0.0f;

  updateStateMachine(speedKmph, gpsValid);
  updateDisplay(speedKmph, gpsValid);
}

// smartDelay() works like delay(), but still keeps parsing GPS characters.
// Without this, GPS updates can be missed while the sketch is "waiting".
static void smartDelay(unsigned long ms) {
  const unsigned long start = millis();
  do {
    while (ss.available()) {
      gps.encode(ss.read());
    }
  } while (millis() - start < ms);
}

// This function contains the measuring logic only.
// Keeping the state machine separate from the display makes the code easier to understand.
static void updateStateMachine(float speedKmph, bool gpsValid) {
  switch (runState) {
    case WAIT_FOR_STOP:
      // A new run is only allowed after the vehicle has fully stopped.
      if (gpsValid && speedKmph <= STOP_SPEED_KMPH) {
        runState = READY_TO_START;
        Serial.println(F("Vehicle stopped. Ready for next run."));
      }
      break;

    case READY_TO_START:
      if (!gpsValid) {
        break;
      }

      // Once armed, the timer begins automatically when speed reaches 1 km/h.
      if (speedKmph >= START_SPEED_KMPH) {
        runStartMillis = millis();
        runState = TIMING;
        Serial.println(F("Timing started at 1 km/h."));
      }
      break;

    case TIMING:
      if (!gpsValid) {
        break;
      }

      // Finish the run at the configured target speed and keep the result on screen.
      // The result is NOT cleared after stopping. It stays visible until a newer run replaces it.
      if (speedKmph >= FINISH_SPEED_KMPH) {
        lastResultSeconds = (millis() - runStartMillis) / 1000.0f;
        hasResult = true;
        runState = WAIT_FOR_STOP;

        Serial.print(F("Finished 1-"));
        Serial.print(FINISH_SPEED_KMPH, 0);
        Serial.print(F(" km/h in "));
        Serial.print(lastResultSeconds, 2);
        Serial.println(F(" s"));
      }
      break;
  }
}

// All screen drawing lives here.
// If you want to change the layout later, this is the main place to edit.
static void updateDisplay(float speedKmph, bool gpsValid) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Top section: live speed
  display.setTextSize(1);
  display.setCursor(0, SPEED_LABEL_Y);
  display.print(F("Speed"));

  display.setTextSize(SPEED_VALUE_SIZE);
  display.setCursor(0, SPEED_VALUE_Y);
  if (gpsValid) {
    display.print(speedKmph, 1);
  } else {
    display.print(F("--.-"));
  }

  display.setTextSize(1);
  display.print(F(" km/h"));

  // Middle section: current status
  display.setTextSize(1);
  display.setCursor(0, STATUS_Y);
  if (!gpsValid) {
    display.print(F("Waiting for GPS fix"));
  } else if (runState == WAIT_FOR_STOP) {
    display.print(F("Wait to stop"));
  } else if (runState == READY_TO_START) {
    display.print(readyStatusText);
  } else {
    display.print(timingStatusText);
  }

  // Bottom section: either the live timer or the last finished result.
  // This area stays visible after a run so you can still read the last time later.
  if (runState == TIMING) {
    char timerText[16];
    dtostrf((millis() - runStartMillis) / 1000.0f, 0, 2, timerText);

    display.setTextSize(1);
    display.setCursor(0, RESULT_LABEL_Y);
    display.print(F("Current"));

    drawCenteredText(timerText, RESULT_VALUE_Y, RESULT_VALUE_SIZE);
    display.print(F(" s"));
  } else if (hasResult) {
    char resultText[16];
    dtostrf(lastResultSeconds, 0, 2, resultText);

    display.setTextSize(1);
    display.setCursor(0, RESULT_LABEL_Y);
    display.print(lastResultLabel);

    drawCenteredText(resultText, RESULT_VALUE_Y, RESULT_VALUE_SIZE);
    display.print(F(" s"));
  }

  display.display();
}

// Helper to center text stored in flash memory, such as F("text").
static void drawCenteredFlashText(const __FlashStringHelper *text, int16_t y, uint8_t size) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

// Helper to center normal C strings such as the converted timer value.
static void drawCenteredText(const char *text, int16_t y, uint8_t size) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor(((SCREEN_WIDTH - w) / 2) + 10, y);
  display.print(text);
}
