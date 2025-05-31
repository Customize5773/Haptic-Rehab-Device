#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define PWM6        OCR4D
#define PWM13       OCR4A

#define PCA9548_ADDRESS 0x70
#define ALPHA 0.96

Adafruit_MPU6050 mpu;
Adafruit_MPU6050 mpu2;

// Define I2C channels
const uint8_t imu_channel = 0;
const uint8_t imu_channel2 = 2;
const uint8_t drv1_channel = 7;
const uint8_t drv2_channel = 5;
const int buttonPin = 7;      // Pin to detect HIGH signal

byte DRV = 0x5A;
byte ModeReg = 0x01;

float angleX = 0.0, angleY = 0.0, angleZ = 0.0;
float gyroBiasX = 0.0, gyroBiasY = 0.0, gyroBiasZ = 0.0;

float angleX2 = 0.0, angleY2 = 0.0, angleZ2 = 0.0;
float gyroBiasX2 = 0.0, gyroBiasY2 = 0.0, gyroBiasZ2 = 0.0;

unsigned long lastUpdate1 = 0;
unsigned long lastUpdate2 = 0;
unsigned long lastPrint = 0;

bool drv1_pulsing = false;
bool drv2_pulsing = false;
unsigned long pulseDuration = 30;
unsigned long pulseStartTime = 0;

float first_threshold = 0.15;
float second_threshold = 0.3;

void selectChannel(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(PCA9548_ADDRESS);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

void setup() {
  
  Serial.begin(9600);
  Wire.begin();

  pinMode(buttonPin, INPUT);   // Set pin 7 as input for the button

  selectChannel(drv1_channel);
  standbyOnB();
  selectChannel(drv2_channel);
  standbyOnB();

  selectChannel(imu_channel);
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip on channel 0");
    while (1);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);
  delay(1000);
  calibrateGyro();

  selectChannel(imu_channel2);
  if (!mpu2.begin()) {
    Serial.println("Failed to find MPU6050 chip on channel 2");
    while (1);
  }
  mpu2.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu2.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu2.setFilterBandwidth(MPU6050_BAND_5_HZ);
  delay(1000);
  calibrateGyro2();

  selectChannel(drv1_channel);
  initializeDRV2605();
  standbyOnB();

  selectChannel(drv2_channel);
  initializeDRV2605();
  standbyOnB();

  pwm613configure();
}

void loop() {
  int inputState = digitalRead(buttonPin);  // Read the button input pin

  selectChannel(imu_channel);
  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  updateAngularPosition(accel, gyro);

  selectChannel(imu_channel2);
  sensors_event_t accel2, gyro2, temp2;
  mpu2.getEvent(&accel2, &gyro2, &temp2);
  updateAngularPosition2(accel2, gyro2);

  if (millis() - lastPrint >= 200) {
    lastPrint = millis();
    Serial.print(inputState); Serial.print(",");
    Serial.print(angleX); Serial.print(",");
    Serial.print(angleY); Serial.print(",");
    Serial.print(angleZ); Serial.print(",");
    Serial.print(angleX2); Serial.print(",");
    Serial.print(angleY2); Serial.print(",");
    Serial.println(angleZ2);
  }

  if (angleY > first_threshold && angleY < second_threshold && !drv1_pulsing) {
    selectChannel(drv2_channel);
    pulse(0.25, pulseDuration);
    drv1_pulsing = true;
    pulseStartTime = millis();
  }
  else if (angleY > second_threshold && !drv1_pulsing) {
    selectChannel(drv2_channel);
    pulse(1.0, pulseDuration);
    drv2_pulsing = true;
    pulseStartTime = millis();
  }
  else if (angleY < -first_threshold && angleY > -second_threshold && !drv2_pulsing) {
    selectChannel(drv1_channel);
    pulse(0.1, pulseDuration);
    drv2_pulsing = true;
    pulseStartTime = millis();
  }
  else if (angleY < -second_threshold && !drv2_pulsing) {
    selectChannel(drv1_channel);
    pulse(1.0, pulseDuration);
    drv2_pulsing = true;
    pulseStartTime = millis();
  }

  if (drv1_pulsing && millis() - pulseStartTime > pulseDuration) {
    drv1_pulsing = false;
  }
  if (drv2_pulsing && millis() - pulseStartTime > pulseDuration) {
    drv2_pulsing = false;
  }
}

void updateAngularPosition(sensors_event_t accel, sensors_event_t gyro) {
  unsigned long now = millis();
  float dt = (now - lastUpdate1) / 1000.0;
  lastUpdate1 = now;

  float gx = gyro.gyro.x - gyroBiasX;
  float gy = gyro.gyro.y - gyroBiasY;
  float gz = gyro.gyro.z - gyroBiasZ;

  angleX += gx * dt;
  angleY += gy * dt;
  angleZ += gz * dt;

  float accelAngleX = atan2(accel.acceleration.y, accel.acceleration.z);
  float accelAngleY = atan2(-accel.acceleration.x,
                            sqrt(accel.acceleration.y * accel.acceleration.y +
                                 accel.acceleration.z * accel.acceleration.z));

  angleX = ALPHA * angleX + (1 - ALPHA) * accelAngleX;
  angleY = ALPHA * angleY + (1 - ALPHA) * accelAngleY;
}

void updateAngularPosition2(sensors_event_t accel, sensors_event_t gyro) {
  unsigned long now = millis();
  float dt = (now - lastUpdate2) / 1000.0;
  lastUpdate2 = now;

  float gx = gyro.gyro.x - gyroBiasX2;
  float gy = gyro.gyro.y - gyroBiasY2;
  float gz = gyro.gyro.z - gyroBiasZ2;

  angleX2 += gx * dt;
  angleY2 += gy * dt;
  angleZ2 += gz * dt;

  float accelAngleX = atan2(accel.acceleration.y, accel.acceleration.z);
  float accelAngleY = atan2(-accel.acceleration.x,
                            sqrt(accel.acceleration.y * accel.acceleration.y +
                                 accel.acceleration.z * accel.acceleration.z));

  angleX2 = ALPHA * angleX2 + (1 - ALPHA) * accelAngleX;
  angleY2 = ALPHA * angleY2 + (1 - ALPHA) * accelAngleY;
}

void calibrateGyro2() {
  Serial.println("Calibrating second gyro... Keep sensor still");
  const int samples = 100;
  float sumX = 0, sumY = 0, sumZ = 0;

  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, t;
    mpu2.getEvent(&a, &g, &t);
    sumX += g.gyro.x;
    sumY += g.gyro.y;
    sumZ += g.gyro.z;
    delay(10);
  }

  gyroBiasX2 = sumX / samples;
  gyroBiasY2 = sumY / samples;
  gyroBiasZ2 = sumZ / samples;

  Serial.println("Second calibration complete.");
}

void calibrateGyro() {
  Serial.println("Calibrating gyro... Keep sensor still");
  const int samples = 100;
  float sumX = 0, sumY = 0, sumZ = 0;

  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    sumX += g.gyro.x;
    sumY += g.gyro.y;
    sumZ += g.gyro.z;
    delay(10);
  }

  gyroBiasX = sumX / samples;
  gyroBiasY = sumY / samples;
  gyroBiasZ = sumZ / samples;

  Serial.println("Calibration complete.");
}

void initializeDRV2605() {
  Wire.beginTransmission(DRV);
  Wire.write(ModeReg);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(DRV);
  Wire.write(0x1D);
  Wire.write(0xA8);
  Wire.endTransmission();

  Wire.beginTransmission(DRV);
  Wire.write(0x03);
  Wire.write(0x02);
  Wire.endTransmission();

  Wire.beginTransmission(DRV);
  Wire.write(0x17);
  Wire.write(0xff);
  Wire.endTransmission();

  Wire.beginTransmission(DRV);
  Wire.write(ModeReg);
  Wire.write(0x03);
  Wire.endTransmission();
  delay(100);
}

void pulse(double intensity, double milliseconds) {
  int minPWM = 140;
  int maxPWM = 255;
  int pwmVal = (intensity * (maxPWM - minPWM)) + minPWM;

  standbyOffB();
  PWM13 = pwmVal;
  usdelay(milliseconds);
  standbyOnB();
}

void standbyOnB() {
  Wire.beginTransmission(DRV);
  Wire.write(ModeReg);
  Wire.write(0x43);
  Wire.endTransmission();
}

void standbyOffB() {
  Wire.beginTransmission(DRV);
  Wire.write(ModeReg);
  Wire.write(0x03);
  Wire.endTransmission();
}

void usdelay(double time) {
  double us = time - ((int)time);
  for (int i = 0; i <= time; i++) {
    delay(1);
  }
  delayMicroseconds(us * 1000);
}

#define PWM12k 5
void pwm613configure() {
  TCCR4A = 0;
  TCCR4B = PWM12k;
  TCCR4C = 0;
  TCCR4D = 0;
  PLLFRQ = (PLLFRQ & 0xCF) | 0x30;
  OCR4C = 255;
  pwmSet13();
}
///
void pwmSet13() {
  OCR4A = 0;
  DDRC |= _BV(7);  // Pin C7
  TCCR4A = 0x82;
}
