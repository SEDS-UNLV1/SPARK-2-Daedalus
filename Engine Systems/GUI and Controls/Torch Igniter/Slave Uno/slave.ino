// Pins for signal input and IGBT driver output
const int inputPin = 9;  // Pin to receive signal from Mega pin 9
const int driverPin = 3; // Pin to drive UCC27517DBVR IN+ (changed to avoid conflict)

void setup() {
  // Initialize input pin to read signal from Mega
  pinMode(inputPin, INPUT);

  // Initialize output pin to IGBT driver
  pinMode(driverPin, OUTPUT);
  digitalWrite(driverPin, LOW); // Start with IGBT off
}

void loop() {
  // Read the signal from Mega and mirror it to the IGBT driver
  int signalState = digitalRead(inputPin);
  digitalWrite(driverPin, signalState);
}
