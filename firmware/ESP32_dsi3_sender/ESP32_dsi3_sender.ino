#include "lcd_bsp.h"
#include "FT3168.h"
#include "lvgl.h"
#include <AiEsp32RotaryEncoder.h>

#include <WiFi.h>
#include <esp_now.h>

// ================= ESP-NOW =================
uint8_t receiverMac[] = { 0x80, 0xb5, 0x4e, 0xc6, 0xe8, 0x28 }; // <-- ÄNDRA 80:b5:4e:c6:e8:28

typedef struct {
  uint8_t type;   // 0 = MAIN, 1 = SUB
  float volume;   // dB
} VolumePacket;

VolumePacket packet;

// ================= ROTARY =================
#define ROTARY_ENCODER_A_PIN 7
#define ROTARY_ENCODER_B_PIN 8
#define ROTARY_ENCODER_BUTTON_PIN 17
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

static lv_obj_t *slider_label[2];
static lv_obj_t *slider[2];
static int activeSlider = 0; // 0 = MAIN (vänster), 1 = SUB (höger)

AiEsp32RotaryEncoder rotaryEncoder(
  ROTARY_ENCODER_A_PIN,
  ROTARY_ENCODER_B_PIN,
  ROTARY_ENCODER_BUTTON_PIN,
  ROTARY_ENCODER_VCC_PIN,
  ROTARY_ENCODER_STEPS,
  false
);

int currentValue[2] = {0, 0};
unsigned long lastEncoderMove = 0;

// --------- BUTTON SOFT-DEBOUNCE ----------
static bool lastButtonState = HIGH;
static unsigned long lastDebounceTime = 0;
static unsigned long lastSliderSwitch = 0;
const unsigned long debounceDelay = 150;
const unsigned long postEncoderLock = 100;
const unsigned long minSwitchInterval = 200;

// ========================================
static void slider_event_cb(lv_event_t *e);
static void update_label(int idx, int value);
static void sendVolume(uint8_t type, float value);

void setup() {
  Serial.begin(115200);

  // ---------- ESP-NOW ----------
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, receiverMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  // ---------- UI ----------
  Touch_Init();
  lcd_lvgl_Init();

  rotaryEncoder.begin();
  rotaryEncoder.setup([] { rotaryEncoder.readEncoder_ISR(); });
  rotaryEncoder.setBoundaries(-50, 5, false);
  rotaryEncoder.setAcceleration(45);

  pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP);

  create_sliders();
}

void loop() {
  delay(10);

  // ---------------- ROTARY ----------------
  int encoderValue = rotaryEncoder.readEncoder();
  if (encoderValue != currentValue[activeSlider]) {
    currentValue[activeSlider] = encoderValue;
    lastEncoderMove = millis();

    lv_slider_set_value(slider[activeSlider], encoderValue, LV_ANIM_OFF);
    update_label(activeSlider, encoderValue);

    sendVolume(activeSlider, encoderValue);
  }

  // ---------------- TOUCH ----------------
  int touchValue = lv_slider_get_value(slider[activeSlider]);
  if (touchValue != currentValue[activeSlider]) {
    currentValue[activeSlider] = touchValue;
    rotaryEncoder.setEncoderValue(touchValue);
    update_label(activeSlider, touchValue);

    sendVolume(activeSlider, touchValue);
  }

  // ---------------- BUTTON ----------------
  bool reading = digitalRead(ROTARY_ENCODER_BUTTON_PIN);

  if (millis() - lastEncoderMove < postEncoderLock) {
    reading = HIGH;
  }

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool lastStableState = HIGH;
    if (reading != lastStableState) {
      lastStableState = reading;
      if (lastStableState == LOW &&
          (millis() - lastSliderSwitch) > minSwitchInterval) {

        activeSlider = 1 - activeSlider;
        rotaryEncoder.setEncoderValue(currentValue[activeSlider]);
        lastSliderSwitch = millis();
      }
    }
  }

  lastButtonState = reading;
}

// ================= UI =================
void create_sliders() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

  for (int i = 0; i < 2; i++) {
    slider[i] = lv_slider_create(scr);
    lv_obj_set_size(slider[i], 40, 300);
    lv_obj_align(slider[i], LV_ALIGN_CENTER, (i == 0 ? -50 : 50), 0);
    lv_slider_set_range(slider[i], -50, 5);

    slider_label[i] = lv_label_create(scr);
    lv_obj_align(slider_label[i], LV_ALIGN_BOTTOM_MID,
                 (i == 0 ? -50 : 50), -15);

    lv_obj_add_event_cb(slider[i], slider_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        (void*)(intptr_t)i);

    update_label(i, 0);
  }
}

static void slider_event_cb(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  int value = lv_slider_get_value(lv_event_get_target(e));

  if (value != currentValue[idx]) {
    currentValue[idx] = value;
    if (idx == activeSlider) rotaryEncoder.setEncoderValue(value);
    update_label(idx, value);
    sendVolume(idx, value);
  }
}

static void update_label(int idx, int value) {
  char buf[32];
  sprintf(buf, "%s: %d dB", idx == 0 ? "MAIN" : "SUB", value);
  lv_label_set_text(slider_label[idx], buf);
}

// ================= ESP-NOW SEND =================
static void sendVolume(uint8_t type, float value) {
  packet.type = type;     // 0 = MAIN, 1 = SUB
  packet.volume = value;

  esp_now_send(receiverMac, (uint8_t*)&packet, sizeof(packet));
}
