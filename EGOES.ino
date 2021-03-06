#include "Adafruit_FONA.h" // https://github.com/botletics/SIM7000-LTE-Shield/tree/master/Code

// You don't need the following includes if you're not using MQTT
// You can find the Adafruit MQTT library here: https://github.com/adafruit/Adafruit_MQTT_Library
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"
#include <SoftwareSerial.h>//include the library code


SoftwareSerial bluetooth(2, 3);//Bluetooth

#if defined(ARDUINO_SAMD_ZERO) && defined(SERIAL_PORT_USBVIRTUAL)
  // Required for Serial on Zero based boards
  #define Serial SERIAL_PORT_USBVIRTUAL
#endif

// Define *one* of the following lines:
#define SIMCOM_7000 // SIM7000

// to send data to the cloud! Leave the other commented out
#define PROTOCOL_MQTT_CLOUDMQTT   // CloudMQTT

/************************* PIN DEFINITIONS *********************************/
// For SIM7000 shield
#define FONA_PWRKEY 6
#define FONA_RST 7
//#define FONA_DTR 8 // Connect with solder jumper
//#define FONA_RI 9 // Need to enable via AT commands
#define FONA_TX 4 // Microcontroller RX
#define FONA_RX 5 // Microcontroller TX
//#define T_ALERT 12 // Connect with solder jumper

#define LED A1 // Just for testing if needed!

// We default to using software serial. If you want to use hardware serial
// (because softserial isnt supported) comment out the following three lines 
// and uncomment the HardwareSerial line
//#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);

SoftwareSerial *fonaSerial = &fonaSS;

// Use this for 2G modules
#ifdef SIMCOM_2G
  Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
  
// Use this one for 3G modules
#elif defined(SIMCOM_3G)
  Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);
  
// Use this one for LTE CAT-M/NB-IoT modules (like SIM7000)
// Notice how we don't include the reset pin because it's reserved for emergencies on the LTE module!
#elif defined(SIMCOM_7000) || defined(SIMCOM_7500)
  Adafruit_FONA_LTE fona = Adafruit_FONA_LTE();
#endif

//#elif defined(PROTOCOL_MQTT_CLOUDMQTT)
//  /************************* MQTT SETUP *********************************/
//  // For CloudMQTT find these under the "Details" tab:
//  #define MQTT_SERVER      "tailor.cloudmqtt.com"
//  #define MQTT_SERVERPORT  14567
//  #define MQTT_USERNAME    "kjridxod"
//  #define MQTT_KEY         "ZSCI7UfAKLOV"
//#endif

//#elif defined(PROTOCOL_MQTT_CLOUDMQTT)
  /************************* MQTT SETUP *********************************/
  // For CloudMQTT find these under the "Details" tab:
  #define MQTT_SERVER      "tailor.cloudmqtt.com"
  #define MQTT_SERVERPORT  14491
  #define MQTT_USERNAME    "xjvsguep"
  #define MQTT_KEY         "MqQTOUrl0o8c"
//#endif

#define SUB_TOPIC       "command" 
/****************************** OTHER STUFF ***************************************/
// For sleeping the AVR
#include <avr/sleep.h>
#include <avr/power.h>

// For temperature sensor
#include <Wire.h>

// The following line is used for applications that require repeated data posting, like GPS trackers
// Comment it out if you only want it to post once, not repeatedly every so often
#define samplingRate 10 // The time in between posts, in seconds

// The following line can be used to turn off the shield after posting data. This
// could be useful for saving energy for sparse readings but keep in mind that it
// will take longer to get a fix on location after turning back on than if it had
// already been on. Comment out to leave the shield on after it posts data.
//#define turnOffShield // Turn off shield after posting data

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);
char imei[16] = {0}; // Use this for device ID
uint8_t type;
uint16_t battLevel = 0; // Battery level (percentage)
float latitude, longitude, speed_kph, heading, altitude, second;
uint16_t year;
uint8_t month, day, hour, minute;
uint8_t counter = 0;
uint8_t datates = 100;
//char PIN[5] = "1234"; // SIM card PIN

char button[255];

char URL[200];  // Make sure this is long enough for your request URL
char body[100]; // Make sure this is long enough for POST body
char latBuff[12], longBuff[12], locBuff[50], speedBuff[12],
     headBuff[12], altBuff[12], battBuff[12], dataBuff[12];

void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);
  Serial.println(F("*** SIMCom Module IoT Example ***"));

//  #ifdef LED
    pinMode(LED, OUTPUT);
//    digitalWrite(LED, LOW);
//  #endif
  
  pinMode(FONA_RST, OUTPUT);
  digitalWrite(FONA_RST, HIGH); // Default state

  pinMode(FONA_PWRKEY, OUTPUT);
  powerOn(); // Power on the module
  moduleSetup(); // Establishes first-time serial comm and prints IMEI

  // Unlock SIM card if needed
  // Remember to uncomment the "PIN" variable definition above
  /*
  if (!fona.unlockSIM(PIN)) {
    Serial.println(F("Failed to unlock SIM card"));
  }
  */

  // Set modem to full functionality
  fona.setFunctionality(1); // AT+CFUN=1

  // Configure a GPRS APN, username, and password.
  // You might need to do this to access your network's GPRS/data
  // network.  Contact your provider for the exact APN, username,
  // and password values.  Username and password are optional and
  // can be removed, but APN is required.
  fona.setNetworkSettings(F("indosatgprs"), F("indosat"), F("indosat"));


  // Perform first-time GPS/GPRS setup if the shield is going to remain on,
  // otherwise these won't be enabled in loop() and it won't work!
#ifndef turnOffShield
  // Enable GPS
  while (!fona.enableGPS(true)) {
    Serial.println(F("Failed to turn on GPS, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Turned on GPS!"));

  #if !defined(SIMCOM_3G) && !defined(SIMCOM_7500)
    // Disable GPRS just to make sure it was actually off so that we can turn it on
    if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS!"));
    
    // Turn on GPRS
    while (!fona.enableGPRS(true)) {
      Serial.println(F("Failed to enable GPRS, retrying..."));
      delay(2000); // Retry every 2s
    }
    Serial.println(F("Enabled GPRS!"));
  #endif
#endif

}

void loop() {
//  bt();
  sim();
}
void sim(){
  // Connect to cell network and verify connection
  // If unsuccessful, keep retrying every 2s until a connection is made
  while (!netStatus()) {
    Serial.println(F("Failed to connect to cell network, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Connected to cell network!"));

  // Measure battery level
  // Note: on the LTE shield this won't be accurate because the SIM7000
  // is supplied by a regulated 3.6V, not directly from the battery. You
  // can use the Arduino and a voltage divider to measure the battery voltage
  // and use that instead, but for now we will use the function below
  // only for testing.
  battLevel = readVcc(); // Get voltage in mV

  delay(500); // I found that this helps

  // Turn on GPS if it wasn't on already (e.g., if the module wasn't turned off)
#ifdef turnOffShield
  while (!fona.enableGPS(true)) {
    Serial.println(F("Failed to turn on GPS, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Turned on GPS!"));
#endif

  // Get a fix on location, try every 2s
  // Use the top line if you want to parse UTC time data as well, the line below it if you don't care
//  while (!fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude, &year, &month, &day, &hour, &minute, &second)) {
  while (!fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude)) {
    Serial.println(F("Failed to get GPS location, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Found 'eeeeem!"));
  Serial.println(F("---------------------"));
  Serial.print(F("Latitude: ")); Serial.println(latitude, 6);
  Serial.print(F("Longitude: ")); Serial.println(longitude, 6);
  Serial.print(F("Speed: ")); Serial.println(speed_kph);
  Serial.print(F("Heading: ")); Serial.println(heading);
  Serial.print(F("Altitude: ")); Serial.println(altitude);
  /*
  // Uncomment this if you care about parsing UTC time
  Serial.print(F("Year: ")); Serial.println(year);
  Serial.print(F("Month: ")); Serial.println(month);
  Serial.print(F("Day: ")); Serial.println(day);
  Serial.print(F("Hour: ")); Serial.println(hour);
  Serial.print(F("Minute: ")); Serial.println(minute);
  Serial.print(F("Second: ")); Serial.println(second);
  */
  Serial.println(F("---------------------"));

#if defined(turnOffShield) && !defined(SIMCOM_3G) && !defined(SIMCOM_7500) // If the shield was already on, no need to re-enable
  // Disable GPRS just to make sure it was actually off so that we can turn it on
  if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS!"));
  
  // Turn on GPRS
  while (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to enable GPRS, retrying..."));
    delay(2000); // Retry every 2s
  }
  Serial.println(F("Enabled GPRS!"));
#endif

  // Post something like temperature and battery level to the web API
  // Construct URL and post the data to the web API

  // Format the floating point numbers
  dtostrf(latitude, 1, 6, latBuff);
  dtostrf(longitude, 1, 6, longBuff);
  dtostrf(speed_kph, 1, 0, speedBuff);
  dtostrf(heading, 1, 0, headBuff);
  dtostrf(altitude, 1, 1, altBuff);
  dtostrf(battLevel, 1, 0, battBuff);
  dtostrf(datates, 1, 0, dataBuff);

  // Also construct a combined, comma-separated location array
  // (many platforms require this for dashboards, like Adafruit IO):
  
  sprintf(locBuff, "%s,%s", latBuff, longBuff); // This could look like "10,33.123456,-85.123456,120.5"
  
  // Construct the appropriate URL's and body, depending on request type
  // In this example we use the IMEI as device ID

#ifdef PROTOCOL_HTTP_GET
  // GET request
  
  counter = 0; // This counts the number of failed attempts tries
  
#elif defined(PROTOCOL_HTTP_POST)  
  // You can also do a POST request instead

  counter = 0; // This counts the number of failed attempts tries
  
  // Let's use MQTT!
  
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected). See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // Now publish all the data to different feeds!
  // The MQTT_publish_checkSuccess handles repetitive stuff.
  // You can see the function near the end of this sketch.
  // For the Adafruit IO dashboard map we send the combined lat/long buffer
  MQTT_publish_checkSuccess(feed_location, locBuff);
//  MQTT_publish_checkSuccess(feed_speed, speedBuff); // Included in "location" feed
  MQTT_publish_checkSuccess(feed_head, headBuff);
//  MQTT_publish_checkSuccess(feed_alt, altBuff); // Included in "location" feed
  MQTT_publish_checkSuccess(feed_voltage, battBuff);
  MQTT_publish_checkSuccess(feed_data, dataBuff);
///////////////////////////////////////////////////////////////
/*
    uint8_t i = 0;
    if (fona.available()) {
      while (fona.available()) {
        button[i] = fona.read();
        i++;
      }
if (strstr(button, "+SMSUB:") != NULL) {
        Serial.println(F("*** Received MQTT message! ***"));
        
        char *p = strtok(button, ",\"");
        char *topic_p = strtok(NULL, ",\"");
        char *message_p = strtok(NULL, ",\"");
        
        Serial.print(F("Topic: ")); Serial.println(topic_p);
        Serial.print(F("Message: ")); Serial.println(message_p);
  
        // Do something with the message
        // For example, if the topic was "command" and we received a "yes", turn on an LED!
        if (strcmp(topic_p, "command") == 0) {
          if (strcmp(message_p, "yes") == 0) {
            Serial.println(F("Turning on LED!"));
            digitalWrite(LED, HIGH);
          }
          else if (strcmp(message_p, "no") == 0) {
            Serial.println(F("Turning off LED!"));
            digitalWrite(LED, HIGH);
          }
        }
      }
*/
fonaSS.println("AT+SMSUB=???button???,1");
//MQTT_subscribe(button,2);
if(button == "on"){
  digitalWrite(LED, HIGH);
}
if(button == "off"){
  digitalWrite(LED, LOW);
}

/////////////////////////////////////////////////////////////////
#elif defined(PROTOCOL_MQTT_CLOUDMQTT)
  // Let's use CloudMQTT! NOTE: connecting and publishing work, but everything else
  // still under development!!!
  char MQTT_CLIENT[16] = " ";  // We'll change this to the IMEI
  
  // Let's begin by changing the client name to the IMEI number to better identify
  strcpy(MQTT_CLIENT, imei); // Copy the contents of the imei into the char array "MQTT_client"

  // Connect to MQTT broker
  if (!fona.TCPconnect(MQTT_SERVER, MQTT_SERVERPORT)) Serial.println(F("Failed to connect to TCP/IP!"));
  // CloudMQTT requires "MQIsdp" instead of "MQTT"
  if (!fona.MQTTconnect("MQIsdp", MQTT_CLIENT, MQTT_USERNAME, MQTT_KEY)) Serial.println(F("Failed to connect to MQTT broker!"));
  
  // Publish each data point under a different topic!
  Serial.println(F("Publishing data to their respective topics!"));  
//  if (!fona.MQTTpublish("latitude", latBuff)) Serial.println(F("Failed to publish data!")); // Can send individually if needed
//  if (!fona.MQTTpublish("longitude", longBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("location", locBuff)) Serial.println(F("Failed to publish data!")); // Combined data
  if (!fona.MQTTpublish("speed", speedBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("heading", headBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("altitude", altBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("voltage", battBuff)) Serial.println(F("Failed to publish data!"));
  if (!fona.MQTTpublish("data", dataBuff)) Serial.println(F("Failed to publish data!"));
  

  // Close TCP connection
  if (!fona.TCPclose()) Serial.println(F("Failed to close connection!"));

#endif

  //Only run the code below if you want to turn off the shield after posting data
#ifdef turnOffShield
  // Disable GPRS
  // Note that you might not want to check if this was successful, but just run it
  // since the next command is to turn off the module anyway
  if (!fona.enableGPRS(false)) Serial.println(F("Failed to disable GPRS!"));

  // Turn off GPS
  if (!fona.enableGPS(false)) Serial.println(F("Failed to turn off GPS!"));
  
  // Power off the module. Note that you could instead put it in minimum functionality mode
  // instead of completely turning it off. Experiment different ways depending on your application!
  // You should see the "PWR" LED turn off after this command
//  if (!fona.powerDown()) Serial.println(F("Failed to power down FONA!")); // No retries
  counter = 0;
  while (counter < 3 && !fona.powerDown()) { // Try shutting down 
    Serial.println(F("Failed to power down FONA!"));
    counter++; // Increment counter
    delay(1000);
  }
#endif
  
  // Alternative to the AT command method above:
  // If your FONA has a PWRKEY pin connected to your MCU, you can pulse PWRKEY
  // LOW for a little bit, then pull it back HIGH, like this:
//  digitalWrite(PWRKEY, LOW);
//  delay(600); // Minimum of 64ms to turn on and 500ms to turn off for FONA 3G. Check spec sheet for other types
//  delay(1300); // Minimum of 1.2s for SIM7000
//  digitalWrite(PWRKEY, HIGH);
  
  // Shut down the MCU to save power
#ifndef samplingRate
  Serial.println(F("Shutting down..."));
  delay(5); // This is just to read the response of the last AT command before shutting down
  MCU_powerDown(); // You could also write your own function to make it sleep for a certain duration instead
#else
  // The following lines are for if you want to periodically post data (like GPS tracker)
  Serial.print(F("Waiting for ")); Serial.print(samplingRate); Serial.println(F(" seconds\r\n"));
//  delay(samplingRate * 1000UL); // Delay
  
  powerOn(); // Powers on the module if it was off previously

  // Only run the initialization again if the module was powered off
  // since it resets back to 115200 baud instead of 4800.
  #ifdef turnOffShield
    moduleSetup();
  #endif
    
#endif

}

// Power on the module
void powerOn() {
  digitalWrite(FONA_PWRKEY, LOW);
  // See spec sheets for your particular module
  #if defined(SIMCOM_2G)
    delay(1050);
  #elif defined(SIMCOM_3G)
    delay(180); // For SIM5320
  #elif defined(SIMCOM_7000)
    delay(100); // For SIM7000
  #elif defined(SIMCOM_7500)
    delay(500); // For SIM7500
  #endif
  
  digitalWrite(FONA_PWRKEY, HIGH);
}

void moduleSetup() {
  // Note: The SIM7000A baud rate seems to reset after being power cycled (SIMCom firmware thing)
  // SIM7000 takes about 3s to turn on but SIM7500 takes about 15s
  // Press reset button if the module is still turning on and the board doesn't find it.
  // When the module is on it should communicate right after pressing reset
  fonaSS.begin(115200); // Default SIM7000 shield baud rate
  
  Serial.println(F("Configuring to 9600 baud"));
  fonaSS.println("AT+IPR=9600"); // Set baud rate
  delay(100); // Short pause to let the command run
  fonaSS.begin(9600);
  if (! fona.begin(fonaSS)) {
    Serial.println(F("Couldn't find FONA"));
    while(1); // Don't proceed if it couldn't find the device
  }

  type = fona.type();
  Serial.println(F("FONA is OK"));
  Serial.print(F("Found "));
  switch (type) {
    case SIM800L:
      Serial.println(F("SIM800L")); break;
    case SIM800H:
      Serial.println(F("SIM800H")); break;
    case SIM808_V1:
      Serial.println(F("SIM808 (v1)")); break;
    case SIM808_V2:
      Serial.println(F("SIM808 (v2)")); break;
    case SIM5320A:
      Serial.println(F("SIM5320A (American)")); break;
    case SIM5320E:
      Serial.println(F("SIM5320E (European)")); break;
    case SIM7000A:
      Serial.println(F("SIM7000A (American)")); break;
    case SIM7000C:
      Serial.println(F("SIM7000C (Chinese)")); break;
    case SIM7000E:
      Serial.println(F("SIM7000E (European)")); break;
    case SIM7000G:
      Serial.println(F("SIM7000G (Global)")); break;
    case SIM7500A:
      Serial.println(F("SIM7500A (American)")); break;
    case SIM7500E:
      Serial.println(F("SIM7500E (European)")); break;
    default:
      Serial.println(F("???")); break;
  }
  
  // Print module IMEI number.
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("Module IMEI: "); Serial.println(imei);
  }
}

// Read the module's power supply voltage
float readVcc() {
  // Read battery voltage
  if (!fona.getBattVoltage(&battLevel)) Serial.println(F("Failed to read batt"));
  else Serial.print(F("battery = ")); Serial.print(battLevel); Serial.println(F(" mV"));

  // Read LiPo battery percentage
  // Note: This will NOT work properly on the LTE shield because the voltage
  // is regulated to 3.6V so you will always read about the same value!
//  if (!fona.getBattPercent(&battLevel)) Serial.println(F("Failed to read batt"));
//  else Serial.print(F("BAT % = ")); Serial.print(battLevel); Serial.println(F("%"));

  return battLevel;
}

bool netStatus() {
  int n = fona.getNetworkStatus();
  
  Serial.print(F("Network status ")); Serial.print(n); Serial.print(F(": "));
  if (n == 0) Serial.println(F("Not registered"));
  if (n == 1) Serial.println(F("Registered (home)"));
  if (n == 2) Serial.println(F("Not registered (searching)"));
  if (n == 3) Serial.println(F("Denied"));
  if (n == 4) Serial.println(F("Unknown"));
  if (n == 5) Serial.println(F("Registered roaming"));

  if (!(n == 1 || n == 5)) return false;
  else return true;
}



// Turn off the MCU completely. Can only wake up from RESET button
// However, this can be altered to wake up via a pin change interrupt
void MCU_powerDown() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0; // Turn off ADC
  power_all_disable ();  // Power off ADC, Timer 0 and 1, serial interface
  sleep_enable();
  sleep_cpu();
}
void bt(){
  bluetooth.listen();
  Serial.println("Data from Bluetooth:");
  while (bluetooth.available() > 0) {
    int inByte = bluetooth.read();
    Serial.write(inByte);
    if(inByte == '1'){
      digitalWrite(LED,HIGH);
      Serial.println("Motor On");
    }
    if(inByte == '0'){
      digitalWrite(LED,LOW);
      Serial.println("Motor Off");
    }
  }
}
