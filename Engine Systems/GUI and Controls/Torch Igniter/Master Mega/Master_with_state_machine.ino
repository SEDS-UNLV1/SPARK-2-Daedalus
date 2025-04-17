// Define the number of pressure transducers
#define NUM_SENSORS 5

// Array of analog pins connected to the transducers (A0 to A4)
const int sensorPins[NUM_SENSORS] = {A0, A1, A2, A3, A4};

// Array to store raw analog readings, voltages, and pressures
int rawValues[NUM_SENSORS];
float voltages[NUM_SENSORS];
float Pressures[NUM_SENSORS];

// Define SSR pins (using digital pins on Arduino Mega)
const int FUEL_LINE_PIN = 2;    // SSR1: Fuel Line a,b
const int FUEL_VENT_PIN = 3;    // SSR2: Fuel Vent c,d
const int OX_VENT_PIN = 7;      // SSR3: Oxygen Vent e,f
const int NITROGEN_TANK_PIN = 5;// SSR4: Nitrogen Tank g,h
const int NITROGEN_LINE_PIN = 6;// SSR5: Nitrogen Line i,j
const int OX_LINE_PIN = 4;      // SSR6: Oxygen Line k, l
const int OX_TANK_PIN = 8;      // SSR7: Oxygen Tank m,n 

// Pin to signal Uno for spark control
const int signalPin = 11; // Pin to send signal to Uno pin 9
int dwellTime = 3;        // Dwell time in ms (coil charges)
int offTime = 7;          // Off time in ms (minimum for spark reset)
int runDuration = 1000;   // Total run time in ms (default for standalone)

// Fixed spark timing for sequence
const int SEQUENCE_DWELL_TIME = 3;    // Fixed dwell time for sequence
const int SEQUENCE_OFF_TIME = 7;      // Fixed off time for sequence
const int SEQUENCE_RUN_DURATION = 1000;// Fixed run duration for sequence (500 ms)

// State machine states for sequence
enum State {
  IDLE,
  PREP,
  PRESSURIZED,
  TEST,
  TEST_DWELL,    // Sub-state for spark dwell
  TEST_OFF,      // Sub-state for spark off
  PURGE,
  PRESSURIZED_2
};

// State machine variables for sequence
State currentState = IDLE;
unsigned long stateStartTime = 0; // Time when the current state started
bool sequenceRunning = false;     // Flag to indicate if a sequence is active
unsigned long sparkStartTime = 0; // For tracking spark cycle in TEST state
int currentDwellTime = dwellTime; // Current dwell time (sequence or standalone)
int currentOffTime = offTime;     // Current off time (sequence or standalone)
int currentRunDuration = runDuration; // Current run duration (sequence or standalone)

// Variables for standalone spark cycle (non-blocking)
bool standaloneSparkRunning = false;
unsigned long standaloneSparkStartTime = 0;
unsigned long standaloneSparkStateTime = 0;
bool standaloneSparkOn = false;

// Non-blocking pressure reading timer
unsigned long lastPressureUpdate = 0;
const unsigned long PRESSURE_UPDATE_INTERVAL = 10; // Update pressure every 50 ms
unsigned long sequenceStartTime = 0; // To track when the sequence starts for timestamps

void setup() {
  // Initialize signal pin to Uno
  pinMode(signalPin, OUTPUT);
  digitalWrite(signalPin, LOW);

  // Initialize serial communication at 115200 baud rate
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect (Mega)
  }
  Serial.println("System Ready. Commands: 'a'-'p' for SSRs, 'startSeq' to start sequence, 'start dwell off duration' for spark only.");

  // Set SSR pins as outputs and turn off initially
  pinMode(FUEL_LINE_PIN, OUTPUT);
  pinMode(FUEL_VENT_PIN, OUTPUT);
  pinMode(OX_VENT_PIN, OUTPUT);
  pinMode(NITROGEN_TANK_PIN, OUTPUT);
  pinMode(NITROGEN_LINE_PIN, OUTPUT);
  pinMode(OX_LINE_PIN, OUTPUT);
  pinMode(OX_TANK_PIN, OUTPUT);
  digitalWrite(FUEL_LINE_PIN, LOW);
  digitalWrite(FUEL_VENT_PIN, LOW);
  digitalWrite(OX_VENT_PIN, LOW);
  digitalWrite(NITROGEN_TANK_PIN, LOW);
  digitalWrite(NITROGEN_LINE_PIN, LOW);
  digitalWrite(OX_LINE_PIN, LOW);
  digitalWrite(OX_TANK_PIN, LOW);
}

void loop() {
  // Handle state machine for sequence
  runStateMachine();

  // Handle standalone spark cycle
  runStandaloneSpark();

  // Check for serial input
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // Process SSR commands
    if (command == "a") { digitalWrite(FUEL_LINE_PIN, HIGH); }
    else if (command == "b") { digitalWrite(FUEL_LINE_PIN, LOW); }
    else if (command == "c") { digitalWrite(FUEL_VENT_PIN, HIGH); }
    else if (command == "d") { digitalWrite(FUEL_VENT_PIN, LOW); }
    else if (command == "e") { digitalWrite(OX_VENT_PIN, HIGH); }
    else if (command == "f") { digitalWrite(OX_VENT_PIN, LOW); }
    else if (command == "g") { digitalWrite(NITROGEN_TANK_PIN, HIGH); }
    else if (command == "h") { digitalWrite(NITROGEN_TANK_PIN, LOW); }
    else if (command == "i") { digitalWrite(NITROGEN_LINE_PIN, HIGH); }
    else if (command == "j") { digitalWrite(NITROGEN_LINE_PIN, LOW); }
    else if (command == "k") { digitalWrite(OX_LINE_PIN, HIGH); }
    else if (command == "l") { digitalWrite(OX_LINE_PIN, LOW); }
    else if (command == "m") { digitalWrite(OX_TANK_PIN, HIGH); }
    else if (command == "n") { digitalWrite(OX_TANK_PIN, LOW); }
    else if (command == "o") {
      digitalWrite(FUEL_LINE_PIN, HIGH);
      digitalWrite(FUEL_VENT_PIN, HIGH);
      digitalWrite(OX_VENT_PIN, HIGH);
      digitalWrite(NITROGEN_TANK_PIN, HIGH);
      digitalWrite(NITROGEN_LINE_PIN, HIGH);
      digitalWrite(OX_LINE_PIN, HIGH);
      digitalWrite(OX_TANK_PIN, HIGH);
    }
    else if (command == "p") {
      digitalWrite(FUEL_LINE_PIN, LOW);
      digitalWrite(FUEL_VENT_PIN, LOW);
      digitalWrite(OX_VENT_PIN, LOW);
      digitalWrite(NITROGEN_TANK_PIN, LOW);
      digitalWrite(NITROGEN_LINE_PIN, LOW);
      digitalWrite(OX_LINE_PIN, LOW);
      digitalWrite(OX_TANK_PIN, LOW);
    }
    // Start the sequence
    else if (command == "startSeq") {
      if (!sequenceRunning && !standaloneSparkRunning) {
        currentState = PREP;
        sequenceRunning = true;
        stateStartTime = millis();
        sequenceStartTime = millis(); // Record sequence start time for timestamps
        // Set fixed spark timing for sequence
        currentDwellTime = SEQUENCE_DWELL_TIME;
        currentOffTime = SEQUENCE_OFF_TIME;
        currentRunDuration = SEQUENCE_RUN_DURATION;
        Serial.println("PREP: Oxygen Vent and Fuel Vent ON");
      } else {
        Serial.println("Sequence or standalone spark already running.");
      }
    }
    // Process spark cycle command (standalone)
    else if (command.startsWith("start")) {
      int firstSpace = command.indexOf(' ');
      int secondSpace = command.indexOf(' ', firstSpace + 1);
      int thirdSpace = command.indexOf(' ', secondSpace + 1);
      
      if (thirdSpace == -1) {
        Serial.println("Error: Provide dwell, off, duration (e.g., 'start 5 5 5000').");
      } else {
        dwellTime = command.substring(firstSpace + 1, secondSpace).toInt();
        offTime = command.substring(secondSpace + 1, thirdSpace).toInt();
        runDuration = command.substring(thirdSpace + 1).toInt();
        
        if (dwellTime <= 0 || offTime <= 0 || runDuration <= 0) {
          Serial.println("Error: Times must be positive.");
        } else if (!sequenceRunning && !standaloneSparkRunning) {
          // Start a standalone spark cycle
          standaloneSparkRunning = true;
          standaloneSparkStartTime = millis();
          standaloneSparkStateTime = millis();
          standaloneSparkOn = true;
          digitalWrite(signalPin, HIGH); // Start dwell
          currentDwellTime = dwellTime;
          currentOffTime = offTime;
          currentRunDuration = runDuration;
          Serial.println("Starting standalone spark...");
        } else {
          Serial.println("Sequence or standalone spark already running.");
        }
      }
    }
    else {
      Serial.println("Invalid command.");
    }
  }

  // Read and process pressure sensor data non-blocking
  unsigned long currentTime = millis();
  if (currentTime - lastPressureUpdate >= PRESSURE_UPDATE_INTERVAL) {
    for (int i = 0; i < NUM_SENSORS; i++) {
      rawValues[i] = analogRead(sensorPins[i]);
      voltages[i] = (rawValues[i] / 1023.0) * 5.0;
      Pressures[i] = voltages[i] * 50.69 - 22.95;
    }
    
    // Always log pressure data
    Serial.print("PT,");
    if (sequenceRunning) {
      // Use timestamp (time since sequence started)
      unsigned long timeSinceSequenceStart = currentTime - sequenceStartTime;
      Serial.print(timeSinceSequenceStart);
    } else {
      // Use "-" when sequence is not running
      Serial.print("-");
    }
    Serial.print(",");
    for (int i = 0; i < NUM_SENSORS; i++) {
      Serial.print(Pressures[i], 2);
      if (i < NUM_SENSORS - 1) Serial.print(",");
    }
    Serial.println();

    lastPressureUpdate = currentTime;
  }
}

void runStateMachine() {
  if (!sequenceRunning) return; // Only run if sequence is active

  unsigned long currentTime = millis();
  
  switch (currentState) {
    case IDLE:
      // All signals off
      digitalWrite(FUEL_LINE_PIN, LOW);
      digitalWrite(FUEL_VENT_PIN, LOW);
      digitalWrite(OX_VENT_PIN, LOW);
      digitalWrite(NITROGEN_TANK_PIN, LOW);
      digitalWrite(NITROGEN_LINE_PIN, LOW);
      digitalWrite(OX_LINE_PIN, LOW);
      digitalWrite(OX_TANK_PIN, LOW);
      digitalWrite(signalPin, LOW);
      sequenceRunning = false;
      break;

    case PREP:
      // Oxygen Vent and Fuel Vent on
      digitalWrite(FUEL_VENT_PIN, HIGH);
      digitalWrite(OX_VENT_PIN, HIGH);
      if (currentTime - stateStartTime >= 1000) {
        currentState = PRESSURIZED;
        stateStartTime = currentTime;
        Serial.println("PRESSURIZED: Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent ON");
      }
      break;

    case PRESSURIZED:
      // Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent on
      digitalWrite(OX_TANK_PIN, HIGH);
      digitalWrite(NITROGEN_TANK_PIN, HIGH);
      digitalWrite(OX_VENT_PIN, HIGH);
      digitalWrite(FUEL_VENT_PIN, HIGH);
      if (currentTime - stateStartTime >= 3000) {
        currentState = TEST;
        stateStartTime = currentTime;
        sparkStartTime = currentTime;
        Serial.println("TEST: Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent, Oxygen Line, Fuel Line ON, Igniting...");
      }
      break;

    case TEST:
      // Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent, Oxygen Line, Fuel Line on
      digitalWrite(OX_TANK_PIN, HIGH);
      digitalWrite(NITROGEN_TANK_PIN, HIGH);
      digitalWrite(OX_VENT_PIN, HIGH);
      digitalWrite(FUEL_VENT_PIN, HIGH);
      digitalWrite(OX_LINE_PIN, HIGH);
      digitalWrite(FUEL_LINE_PIN, HIGH);
      currentState = TEST_DWELL;
      stateStartTime = currentTime;
      digitalWrite(signalPin, HIGH); // Start dwell
      break;

    case TEST_DWELL:
      if (currentTime - stateStartTime >= currentDwellTime) {
        digitalWrite(signalPin, LOW); // End dwell, start off
        currentState = TEST_OFF;
        stateStartTime = currentTime;
      }
      break;

    case TEST_OFF:
      if (currentTime - stateStartTime >= currentOffTime) {
        // Check if spark cycle duration is complete
        if (currentTime - sparkStartTime >= currentRunDuration) {
          digitalWrite(signalPin, LOW); // Ensure spark is off
          currentState = PURGE;
          stateStartTime = currentTime;
          Serial.println("PURGE: Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent, Nitrogen Line ON");
        } else {
          // Continue sparking
          currentState = TEST_DWELL;
          stateStartTime = currentTime;
          digitalWrite(signalPin, HIGH); // Start next dwell
        }
      }
      break;

    case PURGE:
      // Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent, Nitrogen Line on
      digitalWrite(OX_TANK_PIN, HIGH);
      digitalWrite(NITROGEN_TANK_PIN, HIGH);
      digitalWrite(OX_VENT_PIN, HIGH);
      digitalWrite(FUEL_VENT_PIN, HIGH);
      digitalWrite(NITROGEN_LINE_PIN, HIGH);
      digitalWrite(OX_LINE_PIN, LOW);
      digitalWrite(FUEL_LINE_PIN, LOW);
      if (currentTime - stateStartTime >= 15000) {
        currentState = PRESSURIZED_2;
        stateStartTime = currentTime;
        Serial.println("PRESSURIZED_2: Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent ON");
      }
      break;

    case PRESSURIZED_2:
      // Oxygen Tank, Nitrogen Tank, Oxygen Vent, Fuel Vent on
      digitalWrite(OX_TANK_PIN, HIGH);
      digitalWrite(NITROGEN_TANK_PIN, HIGH);
      digitalWrite(OX_VENT_PIN, HIGH);
      digitalWrite(FUEL_VENT_PIN, HIGH);
      digitalWrite(NITROGEN_LINE_PIN, LOW);
      if (currentTime - stateStartTime >= 1000) {
        currentState = IDLE;
        stateStartTime = currentTime;
        Serial.println("Sequence ended");
      }
      break;
  }
}

void runStandaloneSpark() {
  if (!standaloneSparkRunning) return; // Only run if standalone spark is active

  unsigned long currentTime = millis();

  // Check if the total spark duration is complete
  if (currentTime - standaloneSparkStartTime >= currentRunDuration) {
    digitalWrite(signalPin, LOW); // Ensure spark is off
    standaloneSparkRunning = false;
    return;
  }

  // Toggle between dwell (on) and off states
  if (standaloneSparkOn) {
    if (currentTime - standaloneSparkStateTime >= currentDwellTime) {
      digitalWrite(signalPin, LOW); // End dwell, start off
      standaloneSparkOn = false;
      standaloneSparkStateTime = currentTime;
    }
  } else {
    if (currentTime - standaloneSparkStateTime >= currentOffTime) {
      digitalWrite(signalPin, HIGH); // Start next dwell
      standaloneSparkOn = true;
      standaloneSparkStateTime = currentTime;
    }
  }
}
