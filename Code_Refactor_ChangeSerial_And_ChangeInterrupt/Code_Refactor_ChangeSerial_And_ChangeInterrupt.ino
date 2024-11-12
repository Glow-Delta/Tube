#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    461
#define MAX_ACTIONS 10
#define FPS         30
#define FRAME_DELAY (1000 / FPS)
#define TUBE_ID 2

CRGB leds[NUM_LEDS];
unsigned long lastFrameTime = 0;

int tubeState = 1;
int activeSensors = 0;

unsigned long lastActiveSensorChange = 0;

struct BrightnessHuePair {
  int brightness; // Single brightness value
  int hue[3];     // Array to hold hue for each color
};

int currentBrightness = 0; // Single brightness value for current state
uint8_t currentHue[3] = {0};   // Array to store current hue values for each hue

// Struct for each light action
struct LightAction {
    int duration;
    int beginBrightness;
    int endBrightness;
    uint8_t beginHue[3];
    uint8_t endHue[3];    
    int totalFrames;
    int currentFrame;
    bool active;

    LightAction() : duration(0), beginBrightness(0), endBrightness(0), totalFrames(0), currentFrame(0), active(false) {
        for (int i = 0; i < 3; i++) {
            beginHue[i] = 0;
            endHue[i] = 0;
        }
    }

    void initialize(int dur, int beginB, int endB, uint8_t beginH[3], uint8_t endH[3]) {
        duration = dur;
        beginBrightness = beginB;
        endBrightness = endB;
        for (int i = 0; i < 3; i++) {
            beginHue[i] = beginH[i];
            endHue[i] = endH[i];
        }        totalFrames = (duration / FRAME_DELAY);
        currentFrame = 0;
        active = true;
    }

    // Function to get current brightness and hue
    BrightnessHuePair getCurrentBrightnessAndHue() {
      float progress = float(currentFrame) / totalFrames;

      BrightnessHuePair BHPair; // Declare an instance of BrightnessHuePair
      BHPair.brightness = getCos(beginBrightness, endBrightness, progress); // Set brightness

      // Loop to calculate each hue in the array
      for (int i = 0; i < 3; i++) {
          BHPair.hue[i] = getCos(beginHue[i], endHue[i], progress);
      }

      return BHPair;
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
void addLightAction(int duration, int minBrightness, int maxBrightness, uint8_t beginHue[], uint8_t endHue[]) {
    LightAction action1;
    LightAction action2;
    action1.initialize(duration, minBrightness, maxBrightness, beginHue, endHue);
    action2.initialize(duration, maxBrightness, minBrightness, endHue, beginHue);
    lightQueue.enqueue(action1);
    lightQueue.enqueue(action2);
}

// Define animations for each state with hue
void idleAnimation() {
    uint8_t beginHue[3] = {184, 184, 184};
    uint8_t endHue[3] = {139, 139, 139};
    addLightAction(1500, 100, 130, beginHue, endHue);
}

void lowActivityAnimation() {
    uint8_t beginHue[3] = {139, 139, 139};
    uint8_t endHue[3] = {90, 90, 90};
    addLightAction(1500, 100, 130, beginHue, endHue);
}

void moderateActivityAnimation() {
    uint8_t beginHue[3] = {90, 90, 90};
    uint8_t endHue[3] = {25, 25, 25};
    addLightAction(1500, 100, 130, beginHue, endHue);
}

void highActivityAnimation() {
    uint8_t beginHue[3] = {25, 25, 25};
    uint8_t endHue[3] = {0, 0, 0};
    addLightAction(1500, 100, 130, beginHue, endHue);
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
            if (activeSensors <= 1) {
                tubeState = 0; // Back to Idle
            } else if (activeSensors >= 8) {
                tubeState = 2; // Switch to Moderate activity
            }
            break;

        case 2: // Moderate activity state
            if (activeSensors <= 4) {
                tubeState = 1; // Back to Low activity
            } else if (activeSensors >= 13) {
                tubeState = 3; // Switch to High activity
            }
            break;

        case 3: // High activity state
            if (activeSensors <= 8) {
                tubeState = 2; // Back to Moderate activity
            }
            break;
    }
}


void setAnimationForState(int startBrightness = 0, int startHue[3] = nullptr, bool useTransition = false) {
    lightQueue = LightQueue();  // Clear existing actions

    int targetBrightness = 0;
    uint8_t targetHue[3] = {0};

    // Determine the target values based on the next tube state
    switch (tubeState) {
        case 0:
            targetBrightness = 100; // starting brightness for idle animation
            for (int i = 0; i < 3; i++) {
                targetHue[i] = 184;
            }      // starting hue for idle animation
            break;
        case 1:
            targetBrightness = 130; // starting brightness for low activity animation
            for (int i = 0; i < 3; i++) {
                targetHue[i] = 139;
            }         // starting hue for low activity animation
            break;
        case 2:
            targetBrightness = 180; // starting brightness for moderate activity animation
            for (int i = 0; i < 3; i++) { // starting hue for moderate activity animation
                targetHue[i] = 90;
            }         
            break;
        case 3:
            targetBrightness = 215; // starting brightness for high activity animation
            for (int i = 0; i < 3; i++) { // starting hue for High activity animation
                targetHue[i] = 90;
            }            
            break;
    }

    // Calculate a dynamic duration based on the brightness and hue changes
    int brightnessDifference = abs(currentBrightness - targetBrightness);
    int hueDifference = {0};
    for (int i = 0; i < 3; i++) {
      int diff = abs(currentHue[i] - targetHue[i]);
      if(diff > hueDifference){
        hueDifference = diff;
      }
    }

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
  int progress;
  int goal = 0;
  for (int i = 0; i < 9; i++) {
    goal =+ NUM_LEDS%9;
    for (int i = progress; i < goal; i++)
      leds[i] = CHSV(BHPair.hue, 255, BHPair.brightness);
  }

  

  currentBrightness = BHPair.brightness;
  for (int j = 0; j < 3; j++) {
        currentHue[j] = BHPair.hue[j];
    }

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
      int startHue[3];
      for (int i = 0; i < 3; i++) {
          startHue[i] = currentHue[i];
      }
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
