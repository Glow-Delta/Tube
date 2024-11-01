#include <FastLED.h>

#define LEDPIN        6
#define NUMPIXELS     461
#define FASTLED_ALLOW_INTERRUPTS 0

CRGB leds[NUMPIXELS];
byte greenIntensity = 255;

struct Section {
  int start;
  int end;
};

Section sections[] = {
  {0, 116},
  {117, 231},
  {232, 345},
  {346, 460}
};

const int trigPins[] = {22, 24, 26, 28, 30, 32, 34, 36};
const int echoPins[] = {46, 48, 50, 52, 38, 40, 42, 44};
const int numSensors = 8;
const int numReadings = 2;
const int touchThreshold = 175; // cm

long distances[numReadings][numSensors] = {0};
bool wasRedBeams = false;
unsigned long lightTimer = millis();
int currentSensorCount = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);

  FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUMPIXELS);
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < numSensors; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
  }
}

void loop() {
  int previousSensorCount = currentSensorCount;
  currentSensorCount = readSensors();

  if (currentSensorCount == 0) {
    IdleAnimation(previousSensorCount);
  } else {
    ActiveAnimation(previousSensorCount);
  }

  send_serial(0, currentSensorCount);
}

int readSensors() {
  for (int reading = 0; reading < numReadings; reading++) {
    for (int sensor = 0; sensor < numSensors; sensor++) {
      long duration, distance;

      digitalWrite(trigPins[sensor], LOW);
      delayMicroseconds(2);
      digitalWrite(trigPins[sensor], HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPins[sensor], LOW);

      duration = pulseIn(echoPins[sensor], HIGH);
      distance = duration * 0.034 / 2;

      if (distance < 500) {
        distances[reading][sensor] = distance;
      } else {
        distances[reading][sensor] = 500;
      }

      delay(3); // Required delay
    }
    delay(5); // Required delay
  }

  int touchedCount = 0;
  for (int sensor = 0; sensor < numSensors; sensor++) {
    long sum = 0;
    for (int reading = 0; reading < numReadings; reading++) {
      sum += distances[reading][sensor];
    }
    long average = sum / numReadings;
    if (average <= touchThreshold) {
      touchedCount++;
    }
  }

  return touchedCount;
}

void IdleAnimation(int previousCount) {
  if ((millis() - lightTimer) > 33) {
    static byte fade = 0;
    static byte up = 1;

    fade += (up ? 5 : -5);

    if (fade >= 255 || fade <= 0) {
      up = (up == 1) ? 0 : 1;
    }

    CRGB idleColor = CRGB(fade, fade, 255);

    if (currentSensorCount != previousCount) {
      for (int i = 0; i < NUMPIXELS; i++) {
        leds[i] = blend(leds[i], idleColor, 10);
      }
    } else {
      fill_solid(leds, NUMPIXELS, idleColor);
    }

    FastLED.show();
    lightTimer = millis();
  }
}

void ActiveAnimation(int previousCount) {
  if ((millis() - lightTimer) > 33) {
    static int shift = 0;
    static unsigned long lastUpdate = 0;
    CRGB colors[3] = { CRGB(0, 255, 255), CRGB(252, 3, 94), CRGB(255, 163, 3) };

    for (int s = 0; s < 4; s++) {
      int sectionLength = sections[s].end - sections[s].start + 1;

      for (int pos = sectionLength - 1; pos >= 0; pos--) {
        int ledIndex = sections[s].end - pos;

        int gradientPos = map((pos + shift) % sectionLength, 0, sectionLength - 1, 0, 255);

        CRGB color;
        if (gradientPos < 128) {
          color = blend(colors[0], colors[1], gradientPos * 2);
        } else {
          color = blend(colors[1], colors[2], (gradientPos - 128) * 2);
        }

        if (currentSensorCount != previousCount) {
          leds[ledIndex] = blend(leds[ledIndex], color, 45);
        } else {
          leds[ledIndex] = color;
        }
      }
    }
    FastLED.show();
    shift = (shift + 2) % sections[0].end;
    lightTimer = millis();
  }
}

void send_serial(int id, int activation) {
  Serial.println("Sending message");
  String message = "id=" + String(id);
  message += ":activation_level=" + String(activation);
  message += '\n';
  Serial.println(message);
  Serial1.println(message);
}