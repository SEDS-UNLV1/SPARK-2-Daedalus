// Combined Stage 1: Startup and Stage 2: Pre-Hotfire
// Using Potentiometers for Hand Ball Valves and LEDs + Relays for Solenoids

// STAGE 1 PINS - Hand Ball Valve Potentiometers (Manual Control)
#define HBV1F_POT A0  // Hand Ball Valve 1 - Fill (Potentiometer)
#define HBV1P_POT A1  // Hand Ball Valve 1 - Pressurize (Potentiometer)
#define HBV1O_POT A2  // Hand Ball Valve 1 - Output (Potentiometer)

// STAGE 1 - Solenoid with LED + Relay
#define SOLENOID_RELAY_PIN 5     // NOS1-P Relay control
#define SOLENOID_LED_PIN 6       // LED indicator for solenoid state

// Pressure sensors for Stage 1
#define PRESSURE_SENSOR_1 A3
#define PRESSURE_SENSOR_2 A4

// STAGE 2 PINS - Solenoid Valves with LEDs + Relays
// KEROSENE SYSTEM
#define NOS2F_RELAY_PIN 7       // NOS2-F Relay
#define NOS2F_LED_PIN 8         // NOS2-F LED indicator
#define NCS1F_RELAY_PIN 9       // NCS1-F Relay 
#define NCS1F_LED_PIN 10        // NCS1-F LED indicator
#define NOS1F_RELAY_PIN 11      // NOS1-F Relay
#define NOS1F_LED_PIN 12        // NOS1-F LED indicator
#define NOS2F_FLOW_RELAY_PIN 13 // NOS2-F Flow Relay
#define NOS2F_FLOW_LED_PIN 2    // NOS2-F Flow LED indicator
#define NCS2F_RELAY_PIN 14      // NCS2-F Relay
#define NCS2F_LED_PIN 15        // NCS2-F LED indicator
#define NCS3F_RELAY_PIN 16      // NCS3-F Relay
#define NCS3F_LED_PIN 17        // NCS3-F LED indicator

// GOx SYSTEM  
#define NCS1O_RELAY_PIN 18      // NCS1-O Relay
#define NCS1O_LED_PIN 19        // NCS1-O LED indicator
#define NCS2O_RELAY_PIN 20      // NCS2-O Relay
#define NCS2O_LED_PIN 21        // NCS2-O LED indicator
#define NCS3O_RELAY_PIN 22      // NCS3-O Relay
#define NCS3O_LED_PIN 23        // NCS3-O LED indicator

// PRESSURE TRANSDUCERS for Stage 2
#define KEROSENE_PRESSURE_SENSOR A5
#define GOX_PRESSURE_SENSORS A6
#define FUEL_PUMP_PRESSURE_SENSOR A7

// STATUS LEDs for system stages
#define LED_KEROSENE_SYSTEM 24
#define LED_GOX_SYSTEM 25
#define LED_STAGE2_COMPLETE 26

// Hand Ball Valve position thresholds (0-1023 from potentiometer)
#define VALVE_CLOSED_THRESHOLD 200    // Below this = closed
#define VALVE_OPEN_THRESHOLD 800      // Above this = open

// Constants
const float PRESSURE_THRESHOLD = 450.0;
const float KEROSENE_VENT_PRESSURE_THRESHOLD = 100.0;  
const float KEROSENE_PRIME_PRESSURE_THRESHOLD = 300.0; 
const float VOLTAGE_REF = 5.0;
const int ADC_MAX = 1023;
const float PSI_PER_VOLT = 10.0;

// State tracking variables
bool hbv1f_open = false;
bool hbv1p_open = false;
bool hbv1o_open = false;
bool stage1_complete = false;
bool stage2_complete = false;
float pressure;

void setup() {
  // STAGE 1 SETUP - Potentiometer inputs (no pinMode needed for analog inputs)
  // Solenoid relay and LED setup
  pinMode(SOLENOID_RELAY_PIN, OUTPUT);
  pinMode(SOLENOID_LED_PIN, OUTPUT);
  digitalWrite(SOLENOID_RELAY_PIN, LOW);
  digitalWrite(SOLENOID_LED_PIN, LOW);

  // STAGE 2 SETUP - Initialize kerosene system relays and LEDs
  pinMode(NOS2F_RELAY_PIN, OUTPUT);
  pinMode(NOS2F_LED_PIN, OUTPUT);
  pinMode(NCS1F_RELAY_PIN, OUTPUT);
  pinMode(NCS1F_LED_PIN, OUTPUT);
  pinMode(NOS1F_RELAY_PIN, OUTPUT);
  pinMode(NOS1F_LED_PIN, OUTPUT);
  pinMode(NOS2F_FLOW_RELAY_PIN, OUTPUT);
  pinMode(NOS2F_FLOW_LED_PIN, OUTPUT);
  pinMode(NCS2F_RELAY_PIN, OUTPUT);
  pinMode(NCS2F_LED_PIN, OUTPUT);
  pinMode(NCS3F_RELAY_PIN, OUTPUT);
  pinMode(NCS3F_LED_PIN, OUTPUT);
  
  // Initialize GOx system relays and LEDs
  pinMode(NCS1O_RELAY_PIN, OUTPUT);
  pinMode(NCS1O_LED_PIN, OUTPUT);
  pinMode(NCS2O_RELAY_PIN, OUTPUT);
  pinMode(NCS2O_LED_PIN, OUTPUT);
  pinMode(NCS3O_RELAY_PIN, OUTPUT);
  pinMode(NCS3O_LED_PIN, OUTPUT);

  // Initialize all solenoid relays OFF and LEDs OFF
  // Normally Open solenoids OFF (so they're open)
  closeValveWithLED(NOS2F_RELAY_PIN, NOS2F_LED_PIN);
  closeValveWithLED(NOS1F_RELAY_PIN, NOS1F_LED_PIN);
  closeValveWithLED(NOS2F_FLOW_RELAY_PIN, NOS2F_FLOW_LED_PIN);
  
  // Normally Closed solenoids OFF (so they're closed)
  closeValveWithLED(NCS1F_RELAY_PIN, NCS1F_LED_PIN);
  closeValveWithLED(NCS2F_RELAY_PIN, NCS2F_LED_PIN);
  closeValveWithLED(NCS3F_RELAY_PIN, NCS3F_LED_PIN);
  closeValveWithLED(NCS1O_RELAY_PIN, NCS1O_LED_PIN);
  closeValveWithLED(NCS2O_RELAY_PIN, NCS2O_LED_PIN);
  closeValveWithLED(NCS3O_RELAY_PIN, NCS3O_LED_PIN);

  // Status LEDs
  pinMode(LED_KEROSENE_SYSTEM, OUTPUT);
  pinMode(LED_GOX_SYSTEM, OUTPUT);
  pinMode(LED_STAGE2_COMPLETE, OUTPUT);
  digitalWrite(LED_KEROSENE_SYSTEM, LOW);
  digitalWrite(LED_GOX_SYSTEM, LOW);
  digitalWrite(LED_STAGE2_COMPLETE, LOW);

  Serial.begin(9600);
  Serial.println("System initialized. Potentiometers control hand ball valves.");
  Serial.println("Starting Stage 1: Startup sequence...");
}

float pressure1;
void loop() {
  // Execute Stage 1 first
  if (!stage1_complete) {
    executeStage1();
  }
  // Execute Stage 2 after Stage 1 is complete
  else if (!stage2_complete) {
    executeStage2();
  }
  // Both stages complete
  else {
    Serial.println("All stages complete. System ready.");
    delay(5000);
    // Uncomment to restart: stage1_complete = false; stage2_complete = false;
  }
}

void executeStage1() {
  Serial.println("Executing Stage 1: Startup");
  Serial.println("Please manually adjust hand ball valves using potentiometers as needed...");
  
  // Phase 1: Pressurization cycle
  do {
    // Check HBV1-F position and provide feedback
    hbv1f_open = checkHandBallValve(HBV1F_POT, "HBV1-F");
    if (hbv1f_open) {
      Serial.println("HBV1-F is open (manual control)");
    }

    // Turn on solenoid (energize relay and LED)
    openValveWithLED(SOLENOID_RELAY_PIN, SOLENOID_LED_PIN);
    Serial.println("Solenoid energized");

    // Check if HBV1-F is closed
    hbv1f_open = checkHandBallValve(HBV1F_POT, "HBV1-F");
    if (!hbv1f_open) {
      Serial.println("HBV1-F is closed (manual control)");
    } else {
      Serial.println("Waiting for HBV1-F to be closed manually...");
    }

    // Check HBV1-P position
    hbv1p_open = checkHandBallValve(HBV1P_POT, "HBV1-P");
    if (hbv1p_open) {
      Serial.println("HBV1-P is open (manual control)");
    } else {
      Serial.println("Waiting for HBV1-P to be opened manually...");
    }

    // Read pressure from first sensor
    pressure1 = readPressure(PRESSURE_SENSOR_1);
    Serial.print("Pressure 1 (psi): ");
    Serial.println(pressure1);

    delay(500); // Longer delay for manual adjustments

  } while (pressure1 < PRESSURE_THRESHOLD);

  Serial.println("Target pressure reached in Phase 1");

  // Phase 2: Output cycle
  float pressure2;
  do {
    // Check HBV1-O position
    hbv1o_open = checkHandBallValve(HBV1O_POT, "HBV1-O");
    if (hbv1o_open) {
      Serial.println("HBV1-O is open (manual control)");
    } else {
      Serial.println("Waiting for HBV1-O to be opened manually...");
    }

    // Read pressure from second sensor
    pressure2 = readPressure(PRESSURE_SENSOR_2);
    Serial.print("Pressure 2 (psi): ");
    Serial.println(pressure2);

    delay(500); // Delay for manual adjustments

  } while (pressure2 > PRESSURE_THRESHOLD && allValvesOpen());

  // Reset system state
  resetSystem();
  Serial.println("Stage 1 complete. Proceeding to Stage 2: Pre-Hotfire...");
  stage1_complete = true;
  delay(1000);
}

void executeStage2() {
  Serial.println("Executing Stage 2: Pre-Hotfire");
  keroseneSystem();
  gOxSystem();
  stage2_complete = true;
  digitalWrite(LED_STAGE2_COMPLETE, HIGH);
  Serial.println("Stage 2: Pre-Hotfire complete!");
}

void keroseneSystem() {
  Serial.println("Starting Kerosene System");
  digitalWrite(LED_KEROSENE_SYSTEM, HIGH);
  
  // CLOSE NOS2-F
  closeValveWithLED(NOS2F_RELAY_PIN, NOS2F_LED_PIN);
  Serial.println("NOS2-F closed");
  
  // OPEN NCS1-F; flows fuel to pump
  openValveWithLED(NCS1F_RELAY_PIN, NCS1F_LED_PIN);
  Serial.println("NCS1-F opened - fuel flowing to pump");
  
  do {
    // OPEN NOS1-F (vents kerosene/gas)
    openValveWithLED(NOS1F_RELAY_PIN, NOS1F_LED_PIN);
    Serial.println("NOS1-F opened - venting kerosene/gas");
    
    // READ PRESSURE TO ENSURE VENTED AND PRIMED
    pressure = readPressure(KEROSENE_PRESSURE_SENSOR);
    Serial.print("Kerosene Pressure (psi): ");
    Serial.println(pressure);

    delay(100);
  } while(pressure < KEROSENE_VENT_PRESSURE_THRESHOLD && pressure > KEROSENE_PRIME_PRESSURE_THRESHOLD);

  // CLOSE NOS1-F
  closeValveWithLED(NOS1F_RELAY_PIN, NOS1F_LED_PIN);
  Serial.println("NOS1-F closed");
  
  // OPEN NOS2-F
  openValveWithLED(NOS2F_RELAY_PIN, NOS2F_LED_PIN);
  Serial.println("NOS2-F opened");
  
  Serial.println("Kerosene System Complete");
}

void gOxSystem() {
  Serial.println("Starting GOx System");
  digitalWrite(LED_GOX_SYSTEM, HIGH);
  
  // Open NCS1-O
  openValveWithLED(NCS1O_RELAY_PIN, NCS1O_LED_PIN);
  Serial.println("NCS1-O opened - GOx flow initiated");
  
  Serial.println("GOx System Complete");
}

// HELPER FUNCTIONS

// Check hand ball valve position using potentiometer
bool checkHandBallValve(int potPin, String valveName) {
  int potValue = analogRead(potPin);
  bool isOpen = potValue > VALVE_OPEN_THRESHOLD;
  bool isClosed = potValue < VALVE_CLOSED_THRESHOLD;
  
  // Provide position feedback
  if (isOpen) {
    Serial.print(valveName + " position: OPEN (");
    Serial.print(potValue);
    Serial.println(")");
  } else if (isClosed) {
    Serial.print(valveName + " position: CLOSED (");
    Serial.print(potValue);
    Serial.println(")");
  } else {
    Serial.print(valveName + " position: INTERMEDIATE (");
    Serial.print(potValue);
    Serial.println(")");
  }
  
  return isOpen;
}

// Open valve with LED indicator (for solenoids)
void openValveWithLED(int relayPin, int ledPin) {
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, HIGH);
}

// Close valve with LED indicator (for solenoids)
void closeValveWithLED(int relayPin, int ledPin) {
  digitalWrite(relayPin, LOW);
  digitalWrite(ledPin, LOW);
}

// Read pressure sensor
float readPressure(int sensorPin) {
  int sensorValue = analogRead(sensorPin);
  float voltage = sensorValue * (VOLTAGE_REF / ADC_MAX);
  float pressure = voltage * PSI_PER_VOLT;
  return pressure;
}

// Check if all relevant valves are open
bool allValvesOpen() {
  return (hbv1f_open || hbv1p_open || hbv1o_open);
}

// Reset system to initial state
void resetSystem() {
  // Turn off solenoid relay and LED
  closeValveWithLED(SOLENOID_RELAY_PIN, SOLENOID_LED_PIN);
  
  // Reset state variables (hand ball valves controlled manually)
  hbv1f_open = false;
  hbv1p_open = false;
  hbv1o_open = false;
  
  Serial.println("System reset complete");
  Serial.println("Please manually close all hand ball valves using potentiometers");
}