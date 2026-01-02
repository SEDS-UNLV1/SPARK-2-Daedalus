#include <avr/wdt.h>

// Define the number of pressure transducers
#define NUM_SENSORS 6

// Define Thermocouple pin
#define TC_PIN A5

// Set to ADC pin used
#define AREF 5 // 3.3 or 5 V
#define ADC_RESOLUTION 10 // set to ADC bit resolution

// Array of analog pins connected to the transducers (A0 to A5)
const int sensorPins[NUM_SENSORS] = {A0, A1, A2, A3, A4, A5};

// Array to store raw analog readings, voltages, and pressures
int rawValues[NUM_SENSORS];
float voltages[NUM_SENSORS];
float Pressures[NUM_SENSORS];
float temperature;
float temp_conversion;

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

// ====== FUNCTION: Force everything into safe state ======
void setAllSafe() {
  digitalWrite(FUEL_LINE_PIN, LOW);
  digitalWrite(FUEL_VENT_PIN, LOW);
  digitalWrite(OX_VENT_PIN, LOW);
  digitalWrite(NITROGEN_TANK_PIN, LOW);
  digitalWrite(NITROGEN_LINE_PIN, LOW);
  digitalWrite(OX_LINE_PIN, LOW);
  digitalWrite(OX_TANK_PIN, LOW);
  digitalWrite(signalPin, LOW);  //spark off
}

// State machine states for sequence
enum State {
  IDLE,
  PREP,
  PRESSURIZED,
  TEST,
  SPARK_TEST_DWELL,    // Sub-state for spark dwell
  SPARK_TEST_OFF,      // Sub-state for spark off
  PURGE,
  PRESSURIZED_2
};

enum TransitionTypes {
  PREP_TO_PRESSURIZED,
  PRESSURIZED_TO_TEST,
  TEST_TO_PURGE,
  PURGE_TO_PRESSURIZED_2,
  PRESSURIZED_2_TO_IDLE,
  NUMBER_OF_TRANSITIONS
};
// Timers for sequence transitions
unsigned long sequenceTransitionTimes[NUMBER_OF_TRANSITIONS] = {
  1000, // PREP to PRESSURIZED
  3000, // PRESSURIZED to TEST
  15000, // TEST to PURGE
  1000, // PURGE to PRESSURIZED_2
  1000, // PRESSURIZED_2 to IDLE
};

// State machine variables for sequence
State currentState = IDLE;
unsigned long stateStartTime = 0; // Time when the current state started
bool sequenceRunning = false;     // Flag to indicate if a sequence is active
unsigned long sparkStartTime = 0; // For tracking spark cycle in TEST state
int sparkDwellTime = dwellTime; // Current dwell time (sequence or standalone)
int sparkOffTime = offTime;     // Current off time (sequence or standalone)
int currentRunDuration = runDuration; // Current run duration (sequence or standalone)

// Variables for standalone spark cycle (non-blocking)
bool standaloneSparkRunning = false;
unsigned long standaloneSparkStartTime = 0;
unsigned long standaloneSparkStateTime = 0;
bool standaloneSparkOn = false;

// Non-blocking pressure reading timer
unsigned long lastPressureUpdate = 0;
const unsigned long PRESSURE_UPDATE_INTERVAL = 10; // Update pressure every 10 ms
unsigned long sequenceStartTime = 0; // To track when the sequence starts for timestamps

void setup() {

  // Set SSR pins as outputs and turn off initially
  pinMode(FUEL_LINE_PIN, OUTPUT);
  pinMode(FUEL_VENT_PIN, OUTPUT);
  pinMode(OX_VENT_PIN, OUTPUT);
  pinMode(NITROGEN_TANK_PIN, OUTPUT);
  pinMode(NITROGEN_LINE_PIN, OUTPUT);
  pinMode(OX_LINE_PIN, OUTPUT);
  pinMode(OX_TANK_PIN, OUTPUT);
  pinMode(signalPin, OUTPUT);   // Initialize signal pin to Uno

  // Make sure valves/spark are off on startup
  setAllSafe();

  // Initialize serial communication at 115200 baud rate
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect (Mega)
  }
  Serial.println("System Ready.");
  Serial.println("Commands: 'a'-'p' for SSRs, 'startSeq' to start sequence, 'start dwell off duration' for spark only.");

  // Enable watchdog with 2s timeout
  wdt_enable(WDTO_2S);
}

void loop() {

  // Reset watchdog every pass to prevent reset
  wdt_reset();

  // Handle state machine for sequence
  runStateMachine();

  // Handle standalone spark cycle
  runStandaloneSpark();

  // Handle serial commands
  handleSerialCommands();

  // Measure Pressure
  updateMeasurments();
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
      if (currentTime - stateStartTime >= sparkDwellTime) {
        digitalWrite(signalPin, LOW); // End dwell, start off
        currentState = TEST_OFF;
        stateStartTime = currentTime;
      }
      break;

    case TEST_OFF:
      if (currentTime - stateStartTime >= sparkOffTime) {
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
    if (currentTime - standaloneSparkStateTime >= sparkDwellTime) {
      digitalWrite(signalPin, LOW); // End dwell, start off
      standaloneSparkOn = false;
      standaloneSparkStateTime = currentTime;
    }
  } else {
    if (currentTime - standaloneSparkStateTime >= sparkOffTime) {
      digitalWrite(signalPin, HIGH); // Start next dwell
      standaloneSparkOn = true;
      standaloneSparkStateTime = currentTime;
    }
  }
}

void handleSerialCommands() {
  bool isSequence = false;
  // Check for serial input
    if (Serial.available() > 0) {
      String command = Serial.readStringUntil('\n');
      command.trim();

      if(command.length() == 1){
      // Process SSR commands
        switch(command){
          case 'a': digitalWrite(FUEL_LINE_PIN, HIGH); break;
          case 'b': digitalWrite(FUEL_LINE_PIN, LOW); break;
          case 'c': digitalWrite(FUEL_VENT_PIN, HIGH); break;
          case 'd': digitalWrite(FUEL_VENT_PIN, LOW); break;
          case 'e': digitalWrite(OX_VENT_PIN, HIGH); break;
          case 'f': digitalWrite(OX_VENT_PIN, LOW); break;
          case 'g': digitalWrite(NITROGEN_TANK_PIN, HIGH); break;
          case 'h': digitalWrite(NITROGEN_TANK_PIN, LOW); break;
          case 'i': digitalWrite(NITROGEN_LINE_PIN, HIGH); break;
          case 'j': digitalWrite(NITROGEN_LINE_PIN, LOW); break;
          case 'k': digitalWrite(OX_LINE_PIN, HIGH); break;
          case 'l': digitalWrite(OX_LINE_PIN, LOW); break;
          case 'm': digitalWrite(OX_TANK_PIN, HIGH); break;
          case 'n': digitalWrite(OX_TANK_PIN, LOW); break;
          case 'o':
            digitalWrite(FUEL_LINE_PIN, HIGH);
            digitalWrite(FUEL_VENT_PIN, HIGH);
            digitalWrite(OX_VENT_PIN, HIGH);
            digitalWrite(NITROGEN_TANK_PIN, HIGH);
            digitalWrite(NITROGEN_LINE_PIN, HIGH);
            digitalWrite(OX_LINE_PIN, HIGH);
            digitalWrite(OX_TANK_PIN, HIGH);
            break;
          case 'p':
            digitalWrite(FUEL_LINE_PIN, LOW);
            digitalWrite(FUEL_VENT_PIN, LOW);
            digitalWrite(OX_VENT_PIN, LOW);
            digitalWrite(NITROGEN_TANK_PIN, LOW);
            digitalWrite(NITROGEN_LINE_PIN, LOW);
            digitalWrite(OX_LINE_PIN, LOW);
            digitalWrite(OX_TANK_PIN, LOW);
            break;
          default:
            Serial.println("Invalid one letter SSR command.");
            break;
        }
      }
      else if(command.length() > 1){
        // Is a command for state transition time parameters
        isSequence = parseArgs(command);
      }
      if (isSequence){
        if (!sequenceRunning && !standaloneSparkRunning){
          currentState = PREP;
          sequenceRunning = true;
          stateStartTime = millis();
          sequenceStartTime = millis(); // Record sequence start time for timestamps
          // Set fixed spark timing for sequence
          sparkDwellTime = SEQUENCE_DWELL_TIME;
          sparkOffTime = SEQUENCE_OFF_TIME;
          currentRunDuration = SEQUENCE_RUN_DURATION;
          Serial.println("PREP: Oxygen Vent and Fuel Vent ON");
        } else {
          Serial.println("Sequence or standalone spark already running.");
        }
      }
      else {
        if (!sequenceRunning && !standaloneSparkRunning){
          // Start a standalone spark cycle
          standaloneSparkRunning = true;
          standaloneSparkStartTime = millis();
          standaloneSparkStateTime = millis();
          standaloneSparkOn = true;
          digitalWrite(signalPin, HIGH); // Start dwell
          sparkDwellTime = dwellTime;
          sparkOffTime = offTime;
          currentRunDuration = runDuration;
          Serial.println("Starting standalone spark...");
        } else {
          Serial.println("Sequence or standalone spark already running.");
        }
      }
    }
}

bool parseArgs(String command){
  command.trim();     // Removing leading and trailing whitespaces
  command.toLowerCase();

  char* argv[9];
  int argc = 0;

  // Using strtok_r to tokenize command into char* using whitespace as a delimitter
  char* save;
  char* tok = strtok_r(command, " ", &save);
  while(tok && (int)(sizeof(argv)/sizeof(argv[0]))){
    argv[argc++] = tok;
    tok = strtok_r(nullptr, " ", &save);
  }

  // Check command mode, sequencing or spark standalone test
  bool isSequence = false;
  // Sequencing mode
  if(strcmp(argv[0], "startseq") != 0){
    Serial.println("Sequencing mode");
    isSequence = true;
  }
  else if(strcmp(argv[0], "standalone") != 0){
    Serial.println("Standalone mode");
    isSequence = false;
    standaloneSparkRunning = true;
  }
  else {
    // Error, wrong input command
    Serial.println("Error: Invalid command.");
  }

  // parse tokenized command
  for (int i = 0; i < argc; i++){
    TransitionTypes transitionType = getTransitionType(argv[i]);

    bool isStandaloneType = transitionType & TEST_TO_TEST_DWELL || transitionType & TEST_DWELL_TO_TEST_OFF || transitionType & TEST_OFF_TO_PURGE;
    // Sequencing mode
    if(isSequence){
      if(transitionType != NUMBER_OF_TRANSITIONS){
        if(i+1 < argc){
          sequenceTransitionTimes[transitionType] = atoi(argv[i+1]);
        }
        else {
          Serial.println("Error: No " + String(transitionType) + " time provided. Using default of " + sequenceTransitionTimes[transitionType]);
        }
      }
      else {
        Serial.println("Error: Invalid transition type.");
      }
    }
    // Standalone mode
    else {
      if(strcmp(argv[i], "--sparkdwelltime") == 0) {
        if(i+1 < argc) sparkDwellTime = atoi(argv[i+1]);
        else {
          Serial.println("Error: No dwell time provided. Using default of " + String(sparkDwellTime));
        }
      }
      if(strcmp(argv[i], "--sparkdwelltime") == 0) {
        if(i+1 < argc) sparkDwellTime = atoi(argv[i+1]);
        else {
          Serial.println("Error: No dwell time provided. Using default of " + String(sparkDwellTime));
        }
      }
    }
  }
}

//  StartSeq --prepToPressurized 3000 --pressurizedToTest 15000 --testToPurge 1000 --purgeToPressurized2 1000 --pressurized2ToIdle 1000
TransitionTypes getTransitionType(const char* flagName){
  if(strcmp(flagName, "--preptopressurized") == 0) return PREP_TO_PRESSURIZED;
  else if(strcmp(flagName, "--pressurizedtotest") == 0) return PRESSURIZED_TO_TEST;
  else if(strcmp(flagName, "--testtopurge") == 0) return TEST_TO_PURGE;
  else if(strcmp(flagName, "--purgetopressurized2") == 0) return PURGE_TO_PRESSURIZED_2;
  else if(strcmp(flagName, "--pressurized2toidle") == 0) return PRESSURIZED_2_TO_IDLE;
  else return NUMBER_OF_TRANSITIONS;
}

void updateMeasurments() {
  // Read and process sensor data non-blocking
  unsigned long currentTime = millis();
  if (currentTime - lastPressureUpdate >= PRESSURE_UPDATE_INTERVAL) {
    for (int i = 0; i < NUM_SENSORS - 1; i++) {
      rawValues[i] = analogRead(sensorPins[i]);
      voltages[i] = (rawValues[i] / 1023.0) * 5.0;
      Pressures[i] = voltages[i] * 50.69 - 22.95;
    }
    
 // For the thermocouple
    rawValues[5] = analogRead(sensorPins[5]);
    voltages[5] = rawValues[5] * (AREF / (pow(2, ADC_RESOLUTION)-1));
    temperature = (voltages[5] - 1.25) / 0.005;
    temp_conversion = (temperature * 9 / 5) + 32;

    // Always log pressure data
    if (sequenceRunning) {
      // Use timestamp (time since sequence started)
      unsigned long timeSinceSequenceStart = currentTime - sequenceStartTime;
      Serial.print(timeSinceSequenceStart);
    } else {
      // Use "-" when sequence is not running
      Serial.print("-");
    }
    Serial.print(",");
    for (int i = 0; i < NUM_SENSORS -1; i++) {
      Serial.print(Pressures[i], 2);
      if (i < NUM_SENSORS - 1) Serial.print(","); 
    }
    Serial.println(temp_conversion); // temp in Fahrenheit
    lastPressureUpdate = currentTime;
  }
}
