#include <esp_now.h>
#include <WiFi.h>
#include <math.h>

// --- Hardware & Trigger Settings ---
const int BUZZER_PIN = 25;             // Connect Piezo buzzer to GPIO 25 and GND
volatile bool triggerSound = false;    // Flag to signal the main loop
unsigned long lastSoundTime = 0;
const unsigned long COOLDOWN_MS = 3000; // Wait 3 seconds before playing again

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
  // Sweep up quickly (the initial blast)
  for (int freq = 400; freq <= 1200; freq += 40) {
    tone(BUZZER_PIN, freq);
    delay(10);
  }
  
  // Hold the peak
  tone(BUZZER_PIN, 1200);
  delay(150);
  
  // Sweep down with a slight "warble" effect
  for (int freq = 1200; freq >= 300; freq -= 20) {
    tone(BUZZER_PIN, freq + (freq % 60)); // The modulo adds a rumble/warble
    delay(15);
  }
  
  noTone(BUZZER_PIN); // Turn off buzzer
}

// Callback function executed when data is received
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  int rawRSSI = recv_info->rx_ctrl->rssi;

  // Apply Exponential Moving Average filter
  smoothedRSSI = (ALPHA * rawRSSI) + ((1.0 - ALPHA) * smoothedRSSI);
  float estimatedDistance = calculateDistance(smoothedRSSI);

  Serial.print("Raw: ");
  Serial.print(rawRSSI);
  Serial.print(" dBm | Filtered: ");
  Serial.print(smoothedRSSI, 1);
  Serial.print(" dBm | Est: ");
  Serial.print(estimatedDistance, 2);
  Serial.println(" m");

  // Trigger the sound flag if distance exceeds 5 meters
  if (estimatedDistance > 5.0) {
    triggerSound = true;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the receive callback function
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Check if the callback flagged a distance > 5m
  if (triggerSound) {
    triggerSound = false; // Immediately reset the flag

    // Only play if the cooldown period has passed
    if (millis() - lastSoundTime > COOLDOWN_MS) {
      Serial.println("🐘 Distance > 5m! Playing sound...");
      playElephantBuzzer();
      lastSoundTime = millis();
    }
  }
}
