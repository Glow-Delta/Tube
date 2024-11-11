#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    460
#define MAX_ACTIONS 5
#define FPS         30
#define FRAME_DELAY (1000 / FPS)
#define TUBE_ID 1

CRGB stage0Color1 = CRGB(28, 81, 255); //light blue
CRGB stage0Color2 = CRGB(255,255,255); //white

CRGB stage1Color1 = CRGB(28, 81, 255); //light blue
CRGB stage1Color2 = CRGB(255, 208, 0); //yellow

CRGB stage2Color1 = CRGB(0 ,255 ,255); //light blue
CRGB stage2Color2 = CRGB(255, 48, 117); //pink

CRGB stage3Color1 = CRGB(100, 48,255); //light blue
CRGB stage3Color2 = CRGB(255, 48, 76); //orange

CRGB leds[NUM_LEDS];
unsigned long lastFrameTime = 0;

int tubeState = 1;
int activeSensors = 0;

unsigned long lastActiveSensorChange = 0;

CRGB currentColor = CRGB(0,0,0);

// Struct for each light action
struct LightAction {
    int duration;
    CRGB beginColor;
    CRGB endColor;
    int totalFrames;
    int currentFrame;
    bool active;

    LightAction() : duration(0), beginColor(CRGB(0,0,0)), endColor(CRGB(0,0,0)), totalFrames(0), currentFrame(0), active(false) {}

    void initialize(int dur, CRGB beginC, CRGB endC) {
        duration = dur;
        beginColor = beginC;
        endColor = endC;
        totalFrames = (duration / FRAME_DELAY);
        currentFrame = 0;
        active = true;
    }

    // Function to get current brightness and hue
    CRGB getCurrentColor() {
      float progress = float(currentFrame) / totalFrames;

      uint8_t red = getCos(currentColor.red, endColor.red, progress);
      uint8_t green = getCos(currentColor.green, endColor.green, progress);
      uint8_t blue = getCos(currentColor.blue, endColor.blue, progress);

      CRGB color = CRGB(red, green, blue);

      return color;
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
void addLightAction(int duration, CRGB beginColor, CRGB endColor) {
    LightAction action1;
    LightAction action2;
    action1.initialize(duration, beginColor, endColor);
    action2.initialize(duration, endColor, beginColor);
    lightQueue.enqueue(action1);
    lightQueue.enqueue(action2);
}

// Define animations for each state with hue
void idleAnimation() {
    addLightAction(1500, stage0Color1, stage0Color2); // YELLOW hue
}

void lowActivityAnimation() {
    addLightAction(1000, stage1Color1, stage1Color2); // Green hue
}

void moderateActivityAnimation() {
    addLightAction(750, stage2Color1, stage2Color2); // Green hue
}

void highActivityAnimation() {
    addLightAction(500, stage3Color1, stage3Color2); // Red hue
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


void setAnimationForState(CRGB startColor = CRGB(0,0,0), bool useTransition = false) {
    lightQueue = LightQueue();  // Clear existing actions

    CRGB targetColor = 0;

    // Determine the target values based on the next tube state
    switch (tubeState) {
        case 0:
            targetColor = stage0Color1;
            break;
        case 1:
            targetColor = stage1Color1;
            break;
        case 2:
            targetColor = stage1Color1;
            break;
        case 3:
            targetColor = stage1Color1;
            break;
    }

    // Calculate a dynamic duration based on the brightness and hue changes
    int redDifference = abs(currentColor.red - targetColor.red);
    int greenDifference = abs(currentColor.green - targetColor.green);
    int blueDifference = abs(currentColor.blue - targetColor.blue);

    int colorDifference = redDifference + greenDifference + blueDifference;


    int transitionDuration = colorDifference * 1.5;

    if (useTransition) {
        // Add transition action with calculated dynamic duration
        LightAction transitionAction;
        transitionAction.initialize(transitionDuration, currentColor, targetColor);
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
  CRGB color = action->getCurrentColor();

  for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = color;
  }

  currentColor = color;

  FastLED.show();

  Serial.print(color.red);
  Serial.print(", ");
  Serial.print(color.green);
  Serial.print(", ");
  Serial.print(color.blue);
  Serial.print(", ");
  Serial.println(tubeState * 10);

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
      CRGB startColor = currentColor;
      setAnimationForState(startColor, true);  // Start a smooth transition to the new animation
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
