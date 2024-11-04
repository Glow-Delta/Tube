// Aanpassingen:
// - ISR-methode.
// - Detach interrupt en attach interrupt verwijderd.
// - Handle and detach interrupt zijn verwijderd.
// - MySerial implementatie verwijderd. (dus 0 en 1 zijn de RX en TX pinnen).
// - Interrupt pins zijn nu 8, 9.

#include <FastLED.h>
 
#define LEDPIN 5
#define NUMPIXELS 461
#define FASTLED_ALLOW_INTERRUPTS 0
 
CRGB* leds;
byte greenIntensity = 255;

int currentSensorCount = 0;
int previousCount = 0;

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
  Serial1.begin(9600);

  sensorSetupMethod();

  while(!Serial){} //ERUIT HALEN WANNEER DE COMPUTER ER NIET MEER AAN HANGT!!!
 
  Serial.println("Setup started!");
 
  leds = new CRGB[NUMPIXELS]; //An array of RBG led pixels.
  FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUMPIXELS);
}
 
void loop() {
  sensorLoopMethod();

  // previousCount = currentSensorCount;
  // currentSensorCount = getAmountOfActivatedSensors();

  //moved the sendserial method to the sensorLogicHeader and added a parameter to the send frequency, 
  // beware that the more frequent you send data the less accurate the sensor values will be
}
 

 
void Red() {
  
  if ((millis() - lightTimer) > 33) {
    //Serial.println("RED");
    fill_solid(leds, NUMPIXELS, CRGB::Red);
    FastLED.show();
    lightTimer = millis();
  }
  
}
 
void Blue() {
  
  if ((millis() - lightTimer) > 33) {
    //Serial.println("BLUE");
    fill_solid(leds, NUMPIXELS, CRGB::Blue);
    FastLED.show();
    lightTimer = millis();
  }
}
 
void send_serial(int id, int activation) {
  Serial.println("Sending message");
 
  // Construct the message to send
  String message = "id=" + String(id); // Convert id to String
  message += ":activation_level=" + String(activation); // Convert activation to String
  message += '\n'; // Add a newline character to signify the end of the message
 
  Serial.println(message); // Print the message to the Serial Monitor for debugging
 
  // Send the message over Serial1
  Serial1.print(message); // Use print() or println() to send the message
  // Note: If you use print(), remember the Teensy will need to read until '\n' if you're using readStringUntil
 
  // Optional: flush to ensure all data is sent
  Serial1.flush(); // Wait until all data is sent
}
