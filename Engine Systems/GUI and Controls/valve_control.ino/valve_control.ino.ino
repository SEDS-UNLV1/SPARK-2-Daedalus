const int valvePins[5] = {2,3}; //mock values

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 5; i++) {
    pinMode(valvePins[i], OUTPUT);
    digitalWrite(valvePins[i], LOW);
  }
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();  

    if (command.startsWith("VALVE")) {
      int valveIndex = command.substring(5, 6).toInt() - 1;  //e.g., VALVE1 → index 0
      String state = command.substring(7); 

      if (valveIndex >= 0 && valveIndex < 5) {
        if (state == "HI") {
          digitalWrite(valvePins[valveIndex], HIGH);
          Serial.print("VALVE");
          Serial.print(valveIndex + 1);
          Serial.println(":OPENED");
        } else if (state == "LO") {
          digitalWrite(valvePins[valveIndex], LOW);
          Serial.print("VALVE");
          Serial.print(valveIndex + 1);
          Serial.println(":CLOSED");
        }
      }
    }
  }
}
