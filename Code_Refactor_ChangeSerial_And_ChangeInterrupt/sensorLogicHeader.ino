#include <Wire.h>

#define MCP23017_ADDRESS_A 0x20 // Default I2C address
#define MCP23017_ADDRESS_B 0x21 // Default I2C address

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

//volatile bool intARecieved = false;

#define SENSOR_COUNT_A 8
#define SENSOR_COUNT_B 8



#define TIMEOUT_TIME 50000 //in microseconds when to timeout a sensor and continue to the next

const int totalSensorCount = SENSOR_COUNT_A + SENSOR_COUNT_B;

volatile unsigned long interuptTime = 0;
unsigned long rising = 0;
unsigned long lastSensorTriggerTime = 0;
int lastTriggeredSensor = 0;
bool sensorTriggered = false;

//debug only
unsigned long lastSerialCom = 0;
#define SERIAL_COM_DELAY_MS 500
int activeSensorsTemp = 0;

const uint8_t sensorPins[SENSOR_COUNT_A] = {0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000, 0b01000000, 0b10000000};

int distances[SENSOR_COUNT_A + SENSOR_COUNT_B];

void sensorSetupMethod() {

  Wire.begin();
  Wire.setClock(400000);  // Set I²C speed to 400kHz

  pinMode(7, INPUT_PULLUP);     // Pro Micro pin 7 for INTA (interrupt pin), with internal pull-up

  setupMCP23017();

  attachInterrupt(digitalPinToInterrupt(7), handleInterrupt, CHANGE);  // Trigger on CHANGE (INTA goes LOW or HIGH)
}

void sensorLoopMethod() {

  handleSensorLogic();

  getAmountOfActivatedSensors();

  updateTubeState();

  if (millis() - lastSerialCom > SERIAL_COM_DELAY_MS) {
    //send_serial(tubeState);
    lastSerialCom = millis();
  }
}

// Function to configure MCP23017 registers for GPA7 (echo) and GPB0 (trigger)
void setupMCP23017() {
  // Set GPB0 as output (trigger) and GPA7 as input (echo)
  writeRegister(IODIRA, 0b11111111, MCP23017_ADDRESS_A);  // IODIRA: 1 = input for GPA7 (echo)
  writeRegister(IODIRB, 0b00000000, MCP23017_ADDRESS_A);  // IODIRB: 0 = output for GPB0 (trigger)
  writeRegister(IODIRA, 0b11111111, MCP23017_ADDRESS_B);  // IODIRA: 1 = input for GPA7 (echo)
  writeRegister(IODIRB, 0b00000000, MCP23017_ADDRESS_B);  // IODIRB: 0 = output for GPB0 (trigger)

  // Enable interrupt on GPA7 (echo pin)
  writeRegister(GPINTENA, 0b11111111, MCP23017_ADDRESS_A);  // GPINTENA: Enable interrupt on GPA7 (echo)
  writeRegister(GPINTENA, 0b11111111, MCP23017_ADDRESS_B);  // GPINTENA: Enable interrupt on GPA7 (echo)
  
  // Set interrupt-on-change to trigger on any change (both rising and falling edge) for GPA7
  writeRegister(INTCONA, 0b00000000, MCP23017_ADDRESS_A);  // INTCONA: 0 = compare to previous value (interrupt on any change)
  writeRegister(INTCONA, 0b00000000, MCP23017_ADDRESS_B);  // INTCONA: 0 = compare to previous value (interrupt on any change)

  writeRegister(DEFVALA, 0b11111111, MCP23017_ADDRESS_A); // DEFVALA: 1 = high
  writeRegister(DEFVALB, 0b00000000, MCP23017_ADDRESS_A); // DEFVALB: 0 = low
  writeRegister(DEFVALA, 0b11111111, MCP23017_ADDRESS_B); // DEFVALA: 1 = high
  writeRegister(DEFVALB, 0b00000000, MCP23017_ADDRESS_B); // DEFVALB: 0 = low

  // Enable pull-up resistor on GPA7 (echo)
  writeRegister(GPPUA, 0b11111111, MCP23017_ADDRESS_A);  // GPPUA: 1 = pull-up enabled on GPA7 (echo)
  writeRegister(GPPUB, 0b11111111, MCP23017_ADDRESS_A);  // GPPUB: 1 = pull-up enabled on GPA7 (echo)
  writeRegister(GPPUA, 0b11111111, MCP23017_ADDRESS_B);  // GPPUA: 1 = pull-up enabled on GPA7 (echo)
  writeRegister(GPPUB, 0b11111111, MCP23017_ADDRESS_B);  // GPPUB: 1 = pull-up enabled on GPA7 (echo)
  
  // Clear any pending interrupts (optional)
  readRegister(GPIOA, MCP23017_ADDRESS_A);  // GPIOA: Read to clear any existing interrupt flags for Port A
  readRegister(GPIOA, MCP23017_ADDRESS_B);  // GPIOA: Read to clear any existing interrupt flags for Port A
}

void handleSensorLogic() {
  if (!sensorTriggered) {
    readRegister(GPIOA, MCP23017_ADDRESS_A);  // GPIOA: Read to clear any existing interrupt flags for Port A
    readRegister(GPIOA, MCP23017_ADDRESS_B);  // GPIOA: Read to clear any existing interrupt flags for Port A

    triggerNextUltrasonic();
  }
  else if (interuptTime != 0) {
    if (rising == 0) {
      readRegister(GPIOA, MCP23017_ADDRESS_A);  // GPIOA: Read to clear any existing interrupt flags for Port A
      readRegister(GPIOA, MCP23017_ADDRESS_B);  // GPIOA: Read to clear any existing interrupt flags for Port A

      rising = interuptTime;
      interuptTime = 0;
    }
    else {
      readRegister(GPIOA, MCP23017_ADDRESS_A);  // GPIOA: Read to clear any existing interrupt flags for Port A
      readRegister(GPIOA, MCP23017_ADDRESS_B);  // GPIOA: Read to clear any existing interrupt flags for Port A

      long distanceCm = (interuptTime - rising) *  0.017;

      if (distanceCm < 250) {
        distances[lastTriggeredSensor] = distanceCm;
      }

      sensorTriggered = false;
    }
  }
  else if (micros() - lastSensorTriggerTime > TIMEOUT_TIME) {
    readRegister(GPIOA, MCP23017_ADDRESS_A);  // GPIOA: Read to clear any existing interrupt flags for Port A
    readRegister(GPIOA, MCP23017_ADDRESS_B);  // GPIOA: Read to clear any existing interrupt flags for Port A

    triggerNextUltrasonic();
  }
}

// Function to trigger the ultrasonic sensor for chip A
void triggerNextUltrasonic() {
  //Serial.print("triggering sensor: ");
  //Serial.println(index);
  // Record the time the trigger pulse was sent
  rising = 0;
  interuptTime = 0;
  sensorTriggered = true;

  int nextSensor = (lastTriggeredSensor + 1) % totalSensorCount;
  uint8_t address = MCP23017_ADDRESS_A;
  int index = nextSensor;

  if (nextSensor >= SENSOR_COUNT_A) {
    address = MCP23017_ADDRESS_B;
    index = nextSensor - SENSOR_COUNT_A;
  }

  // Serial.print(nextSensor);
  // Serial.print(", ");
  // Serial.println(index);


  Wire.beginTransmission(address);
  Wire.write(GPIOB);
  Wire.write(sensorPins[index]);
  
  delayMicroseconds(10);  // Delay for 10 microseconds

  Wire.write(GPIOB);
  Wire.write(0b00000000);
  Wire.endTransmission();

  lastSensorTriggerTime = micros();
  lastTriggeredSensor = nextSensor;
}

// Function to write to MCP23017 register
void writeRegister(uint8_t reg, uint8_t value, int address) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}


// Function to read from MCP23017 register
uint8_t readRegister(uint8_t reg, int address) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(address, 1);
  return Wire.read();
}

// Function that will be called when an interrupt occurs
void handleInterrupt() {
  //Serial.println("interupt");
  interuptTime = micros();
}

void printDistances(int distances[]) {
  int size = SENSOR_COUNT_A + SENSOR_COUNT_B;
  Serial.print("[");
  for (int i = 0; i < size; i++) {
    Serial.print(distances[i]);
    if (i < size - 1) {
      Serial.print(", ");  // Add a comma and space between values
    }
  }
  Serial.println("]");  // Close the array with a bracket and move to the next line
}

int getAmountOfActivatedSensors() {
  //temporarily return a random number of active sensors
  if (millis() - lastActiveSensorChange > 5000) {
    lastActiveSensorChange = millis();
    activeSensors = random(0, 17);
    updateTubeState();

    Serial.print("Active sensors: ");
    Serial.println(activeSensors);

    Serial.print("State: ");
    Serial.println(tubeState);
  }

  return activeSensors;

  int amountOfActivatedSensors = 0;
 
  for (int i = 0; i < totalSensorCount; i++) {
    if (distances[i] < 250 && distances[i] > 0) {
      amountOfActivatedSensors++;
    }
  }
  return amountOfActivatedSensors;
}

















