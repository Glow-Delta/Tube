#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    461
#define MAX_ACTIONS 10
#define FPS         30
#define FRAME_DELAY (1000 / FPS)
#define TUBE_ID 1

CRGB leds[NUM_LEDS];
unsigned long lastFrameTime = 0;

int tubeState = 1;
int activeSensors = 0;

unsigned long lastActiveSensorChange = 0;

// Transition variables
uint8_t currentHue = 160;  // Initial hue for idle (blue)
uint8_t targetHue = 160;
uint8_t hueTransitionSpeed = 3;  // Speed of hue transition, lower is slower

// Struct for each light action
struct LightAction {
    int duration;
    int minBrightness;
    int maxBrightness;
    int totalFrames;
    int currentFrame;
    bool active;
    uint8_t hue;  // Add a hue member to the struct

    LightAction() : duration(0), minBrightness(0), maxBrightness(0), totalFrames(0), currentFrame(0), active(false), hue(0) {}

    void initialize(int dur, int minB, int maxB, uint8_t h) {
        duration = dur;
        minBrightness = minB;
        maxBrightness = maxB;
        totalFrames = (duration / FRAME_DELAY);
        currentFrame = 0;
        active = true;
        hue = h; // Set the hue for this action
    }

    int getCurrentBrightness() {
        float progress = float(currentFrame) / totalFrames;
        return minBrightness + (sin(progress * PI) * (maxBrightness - minBrightness));
    }

    void advanceFrame() {
        if (currentFrame < totalFrames) {
            currentFrame++;
        } else {
            active = false;
        }
    }
};

// Queue for light actions
struct LightQueue {
    LightAction actions[MAX_ACTIONS];
    int front;
    int rear;
    int size;

    LightQueue() : front(0), rear(-1), size(0) {}

    bool enqueue(LightAction action) {
        if (size == MAX_ACTIONS) {
            return false;
        }
        rear = (rear + 1) % MAX_ACTIONS;
        actions[rear] = action;
        size++;
        return true;
    }

    bool dequeue() {
        if (size == 0) {
            return false;
        }
        front = (front + 1) % MAX_ACTIONS;
        size--;
        return true;
    }

    LightAction* peek() {
        if (size == 0) {
            return nullptr;
        }
        return &actions[front];
    }
};

LightQueue lightQueue;

// Add pulse actions for a breathing or pulsing effect with hue
void addPulseAction(int duration, int minBrightness, int maxBrightness, uint8_t hue) {
    LightAction action;
    action.initialize(duration, minBrightness, maxBrightness, hue); // Pass hue
    lightQueue.enqueue(action);
}

// Define animations for each state with hue
void idleAnimation() {
    addPulseAction(1000, 100, 25, 160); // Blue hue
    //addPulseAction(1000, 112, 125, 145); // Blue hue
}

void lowActivityAnimation() {
    addPulseAction(750, 125, 150, 96); // Green hue
    //addPulseAction(750, 137, 150, 111); // Green hue
}

void moderateActivityAnimation() {
    addPulseAction(500, 150, 200, 32); // Yellow hue
    //addPulseAction(500, 175, 200, 62); // Yellow hue
}

void highActivityAnimation() {
    addPulseAction(500, 200, 255, 0); // Red hue
}

// Function to update tube state based on active sensors with hysteresis
void updateTubeState() {
    switch (tubeState) {
        case 0: // Idle state
            if (activeSensors >= 4) {
                tubeState = 1; // Switch to Low activity
            }
            break;

        case 1: // Low activity state
            if (activeSensors <= 2) {
                tubeState = 0; // Back to Idle
            } else if (activeSensors >= 8) {
                tubeState = 2; // Switch to Moderate activity
            }
            break;

        case 2: // Moderate activity state
            if (activeSensors <= 5) {
                tubeState = 1; // Back to Low activity
            } else if (activeSensors >= 13) {
                tubeState = 3; // Switch to High activity
            }
            break;

        case 3: // High activity state
            if (activeSensors <= 10) {
                tubeState = 2; // Back to Moderate activity
            }
            break;
    }
}


// Function to set animation based on tube state
void setAnimationForState() {
    lightQueue = LightQueue();  // Clear existing actions

    switch (tubeState) {
        case 0:
            idleAnimation();
            break;
        case 1:
            lowActivityAnimation();
            break;
        case 2:
            moderateActivityAnimation();
            break;
        case 3:
            highActivityAnimation();
            break;
    }
}

// Smooth transition of hue towards target
void updateCurrentHue(uint8_t targetHue) {
    if (currentHue < targetHue) {
        currentHue = min(currentHue + hueTransitionSpeed, targetHue);
    } else if (currentHue > targetHue) {
        currentHue = max(currentHue - hueTransitionSpeed, targetHue);
    }
}

// Function to process the current animation frame
void processCurrentAction(LightAction* action) {
    int brightness = action->getCurrentBrightness();

    // Gradually transition to the target color
    updateCurrentHue(action->hue); // Use the action's hue

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(currentHue, 255, brightness);
    }
    FastLED.show();

    action->advanceFrame();
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  sensorSetupMethod();

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  
  setAnimationForState();  // Initialize with the first animation
}

void loop() {

  int previousState = tubeState;
  sensorLoopMethod();
  
  unsigned long currentMillis = millis();

  if (tubeState != previousState) {
      setAnimationForState();  // Change animation if state has changed
  }

  if (currentMillis - lastFrameTime >= FRAME_DELAY) {
      lastFrameTime = currentMillis;

      LightAction* currentAction = lightQueue.peek();
      if (currentAction != nullptr && currentAction->active) {
          processCurrentAction(currentAction);

          if (!currentAction->active) {
              lightQueue.dequeue();
          }
      } else {
          setAnimationForState();
      }
  } 
}


void send_serial(int activation) {
  Serial.println("Sending message");
 
  // Construct the message to send
  String message = "id=" + String(TUBE_ID); // Convert id to String
  message += ":activation_level=" + String(activation); // Convert activation to String
  message += '\n'; // Add a newline character to signify the end of the message
 
  Serial.println(message); // Print the message to the Serial Monitor for debugging
 
  // Send the message over Serial1
  Serial1.print(message); // Use print() or println() to send the message
  // Note: If you use print(), remember the Teensy will need to read until '\n' if you're using readStringUntil
 
  // Optional: flush to ensure all data is sent
  Serial1.flush(); // Wait until all data is sent
}
