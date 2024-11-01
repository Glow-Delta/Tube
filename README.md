# Glow Backup

This project involves using ultrasonic sensors to detect touch events and control LED lighting patterns. The code is designed to work without interrupts, ensuring smooth and continuous operation.

## Key Components

- **Ultrasonic Sensors**: Used to detect the distance to objects and determine if a touch event has occurred.
- **LEDs**: Controlled using the FastLED library to display various lighting patterns based on sensor input.

## Sensor Configuration

The project uses ultrasonic sensors with a trigger and echo pin:

```cpp
const int trigPins[] = {22, 24, 26, 28, 30, 32, 34, 36};
const int echoPins[] = {46, 48, 50, 52, 38, 40, 42, 44};
const int numSensors = 8;
const int numReadings = 2;
const int touchThreshold = 175; // cm
```
* **trigPins**: Pins used to trigger the ultrasonic sensors.
* **echoPins**: Pins used to receive the echo signal from the sensors.
* **numSensors**: Total number of sensors used.
* **numReadings**: Number of readings taken for each sensor to calculate an average distance.
* **touchThreshold**: Distance threshold (in cm) to determine if a touch event has occurred.

> The sensor configuration can be modified based on the number of sensors used and the pin assignments. 

> The light animation has not been tested yet on this backup.
