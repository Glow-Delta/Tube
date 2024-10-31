// Aanpassingen:
// - ISR-methode.
// - Detach interrupt en attach interrupt verwijderd.
// - Handle and detach interrupt zijn verwijderd.
// - MySerial implementatie verwijderd. (dus 0 en 1 zijn de RX en TX pinnen).
// - Interrupt pins zijn nu 8, 9.

#include <Wire.h>
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
 
#define MCP23017_ADDRESS_A 0x20 // Default I2C address
#define MCP23017_ADDRESS_B 0x21 // Default I2C address
 
unsigned long lightTimer = millis();
 
// MCP23017 Register Addresses
#define IODIRA   0x00  // I/O direction register for Port A (1 = input, 0 = output)
#define IODIRB   0x01  // I/O direction register for Port B
#define IPOLA    0x02  // Input polarity register for Port A (1 = inverted, 0 = normal)
#define IPOLB    0x03  // Input polarity register for Port B
#define GPINTENA 0x04  // Interrupt-on-change control for Port A (1 = enable interrupt)
#define GPINTENB 0x05  // Interrupt-on-change control for Port B
#define DEFVALA  0x06  // Default compare register for interrupt on Port A
#define DEFVALB  0x07  // Default compare register for interrupt on Port B
#define INTCONA  0x08  // Interrupt control register for Port A (0 = previous, 1 = DEFVAL)
#define INTCONB  0x09  // Interrupt control register for Port B
#define IOCONA   0x0A  // I/O expander configuration register for Port A
#define IOCONB   0x0B  // I/O expander configuration register for Port B
#define GPPUA    0x0C  // Pull-up resistor configuration for Port A (1 = enable pull-up)
#define GPPUB    0x0D  // Pull-up resistor configuration for Port B
#define INTFA    0x0E  // Interrupt flag register for Port A (read-only, indicates the pin that caused the interrupt)
#define INTFB    0x0F  // Interrupt flag register for Port B
#define INTCAPA  0x10  // Interrupt capture register for Port A (state of the pin at the time of the interrupt)
#define INTCAPB  0x11  // Interrupt capture register for Port B
#define GPIOA    0x12  // General-purpose I/O register for Port A (read/write the state of the pins)
#define GPIOB    0x13  // General-purpose I/O register for Port B
#define OLATA    0x14  // Output latch register for Port A (read/write the output state)
#define OLATB    0x15  // Output latch register for Port B
 
#define INTERRUPT_A_PIN 8
#define INTERRUPT_B_PIN 9

#define SENSOR_COUNT_A 4
#define SENSOR_COUNT_B 4
 
volatile bool interuptedA;
unsigned long risingA = 0;
unsigned long travelTimeA = 0;
 
const uint8_t sensorPinsA[SENSOR_COUNT_A] = {0b00000001, 0b00000010, 0b00000100, 0b00001000};
 
int lastTriggeredSensorA = 0;
bool sensorTriggeredA = false;
unsigned long lastSensorTriggerTimeA = 0;
 
volatile bool interuptedB;
unsigned long risingB = 0;
unsigned long travelTimeB = 0;
 
const uint8_t sensorPinsB[SENSOR_COUNT_B] = {0b00000001, 0b00000010, 0b00000100, 0b00001000};
 
int lastTriggeredSensorB = 0;
bool sensorTriggeredB = false;
unsigned long lastSensorTriggerTimeB = 0;
 
unsigned long lastPrint = 0;

volatile bool interruptAState = digitalRead(INTERRUPT_A_PIN); // State for interrupt A
volatile bool interruptBState = digitalRead(INTERRUPT_B_PIN); // State for interrupt B
 
int distances[5][8];
 
void setup() {
 
  // Start serial communication for debugging
  Serial.begin(115200); 
  Serial1.begin(115200);

  Wire.begin();
  Wire.setClock(800000);  // Set I²C speed to 400kHz //Dit mogelijk nog aanpassen.

  while(!Serial){} //ERUIT HALEN WANNEER DE COMPUTER ER NIET MEER AAN HANGT!!!
 
  Serial.println("Setup started!");

  pinMode(INTERRUPT_A_PIN, INPUT_PULLUP);
  pinMode(INTERRUPT_B_PIN, INPUT_PULLUP);

  // NOTE PINCHANGE INTERRUPTS ARE ALWAYS INTERRUPT ON CHANGE
  PCICR  |= (1 << PCIE0);      // Enable pin change interrupt for Port B (PCINT0..7)
  PCMSK0 |= (1 << PCINT4);     // Enable pin change interrupt on PCINT4 (pin 8)
  PCMSK0 |= (1 << PCINT5);     // Enable pin change interrupt on PCINT5 (pin 9)
 
  setupMCP23017();
 
  interuptedA = false;
  interuptedB = false;
 
  leds = new CRGB[NUMPIXELS]; //An array of RBG led pixels.
  FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUMPIXELS);
}
 
void loop() {
  unsigned long now = micros();
 
  checkForInteruptsA(now);
  checkForInteruptsB(now);
  
  //TODO: handel the travel time instead of calculating the distance in cm.
  if (travelTimeA != 0) {
    int distanceCm = travelTimeA *  0.034 / 2;

    if (distanceCm < 250) {
      transfer(lastTriggeredSensorA, distanceCm);
    }
  }
 
  //TODO handel the travel time
  if (travelTimeB != 0) {
    int distanceCm = travelTimeB *  0.034 / 2;

    if (distanceCm < 250) {
      transfer(lastTriggeredSensorB + SENSOR_COUNT_A, distanceCm);
    }
  }
 
  if (now - lastPrint > 200000) {
    // Calculate smoothed average and print results (will be removed later)
    for (int i = 0; i < 8; i++) {
      int avgDistance = average(i);
      Serial.print(avgDistance);
      Serial.print(", ");
    }
    Serial.println(200);
  }
 
  int Amount = getAmountOfActivatedSensors();
    if (!interuptedA && !interuptedB) {
    send_serial(0, Amount);
  }
  if (Amount > 100) {
    Serial.println("Light ON");
    ActiveAnimation();
  } else {
    Serial.println("Light OFF");
    Heartbeat();
  }
}
 
void checkForInteruptsA(unsigned long now) {
  if (!sensorTriggeredA) {
    triggerNextUltrasonicA();
  }
  else if (interuptedA) {
    readRegisterA(INTCAPA);
    interuptedA = false;
 
    if (risingA == 0) {
      risingA = now;
    }
    else {
      travelTimeA = now - risingA;
      sensorTriggeredA = false;
    }
  }
  else if (now - lastSensorTriggerTimeA > 50000) {
    readRegisterA(INTCAPA);
    triggerNextUltrasonicA();
  }
}

void checkForInteruptsB(unsigned long now) {
  if (!sensorTriggeredB) {
    triggerNextUltrasonicB();
  }
  else if (interuptedB) {
    readRegisterB(INTCAPA);
    interuptedB = false;
 
    if (risingB == 0) {
      risingB = now;
    }
    else {
      travelTimeB = now - risingB;
      sensorTriggeredB = false;
    }
  }
  else if (now - lastSensorTriggerTimeB > 50000) {
    readRegisterB(INTCAPA);
    triggerNextUltrasonicB();
  }
}

// Function to configure MCP23017 registers for GPA7 (echo) and GPB0 (trigger)
void setupMCP23017() {
  // Set GPB0 as output (trigger) and GPA7 as input (echo)
  writeRegisterA(IODIRA, 0b11111111);  // IODIRA: 1 = input for GPA7 (echo)
  writeRegisterA(IODIRB, 0b00000000);  // IODIRB: 0 = output for GPB0 (trigger)
  writeRegisterB(IODIRA, 0b11111111);  // IODIRA: 1 = input for GPA7 (echo)
  writeRegisterB(IODIRB, 0b00000000);  // IODIRB: 0 = output for GPB0 (trigger)
 
  // Enable interrupt on GPA7 (echo pin)
  writeRegisterA(GPINTENA, 0b11111111);  // GPINTENA: Enable interrupt on GPA7 (echo)
  writeRegisterB(GPINTENA, 0b11111111);  // GPINTENA: Enable interrupt on GPA7 (echo)
  
  // Set interrupt-on-change to trigger on any change (both rising and falling edge) for GPA7
  writeRegisterA(INTCONA, 0b00000000);  // INTCONA: 0 = compare to previous value (interrupt on any change)
  writeRegisterB(INTCONA, 0b00000000);  // INTCONA: 0 = compare to previous value (interrupt on any change)
 
  writeRegisterA(DEFVALA, 0b11111111); // DEFVALA: 1 = high
  writeRegisterA(DEFVALB, 0b00000000); // DEFVALB: 0 = low
  writeRegisterB(DEFVALA, 0b11111111); // DEFVALA: 1 = high
  writeRegisterB(DEFVALB, 0b00000000); // DEFVALB: 0 = low
 
  // Enable pull-up resistor on GPA7 (echo)
  writeRegisterA(GPPUA, 0b11111111);  // GPPUA: 1 = pull-up enabled on GPA7 (echo)
  writeRegisterA(GPPUB, 0b11111111);  // GPPUB: 1 = pull-up enabled on GPA7 (echo)
  writeRegisterB(GPPUA, 0b11111111);  // GPPUA: 1 = pull-up enabled on GPA7 (echo)
  writeRegisterB(GPPUB, 0b11111111);  // GPPUB: 1 = pull-up enabled on GPA7 (echo)
  
  // Clear any pending interrupts (optional)
  readRegisterA(0x12);  // GPIOA: Read to clear any existing interrupt flags for Port A
  readRegisterB(0x12);  // GPIOA: Read to clear any existing interrupt flags for Port A
}
 
// Function to trigger the ultrasonic sensor for chip A
void triggerNextUltrasonicA() {
  //Serial.print("triggering sensor: ");
  //Serial.println(index);
  // Record the time the trigger pulse was sent
  risingA = 0;
  travelTimeA = 0;
  interuptedA = false;
  sensorTriggeredA = true;
  lastSensorTriggerTimeA = micros();
 
  int ultrasonicToTrigger = (lastTriggeredSensorA + 1) % SENSOR_COUNT_A;
  lastTriggeredSensorA = ultrasonicToTrigger;
 
  Wire.beginTransmission(MCP23017_ADDRESS_A);
  Wire.write(GPIOB);
  Wire.write(sensorPinsA[ultrasonicToTrigger]);
  
  delayMicroseconds(10);  // Delay for 10 microseconds
 
  Wire.write(GPIOB);
  Wire.write(0b00000000);
  Wire.endTransmission();
}

// Function to trigger the ultrasonic sensor for chip B
void triggerNextUltrasonicB() {
  //Serial.print("triggering sensor: ");
  //Serial.println(index);
  // Record the time the trigger pulse was sent
  risingB = 0;
  travelTimeB = 0;
  interuptedB = false;
  sensorTriggeredB = true;
  lastSensorTriggerTimeB = micros();
 
  int ultrasonicToTrigger = (lastTriggeredSensorB + 1) % SENSOR_COUNT_B;
  lastTriggeredSensorB = ultrasonicToTrigger;
 
  Wire.beginTransmission(MCP23017_ADDRESS_B);
  Wire.write(GPIOB);
  Wire.write(sensorPinsB[ultrasonicToTrigger]);
  
  delayMicroseconds(10);  // Delay for 10 microseconds
 
  Wire.write(GPIOB);
  Wire.write(0b00000000);
  Wire.endTransmission();
}
 
 
// Function to write to MCP23017 register
void writeRegisterA(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MCP23017_ADDRESS_A);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
// Function to write to MCP23017 register
void writeRegisterB(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MCP23017_ADDRESS_B);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
 
// Function to read from MCP23017 register
uint8_t readRegisterA(uint8_t reg) {
  Wire.beginTransmission(MCP23017_ADDRESS_A);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(MCP23017_ADDRESS_A, 1);
  return Wire.read();
}

// Function to read from MCP23017 register
uint8_t readRegisterB(uint8_t reg) {
  Wire.beginTransmission(MCP23017_ADDRESS_B);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(MCP23017_ADDRESS_B, 1);
  return Wire.read();
}
 
// Shifts previous distance measurements up and adds the new one
void transfer(int sensorIndex, int newDistance) {
  // Shift all rows up for the specific sensor
  for (int i = 0; i < 4; i++) {
    distances[i][sensorIndex] = distances[i + 1][sensorIndex];
  }
  // Add the new reading at the end
  distances[4][sensorIndex] = newDistance;
}
 
// Calculates the average distance for a specific sensor
int average(int sensorIndex) {
  int sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += distances[i][sensorIndex];
  }
  return sum / 5; // Return the average
}
 
int getAmountOfActivatedSensors() {
  int amountOfActivatedSensors = 0;
  
  for (int i = 0; i < 8; i++) {
    if (average(i) < 120 && average(i) > 0) {
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
  Serial1.println("Sending message");
  String message = "id=" + String(id); // Convert id to String
  message += ":activation_level=" + String(activation); // Convert activation to String
  message += '\n';
  Serial1.println(message);
  // mySerial.flush(); // Ensure the message is sent
  // TODO: implement attach interrupts.
}

ISR(PCINT0_vect) {
  // Check if there was a change on pin interruptA and interruptB pin.
  if (interruptAState != digitalRead(INTERRUPT_A_PIN)) {
      interruptAState = digitalRead(INTERRUPT_A_PIN);
      interuptedA = true;
  }

  if (interruptBState != digitalRead(INTERRUPT_B_PIN)) {
      interruptBState = digitalRead(INTERRUPT_B_PIN);
      interuptedB = true;
  }
}
