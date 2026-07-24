#include <esp_now.h>
#include <WiFi.h>
#include <math.h>

// Path Loss Calibration Parameters
const float MEASURED_POWER_1M = -43.0; // RSSI value measured at 1 meter distance
const float PATH_LOSS_EXPONENT = 2.7;   // Environment constant (2.0 = open space, 3.0 = indoors)

// Signal Filtering
float smoothedRSSI = -60.0;
const float ALPHA = 0.15; // Smoothing factor (0.0 to 1.0)

// Function to calculate distance from RSSI
float calculateDistance(float rssi) {
  return pow(10.0, (MEASURED_POWER_1M - rssi) / (10.0 * PATH_LOSS_EXPONENT));
}

// Callback function executed when data is received
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  int rawRSSI = recv_info->rx_ctrl->rssi;

  // Apply Exponential Moving Average filter
  smoothedRSSI = (ALPHA * rawRSSI) + ((1.0 - ALPHA) * smoothedRSSI);

  float estimatedDistance = calculateDistance(smoothedRSSI);

  Serial.print("Raw RSSI: ");
  Serial.print(rawRSSI);
  Serial.print(" dBm | Filtered RSSI: ");
  Serial.print(smoothedRSSI, 1);
  Serial.print(" dBm | Est. Distance: ");
  Serial.print(estimatedDistance, 2);
  Serial.println(" meters");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register the receive callback function
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  // Main loop free for display driving, telemetry, or actions based on distance
}