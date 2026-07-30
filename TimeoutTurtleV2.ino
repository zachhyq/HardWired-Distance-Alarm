// ==========================================
// PIN DEFINITIONS & INCLUDES
// ==========================================
#include <WiFi.h>
#include <WebServer.h>

const int POT_PIN      = 34; // Potentiometer signal (ADC1)
const int BUTTON_PIN   = 18; // Push button (Input with internal pull-up)
const int BUZZER_PIN   = 25; // Passive buzzer pin
const int LDR_PIN      = 35; // LDR Photoresistor pin (ADC1)

// ==========================================
// ACCESS POINT CONFIG (ESP32 HOTSPOT)
// ==========================================
const char* ap_ssid = "TimeoutTurtle"; // <-- Wi-Fi name your ESP32 broadcasts
const char* ap_password = "ggsturtle";     // <-- Password to connect (must be >= 8 chars)

WebServer server(80);

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
  ARMING,     // 30-second grace period before alarm arms
  ACTIVE,     // Study session active, monitoring LDR
  ALARM,      // Phone picked up! Siren sounding
  FINISHED    // Timer completed successfully
};

SystemState currentState = IDLE;

// Timer tracking variables for pause/resume capability
unsigned long remainingTimeMs = 0;
unsigned long lastUpdateTime = 0; 
int globalTargetMinutes = 1; // Tracked globally for the web server

// Arming delay variables
unsigned long armingStartTime = 0;
const unsigned long ARMING_DELAY_MS = 30000; // 30 seconds

// Button debounce helper
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; 

// Alarm sound timing helper
unsigned long lastSirenToggle = 0;
bool sirenPitchHigh = false;

// ==========================================
// WEB SERVER FUNCTIONS
// ==========================================

// Serves the main HTML webpage
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Study Timer Dashboard</title>
    <style>
      body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #f4f4f9; color: #333; margin-top: 50px; }
      .card { background: white; padding: 30px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); display: inline-block; min-width: 300px; }
      h1 { color: #444; margin-bottom: 20px; }
      .metric { font-size: 32px; font-weight: bold; color: #007bff; margin: 10px 0; }
      .label { font-size: 14px; color: #888; text-transform: uppercase; letter-spacing: 1px; margin-top: 15px; }
      .status { padding: 6px 14px; border-radius: 20px; font-weight: bold; display: inline-block; margin-bottom: 15px;}
      .status.IDLE { background: #e2e3e5; color: #383d41; }
      .status.ARMING { background: #fff3cd; color: #856404; }
      .status.ACTIVE { background: #d4edda; color: #155724; }
      .status.ALARM { background: #f8d7da; color: #721c24; }
      .status.FINISHED { background: #cce5ff; color: #004085; }
    </style>
    <script>
      // Fetch new data from the ESP32 every 1 second
      setInterval(() => {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            document.getElementById('state').innerText = data.state;
            document.getElementById('state').className = 'status ' + data.state;
            document.getElementById('target').innerText = data.target + ' min';
            
            if(data.state === 'ACTIVE' || data.state === 'ALARM' || data.state === 'ARMING') {
              let mins = Math.floor(data.remaining / 60000);
              let secs = Math.floor((data.remaining % 60000) / 1000);
              secs = secs < 10 ? '0' + secs : secs;
              document.getElementById('time').innerText = mins + ':' + secs;
            } else if (data.state === 'FINISHED') {
              document.getElementById('time').innerText = '0:00';
            } else {
              document.getElementById('time').innerText = '--:--';
            }
          });
      }, 1000);
    </script>
  </head>
  <body>
    <div class="card">
      <h1>Study Timer</h1>
      <div id="state" class="status IDLE">Loading...</div>
      
      <div class="label">Potentiometer Dial Set To</div>
      <div id="target" class="metric">--</div>
      
      <div class="label">Time Remaining</div>
      <div id="time" class="metric">--:--</div>
    </div>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

// Serves the live data as JSON for the webpage
void handleData() {
  String stateStr = "";
  unsigned long remaining = 0;
  
  if (currentState == IDLE) {
    stateStr = "IDLE";
  } else if (currentState == ARMING) {
    stateStr = "ARMING";
    unsigned long elapsed = millis() - armingStartTime;
    if (ARMING_DELAY_MS > elapsed) {
      remaining = ARMING_DELAY_MS - elapsed;
    }
  } else if (currentState == ACTIVE || currentState == ALARM) {
    stateStr = (currentState == ACTIVE) ? "ACTIVE" : "ALARM";
    // Send the currently stored remaining time (which naturally pauses during ALARM)
    remaining = remainingTimeMs;
  } else if (currentState == FINISHED) {
    stateStr = "FINISHED";
  }

  String json = "{";
  json += "\"state\":\"" + stateStr + "\",";
  json += "\"target\":" + String(globalTargetMinutes) + ",";
  json += "\"remaining\":" + String(remaining);
  json += "}";
  
  server.send(200, "application/json", json);
}

// ==========================================
// SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);
  
  noTone(BUZZER_PIN); 
  
  // 1. Configure ESP32 as a Wi-Fi Access Point (Hotspot)
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.println("\n--- Access Point Started! ---");
  Serial.print("1. Connect your Laptop/Phone to Wi-Fi: ");
  Serial.println(ap_ssid);
  Serial.print("2. Enter Password: ");
  Serial.println(ap_password);
  Serial.print("3. Open browser and go to IP: http://");
  Serial.println(apIP);

  // 2. Start Web Server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  // Listen for web requests
  server.handleClient();

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

  int ldrValue = analogRead(LDR_PIN);

  // Continuously read potentiometer so web server updates live dial position
  int potVal = analogRead(POT_PIN);
  globalTargetMinutes = map(potVal, 0, 4095, 1, 60);

  // ==========================================
  // STATE MACHINE LOGIC
  // ==========================================
  switch (currentState) {

    case IDLE: {
      noTone(BUZZER_PIN);

      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 1000) {
        Serial.print("Set duration: ");
        Serial.print(globalTargetMinutes);
        Serial.print(" min | Current LDR Readout: ");
        Serial.println(ldrValue);
        lastPrint = millis();
      }

      if (buttonPressed) {
        armingStartTime = millis();
        currentState = ARMING;

        tone(BUZZER_PIN, 1000, 100);
        Serial.println(">> ARMING... You have 30 seconds to place your phone over the LDR. <<");
      }
      break;
    }

    case ARMING: {
      unsigned long elapsedArmingTime = millis() - armingStartTime;

      if (elapsedArmingTime >= ARMING_DELAY_MS) {
        // Arming over. Calculate total milliseconds needed and start the update clock.
        remainingTimeMs = (unsigned long)globalTargetMinutes * 60 * 1000;
        lastUpdateTime = millis();
        currentState = ACTIVE;

        // Two beeps to indicate the alarm is live
        tone(BUZZER_PIN, 1000, 100);
        delay(120);
        tone(BUZZER_PIN, 2000, 150);

        Serial.println(">> SESSION STARTED! LDR is now armed. <<");
        break;
      }

      if (buttonPressed) {
        currentState = IDLE;
        Serial.println("Arming canceled by user.");
      }
      break;
    }

    case ACTIVE: {
      // Calculate how much time has passed since the last loop iteration
      unsigned long now = millis();
      unsigned long elapsedSinceLastUpdate = now - lastUpdateTime;
      lastUpdateTime = now; // update baseline for the next loop

      // Check if session is finished
      if (elapsedSinceLastUpdate >= remainingTimeMs) {
        remainingTimeMs = 0;
        currentState = FINISHED;
        Serial.println(">> SUCCESS! Study session completed! <<");
        
        tone(BUZZER_PIN, 523, 150); delay(200); 
        tone(BUZZER_PIN, 659, 150); delay(200); 
        tone(BUZZER_PIN, 784, 300); delay(350); 
        noTone(BUZZER_PIN);
        break;
      } else {
        // Decrement remaining time
        remainingTimeMs -= elapsedSinceLastUpdate;
      }

      if (ldrValue < LIGHT_THRESHOLD) {
        currentState = ALARM;
        Serial.print("!! ALARM: Light detected! Timer paused at ");
        Serial.print(remainingTimeMs / 1000);
        Serial.println(" seconds remaining !!");
      }

      if (buttonPressed) {
        currentState = IDLE;
        Serial.println("Session canceled by user.");
      }
      break;
    }

    case ALARM: {
      // While in this state, remainingTimeMs is NOT decremented.

      if (millis() - lastSirenToggle >= 150) {
        lastSirenToggle = millis();
        sirenPitchHigh = !sirenPitchHigh;
        
        if (sirenPitchHigh) {
          tone(BUZZER_PIN, TONE_HIGH);
        } else {
          tone(BUZZER_PIN, TONE_LOW);
        }
      }

      if (buttonPressed) {
        noTone(BUZZER_PIN);
        currentState = IDLE;
        Serial.println("Alarm reset by button press.");
        break;
      }

      if (ldrValue >= LIGHT_THRESHOLD) {
        noTone(BUZZER_PIN);
        
        // CRITICAL: Reset the 'lastUpdateTime' so it doesn't subtract the time 
        // spent alarming during the next ACTIVE loop iteration.
        lastUpdateTime = millis(); 
        
        currentState = ACTIVE; 
        Serial.println("Phone placed back! Alarm muted, session resumed.");
        break;
      }
      break;
    }

    case FINISHED: {
      noTone(BUZZER_PIN);

      if (buttonPressed) {
        currentState = IDLE;
      }
      break;
    }
  }
}
