#include "dht_nonblocking.h"
#define DHT_SENSOR_TYPE DHT_TYPE_11
//String data;
char d1;
static const int DHT_SENSOR_PIN = 2;
DHT_nonblocking dht_sensor( DHT_SENSOR_PIN, DHT_SENSOR_TYPE );


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(11, OUTPUT);
  pinMode(10, OUTPUT);
}
  /*
 * Poll for a measurement, keeping the state machine alive.  Returns
 * true if a measurement is available.
 */
static bool measure_environment( float *temperature, float *humidity )
{
  static unsigned long measurement_timestamp = millis( );

  /* Measure once every four seconds. */
  if( millis( ) - measurement_timestamp > 3000ul )
  {
    if( dht_sensor.measure( temperature, humidity ) == true )
    {
      measurement_timestamp = millis( );
      return( true );
    }
  }

  return( false );
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()){
    //data = Serial.readString();
    //d1 = data.charAt(0);
    d1 = Serial.read();
    switch (d1) {
      case 'A': digitalWrite(13, HIGH);
      break;
      case 'a': digitalWrite(13, LOW);
      break;
      case 'B': digitalWrite(12, HIGH);
      break;
      case 'b': digitalWrite(12, LOW);
      break;
      case 'C': digitalWrite(11, HIGH);
      break;
      case 'c': digitalWrite(11, LOW);
      break;
      case 'D': digitalWrite(10, HIGH);
      break;
      case 'd': digitalWrite(10, LOW);
      break;
      default:
            digitalWrite(6, LOW);
    }
  }
  
  float temperature;
  float humidity;

  /* Measure temperature and humidity.  If the functions returns
     true, then a measurement is available. */
if (measure_environment(&temperature, &humidity) == true) {
    Serial.print("Temp: "); // Change this line
    Serial.print(temperature, 1);
    Serial.print(" deg. C, H = ");
    Serial.print(humidity, 1);
    Serial.println("%");
}
}
