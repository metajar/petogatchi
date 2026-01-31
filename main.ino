/*
 * M5StickC Plus2 Tamagotchi Dog Pet
 * A fun virtual pet with feeding, sleeping, and interactive features!
 * 
 * Controls:
 * - Button A (M5/Front): Select / Confirm
 * - Button B (Side): Navigate menu / Wake up
 * - Long press A: Quick feed
 * - Long press B: Put to sleep
 */

#include <M5StickCPlus2.h>
#include <Preferences.h>

// Display dimensions in vertical mode
#define SCREEN_WIDTH 135
#define SCREEN_HEIGHT 240

// Double buffer sprite to eliminate flicker
M5Canvas canvas(&M5.Lcd);

// Preferences for persistent storage
Preferences prefs;

// IMU for shake detection
float accelX, accelY, accelZ;
float shakeMagnitude = 0;
const float SHAKE_THRESHOLD = 3.0;  // Acceleration threshold for shake
const int SHAKE_DEBOUNCE_MS = 1000;  // Minimum time between shake detections
unsigned long lastShakeTime = 0;
int shakeCount = 0;

// Power management
#define SLEEP_TIMEOUT_MS 60000       // 60 seconds of inactivity before sleep
#define ALERT_CHECK_INTERVAL_US 900000000ULL  // 15 minutes in microseconds
#define STAT_CRITICAL_THRESHOLD 25   // Alert if any stat drops below this

// Forward declarations
void drawDogFace(int centerX, int centerY, bool sleeping = false);
void drawMouth(int x, int y);
void playWakeSound();
void saveStats();
void loadStats();
void resetPet();

// Colors
#define COLOR_BG        TFT_BLACK
#define COLOR_DOG       0xFDA0  // Tan/brown color
#define COLOR_DOG_DARK  0xC380  // Darker tan
#define COLOR_EYE_WHITE TFT_WHITE
#define COLOR_EYE_PUPIL TFT_BLACK
#define COLOR_NOSE      TFT_BLACK
#define COLOR_TONGUE    0xF98F  // Pink
#define COLOR_MENU_BG   0x2104  // Dark gray
#define COLOR_MENU_SEL  0x04FF  // Cyan
#define COLOR_HAPPY     TFT_GREEN
#define COLOR_HUNGRY    TFT_ORANGE
#define COLOR_TIRED     TFT_BLUE
#define COLOR_SICK      0x7BE0  // Purple

// Pet states
enum PetState {
  STATE_IDLE,
  STATE_HAPPY,
  STATE_EATING,
  STATE_SLEEPING,
  STATE_HUNGRY,
  STATE_SICK,
  STATE_PLAYING,
  STATE_VOMITING  // New state for shake-induced vomiting
};

// Menu items
enum MenuItem {
  MENU_NONE = -1,
  MENU_FEED = 0,
  MENU_PLAY,
  MENU_SLEEP,
  MENU_STATS,
  MENU_COUNT
};

const char* menuLabels[] = {"Feed", "Play", "Sleep", "Stats"};
const char* menuIcons[] = {"*", "o", "z", "?"};

// Pet stats
struct PetStats {
  int hunger;      // 0-100 (100 = full)
  int happiness;   // 0-100
  int energy;      // 0-100
  int health;      // 0-100
  unsigned long age;  // in minutes
};

// Global variables
PetStats pet = {80, 80, 100, 100, 0};
PetState currentState = STATE_IDLE;
MenuItem selectedMenu = MENU_NONE;
bool menuOpen = false;

// Animation variables
int eyeOffsetX = 0;
int eyeOffsetY = 0;
int blinkTimer = 0;
bool isBlinking = false;
int mouthFrame = 0;
int animationFrame = 0;
int bounceOffset = 0;
bool bouncingUp = true;
int vomitFrame = 0;  // For vomit animation

// Timing
unsigned long lastUpdate = 0;
unsigned long lastStatDecay = 0;
unsigned long lastBlink = 0;
unsigned long lastAnimation = 0;
unsigned long sleepStartTime = 0;
unsigned long lastButtonPress = 0;
unsigned long buttonHoldStart = 0;
unsigned long lastInteraction = 0;  // For auto-sleep
bool buttonAHeld = false;
bool buttonBHeld = false;
bool alertWakeup = false;  // True if woken by timer for alert check

// Sound frequencies for different actions
void playHappySound() {
  M5.Speaker.tone(800, 100);
  delay(120);
  M5.Speaker.tone(1000, 100);
  delay(120);
  M5.Speaker.tone(1200, 150);
}

void playSadSound() {
  M5.Speaker.tone(400, 200);
  delay(220);
  M5.Speaker.tone(300, 300);
}

void playEatSound() {
  for (int i = 0; i < 3; i++) {
    M5.Speaker.tone(600, 80);
    delay(100);
    M5.Speaker.tone(400, 80);
    delay(100);
  }
}

void playSleepSound() {
  M5.Speaker.tone(300, 400);
  delay(500);
  M5.Speaker.tone(250, 600);
}

void playWakeSound() {
  M5.Speaker.tone(400, 100);
  delay(120);
  M5.Speaker.tone(600, 100);
  delay(120);
  M5.Speaker.tone(800, 200);
}

void playPlaySound() {
  M5.Speaker.tone(1000, 80);
  delay(100);
  M5.Speaker.tone(1200, 80);
  delay(100);
  M5.Speaker.tone(1000, 80);
  delay(100);
  M5.Speaker.tone(800, 150);
}

void playMenuSound() {
  M5.Speaker.tone(1500, 30);
}

void playSelectSound() {
  M5.Speaker.tone(1000, 50);
  delay(60);
  M5.Speaker.tone(1500, 80);
}

void playErrorSound() {
  M5.Speaker.tone(200, 200);
}

void playAlertSound() {
  // Urgent alert sound
  for (int i = 0; i < 3; i++) {
    M5.Speaker.tone(1000, 150);
    delay(200);
    M5.Speaker.tone(500, 150);
    delay(200);
  }
}

// Save pet stats to flash
void saveStats() {
  prefs.begin("puppy", false);
  prefs.putInt("hunger", pet.hunger);
  prefs.putInt("happiness", pet.happiness);
  prefs.putInt("energy", pet.energy);
  prefs.putInt("health", pet.health);
  prefs.putULong("age", pet.age);
  prefs.putInt("state", (int)currentState);
  
  // Save current time from RTC as simple components
  auto dt = M5.Rtc.getDateTime();
  // Store as minutes since midnight + day counter for simplicity
  unsigned long timeVal = dt.date.date * 24 * 60 + dt.time.hours * 60 + dt.time.minutes;
  prefs.putULong("saveTime", timeVal);
  prefs.putInt("saveMonth", dt.date.month);
  
  prefs.end();
}

// Load pet stats from flash and apply time delta
void loadStats() {
  prefs.begin("puppy", true);
  
  // Check if we have saved data at all
  if (!prefs.isKey("hunger")) {
    // First ever boot - use defaults
    prefs.end();
    return;
  }
  
  pet.hunger = prefs.getInt("hunger", 80);
  pet.happiness = prefs.getInt("happiness", 80);
  pet.energy = prefs.getInt("energy", 100);
  pet.health = prefs.getInt("health", 100);
  pet.age = prefs.getULong("age", 0);
  
  int savedState = prefs.getInt("state", STATE_IDLE);
  unsigned long savedTime = prefs.getULong("saveTime", 0);
  int savedMonth = prefs.getInt("saveMonth", 0);
  
  prefs.end();
  
  // Only apply time-based decay if waking from deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER || wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Get current time from RTC
    auto dt = M5.Rtc.getDateTime();
    unsigned long currentTime = dt.date.date * 24 * 60 + dt.time.hours * 60 + dt.time.minutes;
    
    unsigned long elapsedMinutes = 0;
    
    // Only calculate if we have valid saved time and RTC looks initialized
    if (savedTime > 0 && dt.date.year > 2020) {
      if (dt.date.month == savedMonth && currentTime >= savedTime) {
        elapsedMinutes = currentTime - savedTime;
      } else if (dt.date.month == savedMonth && currentTime < savedTime) {
        // Time went backwards? RTC issue, use minimal decay
        elapsedMinutes = 1;
      } else {
        // Different month, estimate based on wake type
        elapsedMinutes = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? 15 : 5;
      }
    } else {
      // RTC not set or no saved time, use minimal estimate
      elapsedMinutes = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) ? 15 : 1;
    }
    
    // Cap elapsed time to reasonable max (60 min) to prevent stats going to zero
    if (elapsedMinutes > 60) {
      elapsedMinutes = 60;
    }
    
    // Apply stat decay based on elapsed time (1 point per 2 minutes)
    int decayTicks = elapsedMinutes / 2;
    
    if (savedState == STATE_SLEEPING) {
      // Pet was sleeping - restore energy instead of decay
      pet.energy = min(100, pet.energy + decayTicks);
      currentState = STATE_SLEEPING;
    } else {
      // Normal decay - gradual
      pet.hunger = max(0, pet.hunger - decayTicks);
      pet.happiness = max(0, pet.happiness - decayTicks);
      pet.energy = max(0, pet.energy - decayTicks / 2);  // Energy decays slower
      currentState = STATE_IDLE;
    }
    
    // Health decay only if stats are really bad
    if (pet.hunger < 10 || pet.happiness < 10) {
      pet.health = max(0, pet.health - decayTicks);
    }
    
    // Age increment
    pet.age += elapsedMinutes;
    
    alertWakeup = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  } else {
    // Fresh boot (power on, reset, etc) - just restore state, NO decay
    currentState = (PetState)savedState;
    if (currentState == STATE_SLEEPING) {
      currentState = STATE_IDLE;  // Wake up on fresh boot
    }
    alertWakeup = false;
  }
}

// Reset pet to defaults (clears saved data)
void resetPet() {
  prefs.begin("puppy", false);
  prefs.clear();
  prefs.end();
  
  pet.hunger = 80;
  pet.happiness = 80;
  pet.energy = 100;
  pet.health = 100;
  pet.age = 0;
  currentState = STATE_IDLE;
}

// Check if any stats need alert
bool needsAlert() {
  return (pet.hunger < STAT_CRITICAL_THRESHOLD ||
          pet.happiness < STAT_CRITICAL_THRESHOLD ||
          pet.energy < STAT_CRITICAL_THRESHOLD ||
          pet.health < STAT_CRITICAL_THRESHOLD);
}

// Get battery percentage
int getBatteryPercent() {
  int vbat = M5.Power.getBatteryLevel();
  return vbat;
}

// Enter deep sleep mode
void enterDeepSleep() {
  // Save current state
  saveStats();
  
  // Show sleep message
  canvas.fillSprite(COLOR_BG);
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextSize(1);
  canvas.drawString("Going to sleep...", 20, 100);
  canvas.drawString("Press PWR to wake", 15, 120);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Checking in 15min", 15, 150);
  canvas.pushSprite(0, 0);
  
  delay(1500);
  
  // Turn off display
  M5.Lcd.sleep();
  M5.Lcd.setBrightness(0);
  
  // Configure wake sources:
  // 1. Button A (GPIO37) - external wakeup
  // 2. Timer - 15 minute check
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, LOW);  // Button A
  esp_sleep_enable_timer_wakeup(ALERT_CHECK_INTERVAL_US);  // 15 minutes
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

// Handle wakeup from deep sleep
void handleWakeup() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    // Timer wakeup - check if we need to alert
    if (needsAlert()) {
      // Show alert screen and play sound
      alertWakeup = true;
      M5.Lcd.wakeup();
      M5.Lcd.setBrightness(100);
      
      playAlertSound();
      
      // Show which stat is low
      canvas.fillSprite(COLOR_BG);
      canvas.setTextColor(TFT_RED);
      canvas.setTextSize(2);
      canvas.drawString("ALERT!", 35, 30);
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_WHITE);
      
      int y = 70;
      if (pet.hunger < STAT_CRITICAL_THRESHOLD) {
        canvas.drawString("I'm hungry!", 30, y);
        y += 20;
      }
      if (pet.happiness < STAT_CRITICAL_THRESHOLD) {
        canvas.drawString("I'm sad!", 35, y);
        y += 20;
      }
      if (pet.energy < STAT_CRITICAL_THRESHOLD) {
        canvas.drawString("I'm tired!", 32, y);
        y += 20;
      }
      if (pet.health < STAT_CRITICAL_THRESHOLD) {
        canvas.drawString("I'm sick!", 35, y);
        y += 20;
      }
      
      // Draw sad face
      drawDogFace(SCREEN_WIDTH / 2, 160);
      
      canvas.setTextColor(TFT_DARKGREY);
      canvas.drawString("Press A to care", 20, 220);
      canvas.pushSprite(0, 0);
      
      // Wait for button or timeout
      unsigned long alertStart = millis();
      while (millis() - alertStart < 30000) {  // 30 second timeout
        M5.update();
        if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
          lastInteraction = millis();
          alertWakeup = false;
          return;  // User responded, continue normal operation
        }
        delay(50);
      }
      
      // No response, go back to sleep
      enterDeepSleep();
    } else {
      // Stats are fine, go back to sleep
      saveStats();  // Save any time-based changes
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, LOW);
      esp_sleep_enable_timer_wakeup(ALERT_CHECK_INTERVAL_US);
      esp_deep_sleep_start();
    }
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Button wakeup - user wants to interact
    alertWakeup = false;
    M5.Lcd.wakeup();
    M5.Lcd.setBrightness(100);
    playWakeSound();
  }
  // For other wakeup reasons (power on, reset), just continue normally
}

// Draw vomit particles for shake sickness
void drawVomit(int centerX, int centerY) {
  if (currentState != STATE_VOMITING) return;
  
  // Draw vomit particles coming from mouth area
  int vomitColor = 0x7BE0;  // Greenish color
  
  // Animated particles
  for (int i = 0; i < 5; i++) {
    int particleX = centerX - 10 + (vomitFrame * 3) + (i * 8);
    int particleY = centerY + 30 + (vomitFrame * 2) + (i * 5);
    
    if (particleX < SCREEN_WIDTH && particleY < SCREEN_HEIGHT) {
      // Draw droplet shape
      canvas.fillCircle(particleX, particleY, 4 + (i % 2), vomitColor);
      canvas.fillCircle(particleX + 2, particleY + 2, 2, 0x4E00);  // Darker green
    }
  }
  
  // Add "sick" indicator
  canvas.setTextColor(TFT_GREEN);
  canvas.setTextSize(1);
  canvas.drawString("*urp*", centerX - 20, centerY - 50);
}

// Draw the dog face
void drawDogFace(int centerX, int centerY, bool sleeping) {
  int faceRadius = 45;
  int actualY = centerY + bounceOffset;
  
  // Draw ears (floppy dog ears)
  // Left ear
  canvas.fillEllipse(centerX - 35, actualY - 30, 18, 35, COLOR_DOG_DARK);
  canvas.fillEllipse(centerX - 35, actualY - 25, 14, 28, COLOR_DOG);
  
  // Right ear
  canvas.fillEllipse(centerX + 35, actualY - 30, 18, 35, COLOR_DOG_DARK);
  canvas.fillEllipse(centerX + 35, actualY - 25, 14, 28, COLOR_DOG);
  
  // Main face (oval)
  canvas.fillEllipse(centerX, actualY, faceRadius, faceRadius + 10, COLOR_DOG);
  
  // Snout (lighter area)
  canvas.fillEllipse(centerX, actualY + 15, 25, 20, COLOR_EYE_WHITE);
  
  // Draw eyes
  int eyeY = actualY - 10;
  int leftEyeX = centerX - 18;
  int rightEyeX = centerX + 18;
  
  if (sleeping) {
    // Sleeping eyes (closed lines)
    canvas.drawLine(leftEyeX - 10, eyeY, leftEyeX + 10, eyeY, COLOR_EYE_PUPIL);
    canvas.drawLine(leftEyeX - 8, eyeY - 3, leftEyeX - 10, eyeY, COLOR_EYE_PUPIL);
    canvas.drawLine(leftEyeX + 8, eyeY - 3, leftEyeX + 10, eyeY, COLOR_EYE_PUPIL);
    
    canvas.drawLine(rightEyeX - 10, eyeY, rightEyeX + 10, eyeY, COLOR_EYE_PUPIL);
    canvas.drawLine(rightEyeX - 8, eyeY - 3, rightEyeX - 10, eyeY, COLOR_EYE_PUPIL);
    canvas.drawLine(rightEyeX + 8, eyeY - 3, rightEyeX + 10, eyeY, COLOR_EYE_PUPIL);
    
    // Z's for sleeping
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(1);
    int zOffset = (millis() / 500) % 3;
    canvas.drawString("z", centerX + 30 + zOffset * 5, actualY - 40 - zOffset * 8);
    canvas.drawString("Z", centerX + 40 + zOffset * 3, actualY - 55 - zOffset * 5);
  } else if (isBlinking) {
    // Blinking (horizontal lines)
    canvas.drawLine(leftEyeX - 8, eyeY, leftEyeX + 8, eyeY, COLOR_EYE_PUPIL);
    canvas.drawLine(rightEyeX - 8, eyeY, rightEyeX + 8, eyeY, COLOR_EYE_PUPIL);
  } else {
    // Open eyes
    // Eye whites
    canvas.fillEllipse(leftEyeX, eyeY, 12, 14, COLOR_EYE_WHITE);
    canvas.fillEllipse(rightEyeX, eyeY, 12, 14, COLOR_EYE_WHITE);
    
    // Pupils (with offset for looking around)
    int pupilX = eyeOffsetX;
    int pupilY = eyeOffsetY;
    canvas.fillCircle(leftEyeX + pupilX, eyeY + pupilY, 6, COLOR_EYE_PUPIL);
    canvas.fillCircle(rightEyeX + pupilX, eyeY + pupilY, 6, COLOR_EYE_PUPIL);
    
    // Eye shine
    canvas.fillCircle(leftEyeX + pupilX + 2, eyeY + pupilY - 2, 2, TFT_WHITE);
    canvas.fillCircle(rightEyeX + pupilX + 2, eyeY + pupilY - 2, 2, TFT_WHITE);
    
    // Eyebrows based on mood
    if (pet.happiness < 30) {
      // Sad eyebrows
      canvas.drawLine(leftEyeX - 10, eyeY - 18, leftEyeX + 8, eyeY - 22, COLOR_DOG_DARK);
      canvas.drawLine(rightEyeX - 8, eyeY - 22, rightEyeX + 10, eyeY - 18, COLOR_DOG_DARK);
    } else if (currentState == STATE_HAPPY || currentState == STATE_PLAYING) {
      // Happy arched eyebrows
      canvas.drawLine(leftEyeX - 8, eyeY - 20, leftEyeX + 8, eyeY - 22, COLOR_DOG_DARK);
      canvas.drawLine(rightEyeX - 8, eyeY - 22, rightEyeX + 8, eyeY - 20, COLOR_DOG_DARK);
    }
  }
  
  // Nose
  canvas.fillTriangle(centerX - 8, actualY + 8, centerX + 8, actualY + 8, 
                       centerX, actualY + 18, COLOR_NOSE);
  canvas.fillEllipse(centerX, actualY + 8, 10, 6, COLOR_NOSE);
  // Nose shine
  canvas.fillCircle(centerX - 3, actualY + 6, 2, 0x4208);
  
  // Mouth
  drawMouth(centerX, actualY + 25);
}

void drawMouth(int x, int y) {
  if (currentState == STATE_VOMITING) {
    // Vomiting mouth - open wide with sick expression
    canvas.fillEllipse(x, y + 5, 18, 15, COLOR_NOSE);
    canvas.fillEllipse(x, y + 10, 12, 10, 0x7BE0);  // Greenish inside
  } else if (currentState == STATE_EATING) {
    // Animated eating mouth
    if (mouthFrame % 2 == 0) {
      // Open mouth
      canvas.fillEllipse(x, y + 5, 15, 12, COLOR_NOSE);
      canvas.fillEllipse(x, y + 8, 10, 8, COLOR_TONGUE);
    } else {
      // Closed mouth
      canvas.drawLine(x - 10, y, x, y + 5, COLOR_NOSE);
      canvas.drawLine(x, y + 5, x + 10, y, COLOR_NOSE);
    }
  } else if (currentState == STATE_HAPPY || currentState == STATE_PLAYING) {
    // Big happy smile with tongue
    canvas.drawLine(x - 15, y - 5, x - 5, y + 5, COLOR_NOSE);
    canvas.drawLine(x - 5, y + 5, x + 5, y + 5, COLOR_NOSE);
    canvas.drawLine(x + 5, y + 5, x + 15, y - 5, COLOR_NOSE);
    // Tongue sticking out
    canvas.fillEllipse(x, y + 12, 8, 10, COLOR_TONGUE);
  } else if (pet.happiness < 30 || pet.hunger < 30) {
    // Sad mouth
    canvas.drawLine(x - 10, y + 5, x, y, COLOR_NOSE);
    canvas.drawLine(x, y, x + 10, y + 5, COLOR_NOSE);
  } else if (currentState == STATE_SLEEPING) {
    // Peaceful sleeping mouth
    canvas.drawLine(x - 8, y, x + 8, y, COLOR_NOSE);
  } else if (currentState == STATE_SICK) {
    // Sick mouth - slightly open, unhappy
    canvas.drawEllipse(x, y + 3, 12, 8, COLOR_NOSE);
  } else {
    // Normal smile
    canvas.drawLine(x - 12, y - 3, x, y + 3, COLOR_NOSE);
    canvas.drawLine(x, y + 3, x + 12, y - 3, COLOR_NOSE);
  }
}

// Draw status bars
void drawStatusBars() {
  int barWidth = 100;
  int barHeight = 8;
  int startX = 17;
  int startY = 180;
  int spacing = 14;
  
  // Labels and bars
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  
  // Hunger bar
  canvas.drawString("HNG", 2, startY - 2);
  canvas.drawRect(startX, startY, barWidth, barHeight, TFT_WHITE);
  int hungerWidth = map(pet.hunger, 0, 100, 0, barWidth - 2);
  uint16_t hungerColor = pet.hunger > 50 ? TFT_GREEN : (pet.hunger > 25 ? TFT_YELLOW : TFT_RED);
  canvas.fillRect(startX + 1, startY + 1, hungerWidth, barHeight - 2, hungerColor);
  
  // Happiness bar
  canvas.drawString("HPY", 2, startY + spacing - 2);
  canvas.drawRect(startX, startY + spacing, barWidth, barHeight, TFT_WHITE);
  int happyWidth = map(pet.happiness, 0, 100, 0, barWidth - 2);
  uint16_t happyColor = pet.happiness > 50 ? TFT_GREEN : (pet.happiness > 25 ? TFT_YELLOW : TFT_RED);
  canvas.fillRect(startX + 1, startY + spacing + 1, happyWidth, barHeight - 2, happyColor);
  
  // Energy bar
  canvas.drawString("NRG", 2, startY + spacing * 2 - 2);
  canvas.drawRect(startX, startY + spacing * 2, barWidth, barHeight, TFT_WHITE);
  int energyWidth = map(pet.energy, 0, 100, 0, barWidth - 2);
  uint16_t energyColor = pet.energy > 50 ? TFT_CYAN : (pet.energy > 25 ? TFT_YELLOW : TFT_RED);
  canvas.fillRect(startX + 1, startY + spacing * 2 + 1, energyWidth, barHeight - 2, energyColor);
}

// Draw menu
void drawMenu() {
  int menuWidth = 110;
  int menuHeight = 100;
  int menuX = (SCREEN_WIDTH - menuWidth) / 2;
  int menuY = 60;
  int itemHeight = 22;
  
  // Menu background
  canvas.fillRoundRect(menuX, menuY, menuWidth, menuHeight, 8, COLOR_MENU_BG);
  canvas.drawRoundRect(menuX, menuY, menuWidth, menuHeight, 8, TFT_WHITE);
  
  // Menu items
  canvas.setTextSize(1);
  for (int i = 0; i < MENU_COUNT; i++) {
    int itemY = menuY + 8 + i * itemHeight;
    
    if (i == selectedMenu) {
      // Selected item
      canvas.fillRoundRect(menuX + 4, itemY - 2, menuWidth - 8, itemHeight - 2, 4, COLOR_MENU_SEL);
      canvas.setTextColor(TFT_BLACK);
    } else {
      canvas.setTextColor(TFT_WHITE);
    }
    
    // Icon and label
    canvas.drawString(menuIcons[i], menuX + 12, itemY + 2);
    canvas.drawString(menuLabels[i], menuX + 28, itemY + 2);
  }
  
  // Instructions
  canvas.setTextColor(TFT_DARKGREY);
  canvas.setTextSize(1);
  canvas.drawString("B:Nav A:Select", menuX + 5, menuY + menuHeight + 5);
}

// Draw stats screen
void drawStatsScreen() {
  canvas.fillSprite(COLOR_BG);
  
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextSize(2);
  canvas.drawString("PET STATS", 20, 15);
  
  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE);
  
  int y = 50;
  int spacing = 22;
  
  char buffer[32];
  
  sprintf(buffer, "Hunger:    %d%%", pet.hunger);
  canvas.drawString(buffer, 15, y);
  
  sprintf(buffer, "Happiness: %d%%", pet.happiness);
  canvas.drawString(buffer, 15, y + spacing);
  
  sprintf(buffer, "Energy:    %d%%", pet.energy);
  canvas.drawString(buffer, 15, y + spacing * 2);
  
  sprintf(buffer, "Health:    %d%%", pet.health);
  canvas.drawString(buffer, 15, y + spacing * 3);
  
  unsigned long ageMinutes = pet.age;
  int hours = ageMinutes / 60;
  int mins = ageMinutes % 60;
  sprintf(buffer, "Age: %dh %dm", hours, mins);
  canvas.drawString(buffer, 15, y + spacing * 4);
  
  // Battery info
  canvas.drawLine(10, y + spacing * 5, 125, y + spacing * 5, TFT_DARKGREY);
  
  int batteryPct = getBatteryPercent();
  uint16_t battColor = batteryPct > 50 ? TFT_GREEN : (batteryPct > 20 ? TFT_YELLOW : TFT_RED);
  canvas.setTextColor(battColor);
  sprintf(buffer, "Battery: %d%%", batteryPct);
  canvas.drawString(buffer, 15, y + spacing * 5 + 8);
  
  // Draw battery icon
  int battX = 100;
  int battY = y + spacing * 5 + 6;
  canvas.drawRect(battX, battY, 20, 10, TFT_WHITE);
  canvas.fillRect(battX + 20, battY + 3, 3, 4, TFT_WHITE);
  int fillWidth = map(batteryPct, 0, 100, 0, 18);
  canvas.fillRect(battX + 1, battY + 1, fillWidth, 8, battColor);
  
  // Overall status
  canvas.setTextSize(1);
  y = y + spacing * 6 + 10;
  
  if (pet.health > 80 && pet.happiness > 60) {
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString("Status: THRIVING!", 15, y);
  } else if (pet.health > 50 && pet.happiness > 40) {
    canvas.setTextColor(TFT_YELLOW);
    canvas.drawString("Status: Doing OK", 15, y);
  } else {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("Status: Needs care!", 15, y);
  }
  
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("Press A to return", 15, 220);
  
  canvas.pushSprite(0, 0);
  
  // Wait for button press
  while (true) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      playSelectSound();
      lastInteraction = millis();
      break;
    }
    delay(50);
  }
}

// Feed animation
void feedPet() {
  // Don't reset lastInteraction here - only after completion
  
  if (pet.hunger >= 100) {
    playErrorSound();
    // Show "Already full!" message
    canvas.fillSprite(COLOR_BG);
    drawDogFace(SCREEN_WIDTH / 2, 90);
    drawStatusBars();
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(1);
    canvas.drawString("Already full!", 30, 160);
    canvas.pushSprite(0, 0);
    delay(1000);
    return;
  }
  
  currentState = STATE_EATING;
  playEatSound();
  
  // Eating animation
  for (int i = 0; i < 6; i++) {
    mouthFrame = i;
    canvas.fillSprite(COLOR_BG);
    
    // Draw title
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextSize(1);
    canvas.drawString("~ PUPPY PAL ~", 25, 5);
    
    // Draw food falling
    int foodY = 20 + i * 20;
    if (foodY < 100) {
      canvas.setTextSize(2);
      canvas.setTextColor(0xFC00);  // Orange for food
      canvas.drawString("*", 60, foodY);
    }
    
    drawDogFace(SCREEN_WIDTH / 2, 90);
    drawStatusBars();
    
    canvas.pushSprite(0, 0);
    delay(200);
  }
  
  // Increase hunger (fullness)
  pet.hunger = min(100, pet.hunger + 25);
  pet.happiness = min(100, pet.happiness + 5);
  
  currentState = STATE_HAPPY;
  playHappySound();
  lastInteraction = millis();  // CRITICAL: Reset interaction timer after completion
}

// Play with pet
void playWithPet() {
  // Don't reset lastInteraction here - only after completion
  
  if (pet.energy < 20) {
    playErrorSound();
    canvas.fillSprite(COLOR_BG);
    drawDogFace(SCREEN_WIDTH / 2, 90);
    drawStatusBars();
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(1);
    canvas.drawString("Too tired!", 35, 160);
    canvas.pushSprite(0, 0);
    delay(1000);
    return;
  }
  
  currentState = STATE_PLAYING;
  playPlaySound();
  
  // Play animation - bouncing and looking around
  for (int i = 0; i < 12; i++) {
    canvas.fillSprite(COLOR_BG);
    
    // Draw title
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextSize(1);
    canvas.drawString("~ PUPPY PAL ~", 25, 5);
    
    bounceOffset = (i % 2 == 0) ? -8 : 8;
    eyeOffsetX = (i % 4 < 2) ? -3 : 3;
    
    // Draw play indicators
    canvas.setTextColor(TFT_YELLOW);
    canvas.setTextSize(1);
    int starX = 20 + (i * 17) % 80;
    int starY = 30 + (i * 13) % 40;
    canvas.drawString("*", starX, starY);
    canvas.drawString("*", starX + 40, starY + 20);
    
    drawDogFace(SCREEN_WIDTH / 2, 90);
    drawStatusBars();
    
    canvas.pushSprite(0, 0);
    delay(150);
  }
  
  bounceOffset = 0;
  eyeOffsetX = 0;
  
  // Update stats
  pet.happiness = min(100, pet.happiness + 20);
  pet.energy = max(0, pet.energy - 15);
  pet.hunger = max(0, pet.hunger - 5);
  
  currentState = STATE_HAPPY;
  lastInteraction = millis();  // CRITICAL: Reset interaction timer after completion
}

// Put pet to sleep
void putToSleep() {
  currentState = STATE_SLEEPING;
  sleepStartTime = millis();
  playSleepSound();
  
  // Dim the screen slightly but keep running (low-power mode, not deep sleep)
  M5.Lcd.setBrightness(30);
  
  // Don't reset lastInteraction here - let it time out naturally
}

// Wake up pet
void wakeUp() {
  if (currentState != STATE_SLEEPING) return;
  
  // Energy is already being restored continuously by updateStats() while sleeping
  // No need to add additional energy here - the pet has been resting
  
  currentState = STATE_IDLE;
  M5.Lcd.setBrightness(100);
  playWakeSound();
  lastInteraction = millis();  // CRITICAL: Reset interaction timer when waking up
}

// Handle shake-induced sickness
void handleShakeSickness() {
  // Can't get sick if already sick or vomiting
  if (currentState == STATE_SICK || currentState == STATE_VOMITING) {
    return;
  }
  
  // Shake makes pet sick
  currentState = STATE_VOMITING;
  pet.health = max(0, pet.health - 15);
  pet.happiness = max(0, pet.happiness - 20);
  
  playSadSound();
  
  // Vomit animation
  for (int i = 0; i < 8; i++) {
    vomitFrame = i;
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextSize(1);
    canvas.drawString("~ PUPPY PAL ~", 25, 5);
    drawDogFace(SCREEN_WIDTH / 2, 90);
    drawVomit(SCREEN_WIDTH / 2, 115);
    drawStatusBars();
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString("Sick!", 50, 150);
    canvas.pushSprite(0, 0);
    delay(200);
  }
  
  // After vomiting, enter sick state
  currentState = STATE_SICK;
  vomitFrame = 0;
}

// Update pet stats over time
void updateStats() {
  if (currentState == STATE_SLEEPING) {
    // Restore energy while resting - 1 point every 10 seconds (called every 10s)
    // This means ~6 energy per minute, or full restoration in ~17 minutes
    pet.energy = min(100, pet.energy + 5);
    return;
  }
  
  // Decay stats over time
  pet.hunger = max(0, pet.hunger - 1);
  pet.happiness = max(0, pet.happiness - 1);
  pet.energy = max(0, pet.energy - 1);
  
  // Health based on other stats
  if (pet.hunger < 20 || pet.happiness < 20) {
    pet.health = max(0, pet.health - 2);
  } else if (pet.hunger > 60 && pet.happiness > 60 && pet.energy > 40) {
    pet.health = min(100, pet.health + 1);
  }
  
  // Update state based on stats
  if (pet.hunger < 30 && currentState == STATE_IDLE) {
    currentState = STATE_HUNGRY;
    playSadSound();
  }
  
  // Recover from sickness over time
  if (currentState == STATE_SICK) {
    if (pet.health > 60 && pet.happiness > 50 && pet.energy > 40) {
      currentState = STATE_IDLE;
      playHappySound();
    }
  }
  
  // Age (in minutes, increment every minute)
  static unsigned long lastAgeUpdate = 0;
  if (millis() - lastAgeUpdate > 60000) {
    pet.age++;
    lastAgeUpdate = millis();
  }
}

// Update animations
void updateAnimations() {
  // Blinking
  if (millis() - lastBlink > (isBlinking ? 150 : random(2000, 5000))) {
    isBlinking = !isBlinking;
    lastBlink = millis();
  }
  
  // Eye movement (looking around)
  if (!isBlinking && random(100) < 5) {
    eyeOffsetX = random(-3, 4);
    eyeOffsetY = random(-2, 3);
  }
  
  // Idle bounce animation
  if (currentState == STATE_IDLE || currentState == STATE_HAPPY) {
    if (millis() - lastAnimation > 500) {
      if (bouncingUp) {
        bounceOffset--;
        if (bounceOffset <= -3) bouncingUp = false;
      } else {
        bounceOffset++;
        if (bounceOffset >= 0) bouncingUp = true;
      }
      lastAnimation = millis();
    }
  }
  
  // Reset happy state after a while
  if (currentState == STATE_HAPPY && millis() - lastUpdate > 3000) {
    currentState = STATE_IDLE;
  }
}

// Main draw function
void drawScreen() {
  canvas.fillSprite(COLOR_BG);
  
  // Draw title
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextSize(1);
  canvas.drawString("~ PUPPY PAL ~", 25, 5);
  
  // Draw small battery indicator in corner
  int battPct = getBatteryPercent();
  uint16_t battColor = battPct > 50 ? TFT_GREEN : (battPct > 20 ? TFT_YELLOW : TFT_RED);
  canvas.drawRect(110, 3, 16, 8, TFT_DARKGREY);
  canvas.fillRect(126, 5, 2, 4, TFT_DARKGREY);
  int fillW = map(battPct, 0, 100, 0, 14);
  canvas.fillRect(111, 4, fillW, 6, battColor);
  
  // Draw the dog
  drawDogFace(SCREEN_WIDTH / 2, 90, currentState == STATE_SLEEPING);
  
  // Draw status bars
  drawStatusBars();
  
  // Draw state indicator
  canvas.setTextSize(1);
  switch (currentState) {
    case STATE_SLEEPING:
      canvas.setTextColor(COLOR_TIRED);
      canvas.drawString("Zzz...", 50, 150);
      break;
    case STATE_HUNGRY:
      canvas.setTextColor(COLOR_HUNGRY);
      canvas.drawString("Hungry!", 45, 150);
      break;
    case STATE_HAPPY:
      canvas.setTextColor(COLOR_HAPPY);
      canvas.drawString("Happy!", 48, 150);
      break;
    case STATE_PLAYING:
      canvas.setTextColor(TFT_YELLOW);
      canvas.drawString("Playing!", 42, 150);
      break;
    case STATE_SICK:
      canvas.setTextColor(COLOR_SICK);
      canvas.drawString("Sick...", 45, 150);
      break;
    case STATE_VOMITING:
      canvas.setTextColor(TFT_GREEN);
      canvas.drawString("*urp!*", 45, 150);
      break;
    default:
      break;
  }
  
  // Draw menu if open
  if (menuOpen) {
    drawMenu();
  } else {
    // Draw button hints
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setTextSize(1);
    canvas.drawString("A:Menu B:Wake", 20, 222);
    
    // Show sleep countdown
    unsigned long timeLeft = (SLEEP_TIMEOUT_MS - (millis() - lastInteraction)) / 1000;
    if (timeLeft < 15) {
      char sleepBuf[16];
      sprintf(sleepBuf, "Sleep: %lus", timeLeft);
      canvas.setTextColor(TFT_YELLOW);
      canvas.drawString(sleepBuf, 35, 232);
    }
  }
  
  // Push the entire canvas to display in one operation (no flicker!)
  canvas.pushSprite(0, 0);
}

// Handle menu selection
void handleMenuSelect() {
  switch (selectedMenu) {
    case MENU_FEED:
      menuOpen = false;
      feedPet();
      break;
    case MENU_PLAY:
      menuOpen = false;
      playWithPet();
      break;
    case MENU_SLEEP:
      menuOpen = false;
      putToSleep();
      break;
    case MENU_STATS:
      menuOpen = false;
      drawStatsScreen();
      break;
    default:
      break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  
  // Set up display - vertical orientation with M5 button at bottom
  M5.Lcd.setRotation(0);
  M5.Lcd.fillScreen(COLOR_BG);
  M5.Lcd.setBrightness(100);
  
  // Create sprite buffer for flicker-free drawing
  canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  canvas.setSwapBytes(true);
  
  // Set up speaker
  M5.Speaker.begin();
  M5.Speaker.setVolume(100);
  
  // Set up IMU for shake detection
  M5.Imu.begin();
  
  // Check for reset combo (hold both buttons on boot)
  M5.update();
  if (M5.BtnA.isPressed() && M5.BtnB.isPressed()) {
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(TFT_RED);
    canvas.setTextSize(1);
    canvas.drawString("RESETTING PET...", 20, 100);
    canvas.pushSprite(0, 0);
    resetPet();
    delay(1500);
    canvas.setTextColor(TFT_GREEN);
    canvas.drawString("Done! New pet!", 25, 120);
    canvas.pushSprite(0, 0);
    playHappySound();
    delay(1500);
  } else {
    // Load saved stats (applies time delta if waking from sleep)
    loadStats();
  }
  
  // Handle different wakeup scenarios
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER || wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    // Waking from deep sleep
    handleWakeup();
  } else {
    // Fresh boot - show welcome
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextSize(2);
    canvas.drawString("PUPPY", 35, 90);
    canvas.drawString("PAL", 50, 115);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Your virtual pet!", 20, 150);
    canvas.pushSprite(0, 0);
    
    playHappySound();
    delay(2000);
  }
  
  // Initialize random seed
  randomSeed(analogRead(0) + millis());
  
  lastUpdate = millis();
  lastStatDecay = millis();
  lastInteraction = millis();
}

// Check for shake
bool checkForShake() {
  M5.Imu.getAccel(&accelX, &accelY, &accelZ);
  
  // Calculate magnitude of acceleration
  shakeMagnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
  
  // If acceleration exceeds threshold and enough time has passed since last shake
  if (shakeMagnitude > SHAKE_THRESHOLD && (millis() - lastShakeTime > SHAKE_DEBOUNCE_MS)) {
    lastShakeTime = millis();
    shakeCount++;
    return true;
  }
  
  return false;
}

void loop() {
  M5.update();
  
  unsigned long currentTime = millis();
  
  // Check for shake
  if (checkForShake()) {
    lastInteraction = currentTime;  // Reset sleep timer on shake
    
    if (currentState == STATE_SLEEPING) {
      // Shake wakes up pet from sleep
      wakeUp();
    } else if (currentState != STATE_SICK && currentState != STATE_VOMITING) {
      // Shake makes pet sick (unless already sick)
      handleShakeSickness();
    }
    // If already sick, shake doesn't do anything harmful
  }
  
  // Button handling
  if (M5.BtnA.wasPressed()) {
    lastButtonPress = currentTime;
    lastInteraction = currentTime;
    buttonHoldStart = currentTime;
    buttonAHeld = true;
    
    if (menuOpen) {
      playSelectSound();
      handleMenuSelect();
    } else if (currentState != STATE_SLEEPING) {
      playMenuSound();
      menuOpen = true;
      selectedMenu = MENU_FEED;
    }
  }
  
  if (M5.BtnA.wasReleased()) {
    buttonAHeld = false;
  }
  
  // Long press A for quick feed
  if (buttonAHeld && !menuOpen && currentTime - buttonHoldStart > 1000) {
    buttonAHeld = false;  // Prevent repeated triggers
    lastInteraction = currentTime;
    feedPet();
  }
  
  if (M5.BtnB.wasPressed()) {
    lastButtonPress = currentTime;
    lastInteraction = currentTime;  // Reset interaction timer on button press
    buttonHoldStart = currentTime;
    buttonBHeld = true;
    
    if (currentState == STATE_SLEEPING) {
      wakeUp();
    } else if (menuOpen) {
      playMenuSound();
      selectedMenu = (MenuItem)((selectedMenu + 1) % MENU_COUNT);
    } else {
      // Just acknowledge button press
      playMenuSound();
    }
  }
  
  if (M5.BtnB.wasReleased()) {
    buttonBHeld = false;
  }
  
  // Long press B to put to sleep (pet sleep, not device sleep)
  if (buttonBHeld && !menuOpen && currentState != STATE_SLEEPING && currentTime - buttonHoldStart > 1500) {
    buttonBHeld = false;
    putToSleep();
    lastInteraction = currentTime;  // Reset timer when putting to sleep
  }
  
  // Close menu if no input for a while
  if (menuOpen && currentTime - lastButtonPress > 5000) {
    menuOpen = false;
    playMenuSound();
  }
  
  // Update stats every 10 seconds
  if (currentTime - lastStatDecay > 10000) {
    updateStats();
    lastStatDecay = currentTime;
  }
  
  // Update animations
  updateAnimations();
  
  // Draw screen (every 100ms to reduce flicker)
  if (currentTime - lastUpdate > 100) {
    drawScreen();
    lastUpdate = currentTime;
  }
  
    // Auto-sleep after inactivity to save battery
  // Only enter deep sleep if in IDLE state and no interaction for 60 seconds
  // Don't sleep during active states like eating, playing, etc.
  if (currentState == STATE_IDLE && currentTime - lastInteraction > SLEEP_TIMEOUT_MS) {
    enterDeepSleep();
  }
  
  // Periodic save (every 30 seconds)
  static unsigned long lastSave = 0;
  if (currentTime - lastSave > 30000) {
    saveStats();
    lastSave = currentTime;
  }
  
  delay(20);
}
