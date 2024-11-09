#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    461
#define MAX_ACTIONS 10
#define FPS         30
#define FRAME_DELAY (1000 / FPS)
#define TUBE_ID 5

CRGB leds[NUM_LEDS];
unsigned long lastFrameTime = 0;

int tubeState = 1;
int activeSensors = 0;

unsigned long lastActiveSensorChange = 0;

struct BrightnessHuePair {
  int brightness;
  int hue;
};

int currentBrightness = 0;
int currentHue = 0;

// Struct for each light action
struct LightAction {
    int duration;
    int beginBrightness;
    int endBrightness;
    uint8_t beginHue;
    uint8_t endHue;
    int totalFrames;
    int currentFrame;
    bool active;

    LightAction() : duration(0), beginBrightness(0), endBrightness(0), beginHue(0), endHue(0), totalFrames(0), currentFrame(0), active(false) {}

    void initialize(int dur, int beginB, int endB, uint8_t beginH, uint8_t endH) {
        duration = dur;
        beginBrightness = beginB;
        endBrightness = endB;
        beginHue = beginH;
        endHue = endH;
        totalFrames = (duration / FRAME_DELAY);
        currentFrame = 0;
        active = true;
    }

    // Function to get current brightness and hue
    BrightnessHuePair getCurrentBrightnessAndHue() {
    float progress = float(currentFrame) / totalFrames;

    int brightness = getCos(beginBrightness, endBrightness, progress);
    int hue = getCos(beginHue, endHue, progress);

    return {brightness, hue};
}

    int getCos(int begin, int end, float p) {
      float d = begin - end;          // Calculate d as (begin - end)
      float c = begin + end;          // Calculate c as (begin + end)

      return (cos(PI * p) * d) / 2 + 0.5 * c;   // Calculate y using the formula
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
void addLightAction(int duration, int minBrightness, int maxBrightness, uint8_t beginHue, uint8_t endHue) {
    LightAction action1;
    LightAction action2;
    action1.initialize(duration, minBrightness, maxBrightness, beginHue, endHue);
    action2.initialize(duration, maxBrightness, minBrightness, endHue, beginHue);
    lightQueue.enqueue(action1);
    lightQueue.enqueue(action2);
}

// Define animations for each state with hue
void idleAnimation() {
    addLightAction(1500, 100, 130, 184, 139); // Blue hue
}

void lowActivityAnimation() {
    addLightAction(1000, 130, 180, 139, 64); // to yellow hue
}

void moderateActivityAnimation() {
    addLightAction(750, 180, 215, 64, 220); // to pink hue
}

void highActivityAnimation() {
    addLightAction(500, 215, 255, 220, 130); // to cyan hue
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


void setAnimationForState(int startBrightness = 0, int startHue = 0, bool useTransition = false) {
    lightQueue = LightQueue();  // Clear existing actions

    int targetBrightness = 0;
    int targetHue = 0;

    // Determine the target values based on the next tube state
    switch (tubeState) {
        case 0:
            targetBrightness = 100; // starting brightness for idle animation
            targetHue = 184;        // starting hue for idle animation
            break;
        case 1:
            targetBrightness = 130; // starting brightness for low activity animation
            targetHue = 139;        // starting hue for low activity animation
            break;
        case 2:
            targetBrightness = 180; // starting brightness for moderate activity animation
            targetHue = 90;         // starting hue for moderate activity animation
            break;
        case 3:
            targetBrightness = 215; // starting brightness for high activity animation
            targetHue = 25;         // starting hue for high activity animation
            break;
    }

    // Calculate a dynamic duration based on the brightness and hue changes
    int brightnessDifference = abs(currentBrightness - targetBrightness);
    int hueDifference = abs(currentHue - targetHue);

    int transitionDuration = ((brightnessDifference + hueDifference) * 5);  // Scaling factor for smoothness
    //Serial.print("transitionDelay: ");
    //Serial.println(transitionDuration);

    if (useTransition) {
        // Add transition action with calculated dynamic duration
        LightAction transitionAction;
        transitionAction.initialize(transitionDuration, currentBrightness, targetBrightness, currentHue, targetHue);
        lightQueue.enqueue(transitionAction);
    }

    // Now enqueue the animation for the current state after the transition
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



// Function to process the current animation frame
void processCurrentAction(LightAction* action) {
  BrightnessHuePair BHPair = action->getCurrentBrightnessAndHue();

  for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(BHPair.hue, 255, BHPair.brightness);
  }

  

  currentBrightness = BHPair.brightness;
  currentHue = BHPair.hue;

  // Serial.print(tubeState);
  // Serial.print(", ");
  // Serial.print(currentHue);
  // Serial.print(", ");
  // Serial.println(currentBrightness);
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

  // If the state has changed, we add a transition
  if (tubeState != previousState) {
      int startBrightness = currentBrightness;
      int startHue = currentHue;
      setAnimationForState(startBrightness, startHue, true);  // Start a smooth transition to the new animation
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
  message += ":activation_level="; // Convert activation to String
  message += String(activation);
  message += '\n'; // Add a newline character to signify the end of the message
 
  Serial.println(message); // Print the message to the Serial Monitor for debugging
 
  // Send the message over Serial1
  Serial1.print(message); // Use print() or println() to send the message
  // Note: If you use print(), remember the Teensy will need to read until '\n' if you're using readStringUntil
 
  // Optional: flush to ensure all data is sent
  Serial1.flush(); // Wait until all data is sent
}
