#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define I2C_SDA 8
#define I2C_SCL 9

#define ENCODER_A 2
#define ENCODER_B 3
#define BUTTON_PIN 4

volatile int encoderPos = 0;
int lastEncoded = 0;

const int NUM_CHANNELS = 12;
int currentChannel = 0;
float volume[NUM_CHANNELS]; // -60 till 0 dB

void IRAM_ATTR updateEncoder() {
  int MSB = digitalRead(ENCODER_A);
  int LSB = digitalRead(ENCODER_B);
  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;

  // Quadrature decoding
  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;

  lastEncoded = encoded;
}

void setup() {
  Serial.begin(115200);

  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), updateEncoder, CHANGE);

  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  for(int i=0;i<NUM_CHANNELS;i++) volume[i] = -20.0; // startvärde
  lastEncoded = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Ready");
  display.display();
}

void loop() {
  static int lastEncoderPos = 0;
  static bool lastButtonState = HIGH;

  // --- Byt kanal ---
  bool buttonState = digitalRead(BUTTON_PIN);
  if(buttonState == LOW && lastButtonState == HIGH) {
    currentChannel++;
    if(currentChannel >= NUM_CHANNELS) currentChannel = 0;
  }
  lastButtonState = buttonState;

  // --- Ändra volym ---
  int delta = encoderPos - lastEncoderPos;
  if(delta != 0) {
    volume[currentChannel] += delta * 0.5; // 0.5 dB per klick
    if(volume[currentChannel] > 0) volume[currentChannel] = 0;
    if(volume[currentChannel] < -60) volume[currentChannel] = -60;

    // Skicka senaste värdet till Python
    Serial.print("CH");
    Serial.print(currentChannel+1);
    Serial.print("=");
    Serial.println(volume[currentChannel]);

    lastEncoderPos = encoderPos;
  }

  // --- OLED visning ---
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.print("CH");
  display.print(currentChannel+1);
  display.print(": ");
  display.print(volume[currentChannel],1);
  display.println("dB");
  display.display();
}
