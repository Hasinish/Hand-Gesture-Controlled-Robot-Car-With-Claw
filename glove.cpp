//glove

#include <Wire.h>
#include <SoftwareSerial.h>

// HC-05: TXD -> D2, RXD -> D3
SoftwareSerial BT(2, 3);

const int MPU = 0x68;

int16_t AcX, AcY, AcZ;
float pitch, roll;

// Buttons
int movementButton = 4; // D4 = movement ON/OFF
int openButton = 5;     // D5 = hold open claw
int closeButton = 6;    // D6 = hold close claw

bool movementMode = false;

// D4 debounce
bool lastMovementButtonState = HIGH;
unsigned long lastMovementDebounceTime = 0;
int debounceDelay = 250;

// Claw hold speed
unsigned long lastOpenSendTime = 0;
unsigned long lastCloseSendTime = 0;
int clawSendDelay = 30;

// Gesture speed
int minAngle = 30;
int maxAngle = 70;
int minSpeed = 30;
int maxSpeed = 255;

unsigned long lastMoveSendTime = 0;
int moveSendDelay = 35;

char lastCommand = 'X';
int lastValue = -1;

void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(movementButton, INPUT_PULLUP);
  pinMode(openButton, INPUT_PULLUP);
  pinMode(closeButton, INPUT_PULLUP);

  Wire.begin();
  Wire.setWireTimeout(3000, true);

  Serial.println("GLOVE READY");
  Serial.println("D4 movement | D5 open | D6 close");

  wakeMPU();

  sendCommand('M', 0);
  sendCommand('S', 0);
}

void loop() {
  checkMovementButton();
  checkClawHoldButtons();

  if (!movementMode) {
    if (millis() - lastMoveSendTime >= 100) {
      sendCommand('S', 0);
      lastMoveSendTime = millis();
    }
    return;
  }

  if (millis() - lastMoveSendTime < moveSendDelay) {
    return;
  }
  lastMoveSendTime = millis();

  bool mpuOK = readMPU6050();

  if (!mpuOK) {
    sendCommand('S', 0);
    wakeMPU();
    return;
  }

  pitch = atan2(AcY, AcZ) * 180 / PI;
  roll  = atan2(AcX, AcZ) * 180 / PI;

  char command = 'S';
  int speedValue = 0;

  float absPitch = abs(pitch);
  float absRoll = abs(roll);

  if (absPitch <= minAngle && absRoll <= minAngle) {
    command = 'S';
    speedValue = 0;
  }
  else {
    if (absPitch >= absRoll) {
      if (pitch > minAngle) {
        command = 'F';
        speedValue = calculateSpeed(absPitch);
      }
      else if (pitch < -minAngle) {
        command = 'B';
        speedValue = calculateSpeed(absPitch);
      }
    }
    else {
      if (roll > minAngle) {
        command = 'R';
        speedValue = calculateSpeed(absRoll);
      }
      else if (roll < -minAngle) {
        command = 'L';
        speedValue = calculateSpeed(absRoll);
      }
    }
  }

  sendCommand(command, speedValue);
}

void checkMovementButton() {
  bool buttonState = digitalRead(movementButton);

  if (buttonState == LOW && lastMovementButtonState == HIGH) {
    if (millis() - lastMovementDebounceTime > debounceDelay) {
      movementMode = !movementMode;

      if (movementMode) {
        Serial.println("Movement ON");
        sendCommand('M', 1);
      }
      else {
        Serial.println("Movement OFF");
        sendCommand('M', 0);
        sendCommand('S', 0);
      }

      lastMovementDebounceTime = millis();
    }
  }

  lastMovementButtonState = buttonState;
}

void checkClawHoldButtons() {
  bool openPressed = digitalRead(openButton) == LOW;
  bool closePressed = digitalRead(closeButton) == LOW;

  if (openPressed && !closePressed) {
    if (millis() - lastOpenSendTime >= clawSendDelay) {
      sendCommand('O', 0);
      lastOpenSendTime = millis();
    }
  }

  if (closePressed && !openPressed) {
    if (millis() - lastCloseSendTime >= clawSendDelay) {
      sendCommand('C', 0);
      lastCloseSendTime = millis();
    }
  }
}

int calculateSpeed(float angleValue) {
  angleValue = abs(angleValue);

  if (angleValue <= minAngle) {
    return 0;
  }

  if (angleValue >= maxAngle) {
    return maxSpeed;
  }

  float speedFloat =
    minSpeed +
    ((angleValue - minAngle) * (maxSpeed - minSpeed)) /
    (maxAngle - minAngle);

  return constrain((int)speedFloat, 0, 255);
}

void sendCommand(char command, int value) {
  BT.print(command);
  BT.print(value);
  BT.print('\n');

  if (command != lastCommand || value != lastValue) {
    Serial.print("Sent: ");
    Serial.print(command);
    Serial.println(value);

    lastCommand = command;
    lastValue = value;
  }
}

void wakeMPU() {
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);

  byte error = Wire.endTransmission(true);

  if (error == 0) {
    Serial.println("MPU OK");
  }
  else {
    Serial.print("MPU error: ");
    Serial.println(error);
  }
}

bool readMPU6050() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);

  byte error = Wire.endTransmission(false);

  if (error != 0) {
    return false;
  }

  int bytesReceived = Wire.requestFrom(MPU, 6, true);

  if (bytesReceived < 6) {
    return false;
  }

  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();

  return true;
}