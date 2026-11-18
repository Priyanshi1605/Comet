/*
 * Comet Desktop Companion Robot - Main Firmware (ESP32-S3)
 * Hardware: ILI9341 2.4" TFT, APDS-9960 Gesture Sensor, Dual SG90 Servos,
 *           TB6612FNG + Dual N20 Motors, MAX98357A I2S Amp, Capacitive Touch Pads
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_APDS9960.h>
#include <ESP32Servo.h>
#include <driver/i2s.h>
#include <math.h>

// -------------------- PIN DEFINITIONS --------------------
// 2.4" ILI9341 TFT Display (SPI)
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  14
#define TFT_MOSI 11
#define TFT_SCK  12

// APDS-9960 Gesture & RGB Sensor (I2C)
#define I2C_SDA  8
#define I2C_SCL  18
#define APDS_INT 17

// SG90 Arm Servos (PWM)
#define SERVO_L_PIN 4
#define SERVO_R_PIN 5

// Capacitive Copper Foil Touch Pads
#define TOUCH_HEAD_PIN  1
#define TOUCH_EAR_L_PIN 2
#define TOUCH_EAR_R_PIN 3

// TB6612FNG Dual Motor Driver (N20 Gear Motors)
#define MOTOR_PWMA 38
#define MOTOR_AIN1 39
#define MOTOR_AIN2 40
#define MOTOR_PWMB 41
#define MOTOR_BIN1 42
#define MOTOR_BIN2 21

// MAX98357A I2S Audio Amplifier
#define I2S_BCLK 15
#define I2S_LRC  16
#define I2S_DIN  7

// -------------------- OBJECTS & STATE --------------------
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Adafruit_APDS9960 apds;
Servo leftArm;
Servo rightArm;

enum Mood { NEUTRAL, HAPPY, SURPRISED, SLEEPY };
Mood currentMood = NEUTRAL;

unsigned long lastBlinkTime = 0;
unsigned long moodResetTime = 0;
const int TOUCH_THRESHOLD = 35000; // Calibrate during hardware testing

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);

  // Initialize SPI & TFT Display
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.begin();
  tft.setRotation(1); // Landscape orientation (320x240)
  tft.fillScreen(ILI9341_BLACK);

  // Initialize I2C & APDS-9960 Gesture Sensor
  Wire.begin(I2C_SDA, I2C_SCL);
  if (apds.begin()) {
    apds.enableProximity(true);
    apds.enableGesture(true);
    Serial.println("APDS-9960 initialized.");
  } else {
    Serial.println("APDS-9960 not detected.");
  }

  // Initialize Arm Servos (Neutral resting position)
  leftArm.attach(SERVO_L_PIN);
  rightArm.attach(SERVO_R_PIN);
  setArms(90, 90);

  // Initialize TB6612FNG Motor Pins
  pinMode(MOTOR_PWMA, OUTPUT);
  pinMode(MOTOR_AIN1, OUTPUT);
  pinMode(MOTOR_AIN2, OUTPUT);
  pinMode(MOTOR_PWMB, OUTPUT);
  pinMode(MOTOR_BIN1, OUTPUT);
  pinMode(MOTOR_BIN2, OUTPUT);
  stopMotors();

  // Initialize I2S Audio
  setupI2S();

  // Boot Animation & Greeting
  drawEyes(HAPPY);
  playChirp(880, 120);
  delay(150);
  playChirp(1175, 180);
  waveArms();
  drawEyes(NEUTRAL);
}

// -------------------- MAIN LOOP --------------------
void loop() {
  handleGestures();
  handleTouchPads();

  // Return to neutral mood after temporary emotion
  if (currentMood != NEUTRAL && millis() > moodResetTime) {
    currentMood = NEUTRAL;
    drawEyes(NEUTRAL);
  }

  // Natural idle blinking every 3.5 seconds
  if (currentMood == NEUTRAL && millis() - lastBlinkTime > 3500) {
    blinkEyes();
    lastBlinkTime = millis();
  }

  delay(20);
}

// -------------------- GESTURE & TOUCH HANDLERS --------------------
void handleGestures() {
  if (!apds.gestureValid()) return;
  uint8_t gesture = apds.readGesture();

  switch (gesture) {
    case APDS9960_UP:
      triggerMood(HAPPY, 2500);
      playChirp(1046, 150);
      waveArms();
      break;
    case APDS9960_DOWN:
      triggerMood(SLEEPY, 2500);
      playChirp(523, 200);
      setArms(120, 60);
      break;
    case APDS9960_LEFT:
      turnRobot(-1, 160, 350);
      break;
    case APDS9960_RIGHT:
      turnRobot(1, 160, 350);
      break;
  }
}

void handleTouchPads() {
  if (touchRead(TOUCH_HEAD_PIN) > TOUCH_THRESHOLD) {
    triggerMood(HAPPY, 2500);
    playChirp(1318, 120);
    waveArms();
  } else if (touchRead(TOUCH_EAR_L_PIN) > TOUCH_THRESHOLD) {
    triggerMood(SURPRISED, 2000);
    leftArm.write(40);
    playChirp(987, 120);
  } else if (touchRead(TOUCH_EAR_R_PIN) > TOUCH_THRESHOLD) {
    triggerMood(SURPRISED, 2000);
    rightArm.write(140);
    playChirp(987, 120);
  }
}

void triggerMood(Mood m, unsigned long durationMs) {
  currentMood = m;
  moodResetTime = millis() + durationMs;
  drawEyes(m);
}

// -------------------- DISPLAY EXPRESSIONS --------------------
void drawEyes(Mood m) {
  tft.fillScreen(ILI9341_BLACK);
  uint16_t eyeColor = ILI9341_CYAN;

  if (m == NEUTRAL) {
    tft.fillRoundRect(65, 75, 65, 85, 16, eyeColor);
    tft.fillRoundRect(190, 75, 65, 85, 16, eyeColor);
  } else if (m == HAPPY) {
    tft.fillRoundRect(65, 75, 65, 85, 16, eyeColor);
    tft.fillRoundRect(190, 75, 65, 85, 16, eyeColor);
    tft.fillCircle(97, 150, 38, ILI9341_BLACK);
    tft.fillCircle(222, 150, 38, ILI9341_BLACK);
  } else if (m == SURPRISED) {
    tft.fillCircle(97, 118, 38, eyeColor);
    tft.fillCircle(222, 118, 38, eyeColor);
  } else if (m == SLEEPY) {
    tft.fillRoundRect(65, 112, 65, 16, 6, eyeColor);
    tft.fillRoundRect(190, 112, 65, 16, 6, eyeColor);
  }
}

void blinkEyes() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRoundRect(65, 114, 65, 12, 4, ILI9341_CYAN);
  tft.fillRoundRect(190, 114, 65, 12, 4, ILI9341_CYAN);
  delay(110);
  drawEyes(NEUTRAL);
}

// -------------------- SERVOS & DRIVE MOTORS --------------------
void setArms(int leftAngle, int rightAngle) {
  leftArm.write(leftAngle);
  rightArm.write(rightAngle);
}

void waveArms() {
  for (int i = 0; i < 2; i++) {
    setArms(45, 135);
    delay(220);
    setArms(110, 70);
    delay(220);
  }
  setArms(90, 90);
}

void turnRobot(int dir, int speed, int durationMs) {
  digitalWrite(MOTOR_AIN1, dir > 0 ? HIGH : LOW);
  digitalWrite(MOTOR_AIN2, dir > 0 ? LOW : HIGH);
  digitalWrite(MOTOR_BIN1, dir > 0 ? LOW : HIGH);
  digitalWrite(MOTOR_BIN2, dir > 0 ? HIGH : LOW);
  analogWrite(MOTOR_PWMA, speed);
  analogWrite(MOTOR_PWMB, speed);
  delay(durationMs);
  stopMotors();
}

void stopMotors() {
  analogWrite(MOTOR_PWMA, 0);
  analogWrite(MOTOR_PWMB, 0);
  digitalWrite(MOTOR_AIN1, LOW);
  digitalWrite(MOTOR_AIN2, LOW);
  digitalWrite(MOTOR_BIN1, LOW);
  digitalWrite(MOTOR_BIN2, LOW);
}

// -------------------- I2S AUDIO TONE --------------------
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void playChirp(int freqHz, int durationMs) {
  int totalSamples = (16000 * durationMs) / 1000;
  int16_t sample;
  size_t bytesWritten;
  for (int i = 0; i < totalSamples; i++) {
    sample = (int16_t)(6000.0 * sin(2.0 * PI * freqHz * i / 16000.0));
    i2s_write(I2S_NUM_0, &sample, sizeof(sample), &bytesWritten, portMAX_DELAY);
  }
}
