#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "DinoGame.h"

//#define USE_NOINTERNET

#ifdef USE_NOINTERNET
#include "no_internet.h"
#endif

// RP2040 ILI9341 pins
#define TFT_CS 17
#define TFT_DC 20
#define TFT_RST 21

// Button pins
#define BUTTON_JUMP 13
#define BUTTON_DUCK 12

// TFT Backlight control (if available, otherwise screen will just show black)
#define TFT_BACKLIGHT 11  // Set to actual pin if backlight control is available

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
DinoGame game(tft);

// Sleep mode variables
bool sleepMode = false;
unsigned long lastInputTime = 0;
unsigned long bothButtonsPressedTime = 0;
bool bothButtonsPressed = false;
bool buttonsReleasedAfterSleep = false;

const unsigned long SLEEP_TIMEOUT = 20000;  // timeout in milliseconds
const unsigned long BUTTON_HOLD_TIME = 3000;  // 3 seconds

void enterSleepMode() {
  sleepMode = true;
  buttonsReleasedAfterSleep = false;  // Reset flag - require button release before wake-up
  
  // Turn off screen
  tft.fillScreen(ILI9341_BLACK);
  
  // Turn off backlight if available
  #if TFT_BACKLIGHT >= 0
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, LOW);
  #endif
  
  Serial.println("Entering sleep mode...");
}

void exitSleepMode() {
  sleepMode = false;
  
  // Turn on backlight if available
  #if TFT_BACKLIGHT >= 0
  digitalWrite(TFT_BACKLIGHT, HIGH);
  #endif
  
  // Start a new game
  game.newGame();
  
  #ifdef USE_NOINTERNET
  tft.drawBitmap(0, 70, 320, 82, no_internet);
  #endif
  
  lastInputTime = millis();
  bothButtonsPressed = false;
  Serial.println("Exiting sleep mode...");
}

void checkBothButtons() {
  bool button1 = !digitalRead(BUTTON_JUMP);
  bool button2 = !digitalRead(BUTTON_DUCK);
  
  if (button1 && button2) {
    if (!bothButtonsPressed) {
      bothButtonsPressed = true;
      bothButtonsPressedTime = millis();
    } else {
      // Check if held for 3 seconds
      if (millis() - bothButtonsPressedTime >= BUTTON_HOLD_TIME) {
        if (sleepMode) {
          exitSleepMode();
        } else {
          enterSleepMode();
        }
        bothButtonsPressed = false;  // Reset to prevent repeated triggers
      }
    }
  } else {
    bothButtonsPressed = false;
  }
}

void checkInactivity() {
  if (!sleepMode && (millis() - lastInputTime >= SLEEP_TIMEOUT)) {
    enterSleepMode();
  }
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  game.newGame();
  pinMode(BUTTON_JUMP, INPUT_PULLUP);
  pinMode(BUTTON_DUCK, INPUT_PULLUP);
  
  #if TFT_BACKLIGHT >= 0
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);
  #endif
  
  #ifdef USE_NOINTERNET
  tft.drawBitmap(0, 70, 320, 82, no_internet);
  #endif
  
  lastInputTime = millis();
}

void loop() {
  // Always check for both buttons to enter/exit sleep mode
  checkBothButtons();
  
  if (sleepMode) {
    // Check if buttons are released
    if (digitalRead(BUTTON_JUMP) && digitalRead(BUTTON_DUCK)) {
      buttonsReleasedAfterSleep = true;
    }
    
    // Only wake up if buttons were released first, then pressed again
    if (buttonsReleasedAfterSleep && (!digitalRead(BUTTON_JUMP) || !digitalRead(BUTTON_DUCK))) {
      exitSleepMode();
      delay(500);  // Debounce delay
    }
    delay(100);
    return;
  }
  
  // Check for inactivity timeout
  checkInactivity();
  
  // Handle serial input
  char c = Serial.read();
  if(c >= '0' && c <= '9') {
    game.setSpeedModifier(c - '0');
    lastInputTime = millis();
  }
  
  // Handle button inputs
  bool jumpPressed = !digitalRead(BUTTON_JUMP);
  bool duckPressed = !digitalRead(BUTTON_DUCK);
  
  if(jumpPressed) {
    game.jump();
    lastInputTime = millis();
  } else {
    if(duckPressed) {
      game.duck();
      lastInputTime = millis();
    } else {
      game.stand();
    }
  }
  
  // Handle game over
  while(game.isGameOver()){
    // Check inactivity in game over screen too
    checkInactivity();
    checkBothButtons();
    
    if (sleepMode) break;  // Exit if entering sleep mode
    
    if(!digitalRead(BUTTON_JUMP) || !digitalRead(BUTTON_DUCK)){
      game.newGame();
      #ifdef USE_NOINTERNET
      tft.drawBitmap(0, 70, 320, 82, no_internet);
      #endif   
      lastInputTime = millis();
      break;
    }
  }
  
  game.loop();
  lastInputTime = millis();  // Update timer on each active game frame
}
