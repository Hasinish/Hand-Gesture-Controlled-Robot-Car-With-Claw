//car

#include <SoftwareSerial.h>
#include <Servo.h>

// HC-05: TXD -> D2, RXD -> D3
SoftwareSerial BT(2, 3);

// L298N
int ENA = 5;
int ENB = 6;

int IN1 = 8;
int IN2 = 9;
int IN3 = 10;
int IN4 = 11;

// Servo
int servoPin = 7;
Servo gripper;

int openAngle = 70;
int closeAngle = 120;
int currentAngle = 90;
int servoStep = 8;

// LEDs
int redLED = A0;     // movement OFF
int greenLED = A1;   // movement ON
int yellowLED = A2;  // obstacle close only

// Ultrasonic sensor
int trigPin = 12;
int echoPin = 13;

// Yellow LED distance
int obstacleDistanceCM = 15;

bool movementMode = false;

// Movement
char command = 'S';
int speedValue = 0;

unsigned long lastCommandTime = 0;
int bluetoothTimeout = 500;

// Ultrasonic timing
unsigned long lastObstacleCheck = 0;
int obstacleCheckDelay = 100;

// Fast Bluetooth reader
char btBuffer[12];
byte btIndex = 0;

void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  gripper.attach(servoPin);
  currentAngle = constrain(currentAngle, min(openAngle, closeAngle), max(openAngle, closeAngle));
  gripper.write(currentAngle);

  stopMotors();
  setMovementLED(false);
  digitalWrite(yellowLED, LOW);

  Serial.println("CAR READY");
  Serial.println("Obstacle only controls yellow LED");
}

void loop() {
  readBluetoothFast();
  checkObstacleTimed();

  if (millis() - lastCommandTime > bluetoothTimeout) {
    command = 'S';
    speedValue = 0;
  }

  runMotors();
}

void readBluetoothFast() {
  while (BT.available()) {
    char c = BT.read();

    if (c == '\n') {
      btBuffer[btIndex] = '\0';
      processBluetoothMessage(btBuffer);
      btIndex = 0;
    }
    else if (c != '\r') {
      if (btIndex < sizeof(btBuffer) - 1) {
        btBuffer[btIndex] = c;
        btIndex++;
      }
      else {
        btIndex = 0;
      }
    }
  }
}

void processBluetoothMessage(char *message) {
  if (message[0] == '\0') {
    return;
  }

  char newCommand = message[0];
  int newValue = atoi(message + 1);

  if (
    newCommand == 'F' ||
    newCommand == 'B' ||
    newCommand == 'L' ||
    newCommand == 'R' ||
    newCommand == 'S'
  ) {
    if (!movementMode && newCommand != 'S') {
      return;
    }

    command = newCommand;
    speedValue = constrain(newValue, 0, 255);

    if (command == 'S') {
      speedValue = 0;
    }

    lastCommandTime = millis();
  }

  else if (newCommand == 'M') {
    if (newValue == 1) {
      movementMode = true;
      setMovementLED(true);
    }
    else {
      movementMode = false;
      command = 'S';
      speedValue = 0;
      stopMotors();
      setMovementLED(false);
    }
  }

  else if (newCommand == 'O') {
    moveClawToward(openAngle);
  }

  else if (newCommand == 'C') {
    moveClawToward(closeAngle);
  }
}

void runMotors() {
  if (!movementMode) {
    stopMotors();
    return;
  }

  if (command == 'F') {
    moveForward(speedValue);
  }
  else if (command == 'B') {
    moveBackward(speedValue);
  }
  else if (command == 'L') {
    rotateLeft(speedValue);
  }
  else if (command == 'R') {
    rotateRight(speedValue);
  }
  else {
    stopMotors();
  }
}

void checkObstacleTimed() {
  if (millis() - lastObstacleCheck >= obstacleCheckDelay) {
    lastObstacleCheck = millis();
    checkObstacle();
  }
}

void checkObstacle() {
  long distance = getDistanceCM();

  if (distance > 0 && distance <= obstacleDistanceCM) {
    digitalWrite(yellowLED, HIGH);
  }
  else {
    digitalWrite(yellowLED, LOW);
  }
}

long getDistanceCM() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 8000);

  if (duration == 0) {
    return 999;
  }

  long distance = duration * 0.034 / 2;
  return distance;
}

void moveClawToward(int targetAngle) {
  if (currentAngle < targetAngle) {
    currentAngle += servoStep;
    if (currentAngle > targetAngle) {
      currentAngle = targetAngle;
    }
  }
  else if (currentAngle > targetAngle) {
    currentAngle -= servoStep;
    if (currentAngle < targetAngle) {
      currentAngle = targetAngle;
    }
  }

  currentAngle = constrain(currentAngle, min(openAngle, closeAngle), max(openAngle, closeAngle));
  gripper.write(currentAngle);
}

void setMovementLED(bool isOn) {
  if (isOn) {
    digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH);
  }
  else {
    digitalWrite(redLED, HIGH);
    digitalWrite(greenLED, LOW);
  }
}

void moveForward(int spd) {
  analogWrite(ENA, spd);
  analogWrite(ENB, spd);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveBackward(int spd) {
  analogWrite(ENA, spd);
  analogWrite(ENB, spd);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void rotateLeft(int spd) {
  analogWrite(ENA, spd);
  analogWrite(ENB, spd);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void rotateRight(int spd) {
  analogWrite(ENA, spd);
  analogWrite(ENB, spd);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}