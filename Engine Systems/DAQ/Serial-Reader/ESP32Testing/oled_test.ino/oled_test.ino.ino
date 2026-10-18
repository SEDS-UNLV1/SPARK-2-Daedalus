#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define OLED screen resolution
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Define OLED I2C address (default is usually 0x3C or 0x3D)
#define OLED_ADDR   0x3C

// Create OLED object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  Wire.begin(21,22);
  // Initialize OLED screen
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Infinite loop to terminate program
  }
  
  // Clear display buffer
  display.clearDisplay();
  
  // Set text size and color
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Display initialization information
  display.setCursor(0, 0);
  display.println(F("OLED Test Program"));
  display.println(F("Arduino UNO"));
  display.println(F("SSD1306 128x64"));
  display.println(F("Initializing..."));
  
  // Update display
  display.display();
  
  // Delay for 2 seconds
  delay(2000);
}

void loop() {
  // Test 1: Display text
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Test 1: Text Display"));
  display.println(F("Line 1"));
  display.println(F("Line 2"));
  display.println(F("Line 3"));
  display.println(F("Line 4"));
  display.display();
  delay(2000);
  
  // Test 2: Display text with different sizes
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(F("Text Size 1"));
  display.setTextSize(2);
  display.println(F("Size 2"));
  display.setTextSize(3);
  display.println(F("Size 3"));
  display.display();
  delay(2000);
  
  // Test 3: Display graphics (rectangles and circles)
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Test 3: Graphics"));
  
  // Draw rectangles
  display.drawRect(10, 20, 50, 30, SSD1306_WHITE);
  display.fillRect(70, 20, 50, 30, SSD1306_WHITE);
  
  // Draw circles
  display.drawCircle(35, 40, 10, SSD1306_WHITE);
  display.fillCircle(95, 40, 10, SSD1306_WHITE);
  
  display.display();
  delay(2000);
  
  // Test 4: Display lines
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Test 4: Lines"));
  
  // Draw various lines
  for(int i=0; i<SCREEN_WIDTH; i+=4) {
    display.drawLine(0, 0, i, SCREEN_HEIGHT-1, SSD1306_WHITE);
  }
  
  display.display();
  delay(2000);
  
  // Test 5: Display scrolling text
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Test 5: Scrolling"));
  display.println(F("Hello, Arduino!"));
  display.println(F("OLED Test"));
  display.println(F("1234567890"));
  display.display();
  
  // Horizontal scrolling
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(500);
  
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(500);
  
  // Clear display and show completion message
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(F("Test Completed!"));
  display.println(F("Restarting..."));
  display.display();
  delay(2000);
}
