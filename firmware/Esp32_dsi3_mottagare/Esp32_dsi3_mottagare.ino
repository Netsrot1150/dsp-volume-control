#include <WiFi.h>
#include <esp_now.h>

#define NUM_CHANNELS 12

// ===== Paket från sender =====
typedef struct __attribute__((packed)) {
  int8_t mainVolume;   // dB
  int16_t subVolume;   // 0.1 dB units
} VolumePacket;

VolumePacket incoming;
float volume[NUM_CHANNELS]; // kan behållas float för Python / serial

// ===== ESP-NOW RX =====
void onDataRecv(const esp_now_recv_info_t *info,
                const uint8_t *data,
                int len)
{
  if (len != sizeof(VolumePacket)) return;

  memcpy(&incoming, data, sizeof(incoming));

  // ===== MAIN → CH1–CH10 =====
  for (int i = 0; i < 10; i++) {
    if (volume[i] != incoming.mainVolume) {
      volume[i] = incoming.mainVolume;
      Serial.print("CH");
      Serial.print(i + 1);
      Serial.print("=");
      Serial.println(volume[i]);
    }
  }

  // ===== SUB → CH11 & CH12 =====
  for (int i = 10; i < 12; i++) {
   float sub_dB = incoming.subVolume / 10.0f;  // konvertera till dB
   if (volume[i] != sub_dB) {
      volume[i] = sub_dB;
     Serial.print("CH");
     Serial.print(i + 1);
     Serial.print("=");
     Serial.println(volume[i], 1);  // 1 decimal
   }
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    volume[i] = 999; // force first update
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("ESP-NOW receiver → Python bridge ready");
}

void loop() {
  // Tom
}
