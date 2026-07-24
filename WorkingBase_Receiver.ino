#include <esp_now.h>
#include <WiFi.h>
#include <math.h>

// --- Hardware & Trigger Settings ---
const int BUZZER_PIN = 25;             // Connect Piezo buzzer to GPIO 25 and GND
const int BUTTON_PIN = 4;              // Connect button between GPIO 4 and GND

volatile bool triggerSound = false;    
bool alarmArmed = false;               // System starts OFF

// --- Timing Variables ---
unsigned long lastSoundTime = 0;
const unsigned long COOLDOWN_MS = 3000; 
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 250; // Milliseconds to ignore contact bounce
int lastButtonState = HIGH;            // HIGH means unpressed (using INPUT_PULLUP)

// --- Path Loss Calibration Parameters ---
const float MEASURED_POWER_1M = -43.0; 
const float PATH_LOSS_EXPONENT = 2.7;  

// --- Signal Filtering ---
float smoothedRSSI = -60.0;
const float ALPHA = 0.15; 

// Calculate distance from RSSI
float calculateDistance(float rssi) {
  return pow(10.0, (MEASURED_POWER_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
}

// Function to simulate an elephant trumpet via Piezo
void playElephantBuzzer() {
  for (int freq = 400; freq <= 1200; freq += 40) {
    tone(BUZZER_PIN, freq);
    delay(10);
  }
  tone(BUZZER_PIN, 1200);
  delay(150);
  for (int freq = 1200; freq >= 300; freq -= 20) {
    tone(BUZZER_PIN, freq + (freq % 60)); 
    delay(15);
  }
  noTone(BUZZER_PIN); 
}

// Callback function executed when data is received
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  int rawRSSI = recv_info->rx_ctrl->rssi;

  smoothedRSSI = (ALPHA * rawRSSI) + ((1.0 - ALPHA) * smoothedRSSI);
  float estimatedDistance = calculateDistance(smoothedRSSI);

  // We only print telemetry if the alarm is actually armed
  if (alarmArmed) {
    Serial.print("Filtered RSSI: ");
    Serial.print(smoothedRSSI, 1);
    Serial.print(" dBm | Est Distance: ");
    Serial.print(estimatedDistance, 2);
    Serial.println(" m");
  }

  // Trigger the sound flag if distance exceeds 5 meters
  if (estimatedDistance > 5.0) {
    triggerSound = true;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // INPUT_PULLUP uses the ESP32's internal resistor
  // The pin will read HIGH normally, and LOW when the button is pressed
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("System Ready. Press the button to arm the alarm.");
}

void loop() {
  // 1. Read the button and handle debouncing
  int currentButtonState = digitalRead(BUTTON_PIN);
  
  // If the button changed from HIGH (unpressed) to LOW (pressed)
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    if (millis() - lastButtonPress > DEBOUNCE_DELAY) {
      alarmArmed = !alarmArmed; // Toggle the state
      
      Serial.print("ALARM SYSTEM IS NOW: ");
      Serial.println(alarmArmed ? "ARMED" : "DISARMED");
      
      lastButtonPress = millis();
    }
  }
  lastButtonState = currentButtonState;

  // 2. Check if the callback flagged a distance > 5m
  if (triggerSound) {
    triggerSound = false; // Immediately reset the flag

    // Only play if the alarm is ARMED and cooldown period has passed
    if (alarmArmed && (millis() - lastSoundTime > COOLDOWN_MS)) {
      Serial.println("🐘 Intruder detected! Playing sound...");
      playElephantBuzzer();
      lastSoundTime = millis();
    }
  }
}
