// Aanpassingen:
// - ISR-methode.
// - Detach interrupt en attach interrupt verwijderd.
// - Handle and detach interrupt zijn verwijderd.
// - MySerial implementatie verwijderd. (dus 0 en 1 zijn de RX en TX pinnen).
// - Interrupt pins zijn nu 8, 9.

#include <FastLED.h>
 
#define LEDPIN        6
#define NUMPIXELS  461
#define FASTLED_ALLOW_INTERRUPTS 0
 
CRGB* leds;
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
 
unsigned long lightTimer = millis();
 
void setup() {
 
  // Start serial communication for debugging
  Serial.begin(115200); 
  Serial1.begin(115200);

  sensorSetupMethod();

  while(!Serial){} //ERUIT HALEN WANNEER DE COMPUTER ER NIET MEER AAN HANGT!!!
 
  Serial.println("Setup started!");
 
  leds = new CRGB[NUMPIXELS]; //An array of RBG led pixels.
  FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUMPIXELS);
}
 
void loop() {
  sensorLoopMethod();
 
  int Amount = getAmountOfActivatedSensors();
    if (!interuptedA && !interuptedB) {
    send_serial(0, Amount);
  }
  //light goes on if one sensor detects a user
  if (Amount >= 1) {
    Serial.println("Light ON");
    ActiveAnimation();
  } else {
    Serial.println("Light OFF");
    Heartbeat();
  }
}
 
int getAmountOfActivatedSensors() {
  int amountOfActivatedSensors = 0;
 
  for (int i = 0; i < totalSensorCount; i++) {
    if (distances(i) < 200 && distances(i) > 0) {
      amountOfActivatedSensors++;
    }
  }
  return amountOfActivatedSensors;
}
 
void Heartbeat() {
  if((millis() - lightTimer) > 33) {
    static byte fade = 0;
    static byte up = 1;
 
    fade += (up ? 5 : -5);
 
    if (fade >= 255 || fade <= 0) {
      up = (up == 1) ? 0 : 1;
    }
    fill_solid(leds, NUMPIXELS, CRGB(fade, fade, 255));
    FastLED.show();
    lightTimer = millis();
  }
}
 
void ActiveAnimation() {
  //if((millis() - lightTimer) > 33) {
  if((millis() - lightTimer) > 10) {    // PLAY WITH THIS SETTNG
    // fill_solid(leds, NUMPIXELS, CRGB(0, 0, 255));
    // int maxStep = (sections[0].end - sections[0].start) / 2;

    greenIntensity = max(greenIntensity - 85, 0);
    static int step = (sections[0].end - sections[0].start) / 2;
    //for (int step = maxStep; step >= 0; step--) {
      for (int s = 0; s < 4; s++) {
        leds[sections[s].start + step] = CRGB(255, greenIntensity, 0);
        leds[sections[s].end - step] = CRGB(255, greenIntensity, 0);
      }
      FastLED.show();
    step--;
    if (step == 0) {
         step = (sections[0].end - sections[0].start) / 2;
    }
    lightTimer = millis();
 
    if (greenIntensity == 0) {
      greenIntensity = 255;
    }
  }
}
 
void send_serial(int id, int activation) {
  // TODO: implement detach interrupts.
  Serial.println("Sending message");
  String message = "id=" + String(id); // Convert id to String
  message += ":activation_level=" + String(activation); // Convert activation to String
  message += '\n';
  Serial.println(message);
  char dataToSend = Serial.read();
  Serial1.println(dataToSend);
  // mySerial.flush(); // Ensure the message is sent
  // TODO: implement attach interrupts.
}
