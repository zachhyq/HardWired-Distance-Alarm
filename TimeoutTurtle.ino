// ==========================================
// PIN DEFINITIONS
// ==========================================
const int POT_PIN      = 34; // Potentiometer signal (ADC1)
const int BUTTON_PIN   = 18; // Push button (Input with internal pull-up)
const int BUZZER_PIN   = 25; // Passive buzzer pin
const int LDR_PIN      = 35; // LDR Photoresistor pin (ADC1)

// ==========================================
// THRESHOLDS & TONE FREQUENCIES
// ==========================================
const int LIGHT_THRESHOLD = 2000; // Above = Dark (Covered), Below = Light (Exposed)

// Alarm Frequencies (in Hz)
const int TONE_LOW  = 800;  
const int TONE_HIGH = 2000; 

// ==========================================
// SYSTEM STATES
// ==========================================
enum SystemState {
  IDLE,       // Setting up timer
  ACTIVE,     // Study session active, monitoring LDR
  ALARM,      // Phone picked up! Siren sounding
  FINISHED    // Timer completed successfully
};

SystemState currentState = IDLE;

unsigned long sessionStartTime = 0;
unsigned long sessionDurationMs = 0;

// Button debounce helper
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; 

// Alarm sound timing helper
unsigned long lastSirenToggle = 0;
bool sirenPitchHigh = false;

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);
  
  // Ensure buzzer starts completely silent
  noTone(BUZZER_PIN); 
  
  Serial.println("--- Initialized ---");
}

void loop() {
  bool currentButtonReading = digitalRead(BUTTON_PIN);
  bool buttonPressed = false;

  // Handle button debounce
  if (currentButtonReading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    static bool buttonState = HIGH;
    if (currentButtonReading != buttonState) {
      buttonState = currentButtonReading;
      if (buttonState == LOW) {
        buttonPressed = true;
      }
    }
  }
  lastButtonState = currentButtonReading;

  // Read LDR light intensity
  int ldrValue = analogRead(LDR_PIN);

  // ==========================================
  // STATE MACHINE LOGIC
  // ==========================================
  switch (currentState) {

    // 1. SETTING THE TIMER
    case IDLE: {
      noTone(BUZZER_PIN); // Keep silent

      int potVal = analogRead(POT_PIN);
      int targetMinutes = map(potVal, 0, 4095, 1, 60);

      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 1000) {
        Serial.print("Set duration: ");
        Serial.print(targetMinutes);
        Serial.print(" min | Current LDR Readout: ");
        Serial.println(ldrValue);
        lastPrint = millis();
      }

      if (buttonPressed) {
        sessionDurationMs = (unsigned long)targetMinutes * 60 * 1000;
        sessionStartTime = millis();
        currentState = ACTIVE;

        // Friendly start chime (Low pitch -> High pitch)
        tone(BUZZER_PIN, 1000, 100);
        delay(120);
        tone(BUZZER_PIN, 2000, 150);

        Serial.println(">> SESSION STARTED! Keep your phone over the LDR. <<");
      }
      break;
    }

    // 2. MONITORING STUDY SESSION
    case ACTIVE: {
      unsigned long elapsedTime = millis() - sessionStartTime;

      // Timer completed successfully
      if (elapsedTime >= sessionDurationMs) {
        currentState = FINISHED;
        Serial.println(">> SUCCESS! Study session completed! <<");
        
        // Victory fanfare melody
        tone(BUZZER_PIN, 523, 150); delay(200); // Note C5
        tone(BUZZER_PIN, 659, 150); delay(200); // Note E5
        tone(BUZZER_PIN, 784, 300); delay(350); // Note G5
        noTone(BUZZER_PIN);
        break;
      }

      // Check if phone was removed
      if (ldrValue < LIGHT_THRESHOLD) {
        currentState = ALARM;
        Serial.print("!! ALARM: Light detected! (LDR Value: ");
        Serial.print(ldrValue);
        Serial.println(") !!");
      }

      // Manual cancel via button
      if (buttonPressed) {
        currentState = IDLE;
        Serial.println("Session canceled by user.");
      }
      break;
    }

    // 3. ALARM STATE (Two-Tone Siren)
    case ALARM: {
      // Non-blocking alternating siren tone (flips pitch every 150ms)
      if (millis() - lastSirenToggle >= 150) {
        lastSirenToggle = millis();
        sirenPitchHigh = !sirenPitchHigh;
        
        if (sirenPitchHigh) {
          tone(BUZZER_PIN, TONE_HIGH);
        } else {
          tone(BUZZER_PIN, TONE_LOW);
        }
      }

      // Condition 1: Reset by pressing button
      if (buttonPressed) {
        noTone(BUZZER_PIN);
        currentState = IDLE;
        Serial.println("Alarm reset by button press.");
        break;
      }

      // Condition 2: Reset/Mute by putting phone back over LDR
      if (ldrValue >= LIGHT_THRESHOLD) {
        noTone(BUZZER_PIN);
        currentState = ACTIVE; // Resumes counting down
        Serial.println("Phone placed back! Alarm muted, session resumed.");
        break;
      }
      break;
    }

    // 4. FINISHED STATE
    case FINISHED: {
      noTone(BUZZER_PIN);

      if (buttonPressed) {
        currentState = IDLE;
      }
      break;
    }
  }
}