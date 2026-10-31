// Obstacle avoiding car - fresh start
// ESP32 + L298N + HC-SR04 on MG90S servo + 2x IR side sensors
// Library needed: ESP32Servo (Arduino IDE > Library Manager)
//
// IMPORTANT: switch on with NOTHING close to the car (about 30 cm clear).
// At start-up the car learns what "clear" looks like for each IR sensor,
// so it works whether your IR modules output LOW or HIGH on detection.

#include <ESP32Servo.h>

// ---------- Pins ----------
const int IN1 = 26;          // left motor
const int IN2 = 27;
const int IN3 = 14;          // right motor
const int IN4 = 32;

const int TRIG_PIN  = 4;     // ultrasonic
const int ECHO_PIN  = 19;    // through the 1k/2k divider
const int SERVO_PIN = 13;

const int IR_LEFT_PIN  = 25;
const int IR_RIGHT_PIN = 33;

// ---------- Settings ----------
const int STOP_DISTANCE_CM = 25;
const int SERVO_CENTER = 90;
const int SERVO_LEFT   = 160;    // swap these two if left/right is mirrored
const int SERVO_RIGHT  = 20;

const int BACK_TIME_MS    = 350;
const int TURN_TIME_MS    = 650;   // front obstacle turn (~90 degrees)
const int IR_BACK_MS      = 200;
const int IR_TURN_MS      = 450;   // small turn away from a side obstacle

Servo scanner;
int irLeftClear  = HIGH;     // learned at start-up
int irRightClear = HIGH;

// ---------- Motors ----------
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

// If the car turns the wrong way, swap the contents of these two
void turnLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}

// ---------- IR ----------
int learnClearLevel(int pin) {
  int highCount = 0;
  for (int i = 0; i < 30; i++) {
    if (digitalRead(pin) == HIGH) highCount++;
    delay(20);
  }
  return (highCount > 15) ? HIGH : LOW;
}

// Obstacle = pin differs from its learned "clear" level (two reads in a row)
bool irSeen(int pin, int clearLevel) {
  if (digitalRead(pin) == clearLevel) return false;
  delayMicroseconds(500);
  return digitalRead(pin) != clearLevel;
}

// ---------- Ultrasonic ----------
long pingOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 400;            // no echo = nothing close
  return duration * 0.0343 / 2;
}

// Median of 3 readings (ignores a single bad one)
long readDistance() {
  long a = pingOnce(); delay(10);
  long b = pingOnce(); delay(10);
  long c = pingOnce();
  if ((a >= b && a <= c) || (a <= b && a >= c)) return a;
  if ((b >= a && b <= c) || (b <= a && b >= c)) return b;
  return c;
}

long lookAt(int angle) {
  scanner.write(angle);
  delay(450);
  return readDistance();
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);
  stopMotors();

  scanner.setPeriodHertz(50);
  scanner.attach(SERVO_PIN, 500, 2400);
  scanner.write(SERVO_CENTER);

  delay(1500);
  irLeftClear  = learnClearLevel(IR_LEFT_PIN);
  irRightClear = learnClearLevel(IR_RIGHT_PIN);

  Serial.print("IR clear level  left: ");  Serial.print(irLeftClear);
  Serial.print("  right: ");               Serial.println(irRightClear);
  Serial.println("Starting in 2 seconds...");
  delay(2000);
}

// ---------- Main loop ----------
void loop() {
  bool irL = irSeen(IR_LEFT_PIN,  irLeftClear);
  bool irR = irSeen(IR_RIGHT_PIN, irRightClear);

  // 1) Side sensors: back up a little and turn away from the obstacle
  if (irL || irR) {
    Serial.print("IR  left: "); Serial.print(irL);
    Serial.print("  right: ");  Serial.println(irR);

    stopMotors();
    backward();
    delay(IR_BACK_MS);

    if (irL && !irR) turnRight();   // obstacle on the left
    else             turnLeft();    // obstacle on the right, or both
    delay(IR_TURN_MS);

    stopMotors();
    delay(100);
    return;
  }

  // 2) Front: ultrasonic
  long distance = readDistance();
  Serial.print("Front: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > STOP_DISTANCE_CM) {
    forward();
  } else {
    stopMotors();
    delay(150);

    backward();
    delay(BACK_TIME_MS);
    stopMotors();

    long leftDist  = lookAt(SERVO_LEFT);
    long rightDist = lookAt(SERVO_RIGHT);
    scanner.write(SERVO_CENTER);

    Serial.print("Left: ");   Serial.print(leftDist);
    Serial.print("  Right: "); Serial.println(rightDist);

    if (leftDist >= rightDist) turnLeft();
    else                       turnRight();
    delay(TURN_TIME_MS);

    stopMotors();
    delay(150);
  }

  delay(30);
}
