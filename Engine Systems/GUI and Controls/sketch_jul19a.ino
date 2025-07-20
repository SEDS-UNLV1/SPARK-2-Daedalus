// Combined Stage 1: Startup and Stage 2: Pre-Hotfire

// STAGE 1 PINS - Define relay pins with consistent naming
// HBV1-F (Hand Ball Valve 1 - Fill)
#define HBV1F_OPEN 3
#define HBV1F_CLOSE 4

// NOS1-P (Normally Open Solenoid 1 - Pressurize)
#define SOLENOID_PIN 5

// HBV1-P (Hand Ball Valve 1 - Pressurize)
#define HBV1P_OPEN 6
#define HBV1P_CLOSE 7

// PG1-P (Pressure Gauge 1 - Pressurize)
#define PRESSURE_SENSOR_1 A0  // Use analog pin A0 instead of digital pin 8
#define PRESSURE_SENSOR_2 A1  // Use analog pin A1 instead of digital pin 12

// HBV1-O (Hand Ball Valve 1 - Output)
#define HBV1O_OPEN 10
#define HBV1O_CLOSE 11

// STAGE 2 PINS - KEROSENE SYSTEM PINS
// NOS2-F (Normally Open Solenoid 2 - Fuel)
#define NOS2F_PIN 22
// NCS1-F (Normally Closed Solenoid 1 - Fuel) - flows fuel to pump
#define NCS1F_PIN 23
// NOS1-F (Normally Open Solenoid 1 - Fuel) - vents gas/kerosene
#define NOS1F_PIN 24
// NOS2-F (Normally Open Solenoid 2 - Fuel) - flow kerosene to NCS2-F, NCS3-F
#define NOS2F_FLOW_PIN 25
// NCS2-F and NCS3-F control (Normally Closed Solenoids - Fuel)
#define NCS2F_PIN 26
#define NCS3F_PIN 27

// GOx SYSTEM PINS  
// NCS1-O (Normally Closed Solenoid 1 - Oxidizer) - flow GOx to NCS2-O, NCS3-O
#define NCS1O_PIN 28
#define NCS2O_PIN 29
#define NCS3O_PIN 30
// PRESSURE TRANSDUCERS for system monitoring
#define KEROSENE_PRESSURE_SENSOR A2
#define GOX_PRESSURE_SENSORS A3
#define FUEL_PUMP_PRESSURE_SENSOR A4
// STATUS LEDs
#define LED_KEROSENE_SYSTEM 31
#define LED_GOX_SYSTEM 32
#define LED_STAGE2_COMPLETE 33
// System status indicators
#define KEROSENE_SYSTEM_POWER_STATUS A5

// Constants
const float PRESSURE_THRESHOLD = 450.0;
const float KEROSENE_VENT_PRESSURE_THRESHOLD = 100.0;  // PSI - system considered vented below this
const float KEROSENE_PRIME_PRESSURE_THRESHOLD = 300.0; // PSI - system considered primed above this
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
  // STAGE 1 SETUP - Set relay pins as outputs
  pinMode(HBV1F_OPEN, OUTPUT);
  pinMode(HBV1F_CLOSE, OUTPUT);
  pinMode(HBV1P_OPEN, OUTPUT);
  pinMode(HBV1P_CLOSE, OUTPUT);
  pinMode(HBV1O_OPEN, OUTPUT);
  pinMode(HBV1O_CLOSE, OUTPUT);
  pinMode(SOLENOID_PIN, OUTPUT);

  // Initialize all valves to closed state
  closeValve(HBV1F_OPEN, HBV1F_CLOSE);
  closeValve(HBV1P_OPEN, HBV1P_CLOSE);
  closeValve(HBV1O_OPEN, HBV1O_CLOSE);
  digitalWrite(SOLENOID_PIN, LOW);

  // STAGE 2 SETUP - Initialize kerosene system pins
  pinMode(NOS2F_PIN, OUTPUT);
  pinMode(NCS1F_PIN, OUTPUT);
  pinMode(NOS1F_PIN, OUTPUT);
  pinMode(NOS2F_FLOW_PIN, OUTPUT);
  pinMode(NCS2F_PIN, OUTPUT);
  pinMode(NCS3F_PIN, OUTPUT);
  
  // Initialize GOx system pins
  pinMode(NCS1O_PIN, OUTPUT);
  pinMode(NCS2O_PIN, OUTPUT);
  pinMode(NCS3O_PIN, OUTPUT);

  // Normally Open solenoids OFF (so they're open)
  digitalWrite(NOS2F_PIN, LOW);
  digitalWrite(NOS1F_PIN, LOW);
  digitalWrite(NOS2F_FLOW_PIN, LOW);
  
  // Normally Closed solenoids OFF (so they're closed)
  digitalWrite(NCS1F_PIN, LOW);
  digitalWrite(NCS2F_PIN, LOW);
  digitalWrite(NCS3F_PIN, LOW);
  digitalWrite(NCS1O_PIN, LOW);
  digitalWrite(NCS2O_PIN, LOW);
  digitalWrite(NCS3O_PIN, LOW);

  // Turn off status LEDs initially
  pinMode(LED_KEROSENE_SYSTEM, OUTPUT);
  pinMode(LED_GOX_SYSTEM, OUTPUT);
  pinMode(LED_STAGE2_COMPLETE, OUTPUT);
  digitalWrite(LED_KEROSENE_SYSTEM, LOW);
  digitalWrite(LED_GOX_SYSTEM, LOW);
  digitalWrite(LED_STAGE2_COMPLETE, LOW);

  Serial.begin(9600);
  Serial.println("System initialized. Starting Stage 1: Startup sequence...");
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
  // Both stages complete - could add restart logic or halt here
  else {
    Serial.println("All stages complete. System ready.");
    delay(5000); // Wait before potential restart
    // Uncomment next line to restart the sequence
    // stage1_complete = false; stage2_complete = false;
  }
}

void executeStage1() {
  Serial.println("Executing Stage 1: Startup");
  
  // Phase 1: Pressurization cycle
  do {
    // Open HBV1-F (Fill valve)
    openValve(HBV1F_OPEN, HBV1F_CLOSE);
    hbv1f_open = true;
    Serial.println("HBV1-F opened");

    // Turn on solenoid (energize coil)
    digitalWrite(SOLENOID_PIN, HIGH);
    Serial.println("Solenoid energized");

    // Close HBV1-F (Fill valve)
    closeValve(HBV1F_OPEN, HBV1F_CLOSE);
    hbv1f_open = false;
    Serial.println("HBV1-F closed");

    // Open HBV1-P (Pressurize valve)
    openValve(HBV1P_OPEN, HBV1P_CLOSE);
    hbv1p_open = true;
    Serial.println("HBV1-P opened");

    // Read pressure from first sensor
    pressure1 = readPressure(PRESSURE_SENSOR_1);
    Serial.print("Pressure 1 (psi): ");
    Serial.println(pressure1);

    delay(100); // Small delay for stability

  } while (pressure1 < PRESSURE_THRESHOLD);

  Serial.println("Target pressure reached in Phase 1");

  // Phase 2: Output cycle
  float pressure2;
  do {
    // Open HBV1-O (Output valve)
    openValve(HBV1O_OPEN, HBV1O_CLOSE);
    hbv1o_open = true;
    Serial.println("HBV1-O opened");

    // Read pressure from second sensor
    pressure2 = readPressure(PRESSURE_SENSOR_2);
    Serial.print("Pressure 2 (psi): ");
    Serial.println(pressure2);

    delay(100); // Small delay for stability

    // Continue loop while pressure is above threshold
    // When pressure drops below threshold, the loop will exit
  } while (pressure2 > PRESSURE_THRESHOLD && allValvesOpen());

  // Reset system state for next cycle
  resetSystem();
  Serial.println("Stage 1 complete. Proceeding to Stage 2: Pre-Hotfire...");
  stage1_complete = true;
  delay(1000); // Brief pause before next stage
}

void executeStage2() {
  Serial.println("Executing Stage 2: Pre-Hotfire");
  keroseneSystem();
  gOxSystem();
  stage2_complete = true;
  digitalWrite(LED_STAGE2_COMPLETE, HIGH);
  Serial.println("Stage 2: Pre-Hotfire complete!");
}

void keroseneSystem()
{
  Serial.println("Starting Kerosene System");
  digitalWrite(LED_KEROSENE_SYSTEM, HIGH);
  
  //CLOSE NOS2-F
  closeValve(NOS2F_PIN);
  //OPEN NCS1-F; flows fuel to pump
  openValve(NCS1F_PIN);
  do
  {
    //OPEN NOS1-F (vents kerosene/gas)
    openValve(NOS1F_PIN);
    //READ PRESSURE TO ENSURE VENTED AND PRIMED
    pressure = readPressure(KEROSENE_PRESSURE_SENSOR);
    Serial.print("Kerosene Pressure (psi): ");
    Serial.println(pressure);

    delay(100); // Small delay for stability
  } while(pressure < KEROSENE_VENT_PRESSURE_THRESHOLD && pressure > KEROSENE_PRIME_PRESSURE_THRESHOLD);

  //CLOSE NOS1-F
  closeValve(NOS1F_PIN);
  //OPEN NOS2-F
  openValve(NOS2F_PIN);
  Serial.println("Kerosene System Complete");
}

void gOxSystem()
{
  Serial.println("Starting GOx System");
  digitalWrite(LED_GOX_SYSTEM, HIGH);
  openValve(NCS1O_PIN);
  Serial.println("GOx System Complete");
}

// STAGE 1 HELPER FUNCTIONS
// Helper function to open a valve
void openValve(int openPin, int closePin) {
  digitalWrite(openPin, HIGH);
  digitalWrite(closePin, LOW);
}

// Helper function to close a valve
void closeValve(int openPin, int closePin) {
  digitalWrite(openPin, LOW);
  digitalWrite(closePin, HIGH);
}

// Helper function to read and convert pressure sensor value
float readPressure(int sensorPin) {
  int sensorValue = analogRead(sensorPin);
  float voltage = sensorValue * (VOLTAGE_REF / ADC_MAX);
  float pressure = voltage * PSI_PER_VOLT;
  return pressure;
}

// Function to check if all valves that should be open are open
bool allValvesOpen() {
  // This function checks if all relevant valves are still open
  return (hbv1f_open || hbv1p_open || hbv1o_open);
}

// Function to reset system to initial state
void resetSystem() {
  // Close all valves
  closeValve(HBV1F_OPEN, HBV1F_CLOSE);
  closeValve(HBV1P_OPEN, HBV1P_CLOSE);
  closeValve(HBV1O_OPEN, HBV1O_CLOSE);
  
  // Turn off solenoid
  digitalWrite(SOLENOID_PIN, LOW);
  
  // Reset state variables
  hbv1f_open = false;
  hbv1p_open = false;
  hbv1o_open = false;
  
  Serial.println("System reset complete");
}

// STAGE 2 HELPER FUNCTIONS
// Open valve helper function (single pin version for Stage 2)
void openValve(int pin) {
  digitalWrite(pin, HIGH);
}

// Close valve helper function (single pin version for Stage 2)
void closeValve(int pin) {
  digitalWrite(pin, LOW);
}