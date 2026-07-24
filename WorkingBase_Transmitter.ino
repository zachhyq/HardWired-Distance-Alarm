#include <esp_now.h>
#include <WiFi.h>

// Replace with Receiver's MAC Address if targeted, or use Broadcast Address
uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message {
  uint32_t count;
} struct_message;

struct_message pingData;
esp_now_peer_info_t peerInfo;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  pingData.count++;
  esp_now_send(receiverAddress, (uint8_t *) &pingData, sizeof(pingData));
  delay(100); // Send every 100ms for fast sampling
}