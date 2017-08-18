/***************************************************
  Adafruit MQTT Library ESP8266 Example

  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino

  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "NETGEAR99"
#define WLAN_PASS       "silkyshoe359"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "fuzzybabybunny"
#define AIO_KEY         "4276a30af20d41a484a171b6cef921b8"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'pH' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish ph = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/ph");
Adafruit_MQTT_Publish ec = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/ec");
Adafruit_MQTT_Publish vernierSerialMonitor = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/vernier-serial-monitor");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

/*************************** Sketch Code ************************************/

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

/*************************** Pin Setup *************************************/

String sensorReading;
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];        // temporary array for use when parsing

      // variables to hold the parsed data
char messageFromPC[numChars] = {0};
int integerFromPC = 0;
float floatFromPC = 0.0;

boolean newData = false;

/*************************** Rolling Average Setup *************************************/

const int numReadings = 5;

float readingsPh[numReadings];      // the readings from the analog input
int readIndexPh = 0;              // the index of the current reading
float totalPh = 0;                  // the running total
float averagePh = 0;                // the average

float readingsEc[numReadings];      // the readings from the analog input
int readIndexEc = 0;              // the index of the current reading
float totalEc = 0;                  // the running total
float averageEc = 0;                // the average

void setup() {

  Serial.begin(115200);
  delay(10);

  Serial.println(F("Adafruit MQTT demo"));

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&onoffbutton);

  // Initialize all rolling average readings to 0
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readingsPh[thisReading] = 0;
  }
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readingsEc[thisReading] = 0;
  }
  
}

void loop() {

  while (Serial.available() == 0) {
  }

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbutton.lastread);
    }
  }

  // Now we can publish stuff!

  recvWithStartEndMarkers();
  if (newData == true) {
      strcpy(tempChars, receivedChars);
          // this temporary copy is necessary to protect the original data
          //   because strtok() used in parseData() replaces the commas with \0
//      publishVernierMonitor();
      parseData();
      showParsedData();
      calculateAndPublishRollingAverage();
      publishData();
      newData = false;
  }

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
    if(! mqtt.ping()) {
    mqtt.disconnect();
    }
  */
}

//============

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

//============

void parseData() {      // split the data into its parts

    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok(tempChars,":");      // get the first part - the string
    strcpy(messageFromPC, strtokIndx); // copy it to messageFromPC

    strtokIndx = strtok(NULL, ",");
    floatFromPC = atof(strtokIndx);     // convert this part to a float

}

//============

void showParsedData() {
    Serial.print("Message ");
    Serial.println(messageFromPC);
    Serial.print("Float ");
    Serial.println(floatFromPC);
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}

//============

void publishData(){
  // Now we can publish stuff!
  
  if (strcmp(messageFromPC, "ph") == 0){
    if (! ph.publish(floatFromPC)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
  } else if (strcmp(messageFromPC, "ec") == 0){
    if (! ec.publish(floatFromPC)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));
    }
  } else {
    Serial.println("invalid data key");
  }

}

//============

void calculateAndPublishRollingAverage(){
  
  if (strcmp(messageFromPC, "ph") == 0){
  
    // subtract the last reading:
    totalPh = totalPh - readingsPh[readIndexPh];
    // read from the sensor:
    readingsPh[readIndexPh] = floatFromPC;
    // add the reading to the total:
    totalPh = totalPh + readingsPh[readIndexPh];
    // advance to the next position in the array:
    readIndexPh = readIndexPh + 1;
  
    // if we're at the end of the array...
    if (readIndexPh >= numReadings) {
      // ...wrap around to the beginning:
      readIndexPh = 0;
    }
  
    // calculate the average:
    averagePh = totalPh / numReadings;
    // send it to the computer as ASCII digits
    Serial.println("averagePh: ");
    Serial.println(averagePh);
    
//    if (! ph.publish(floatFromPC)) {
//      Serial.println(F("Failed"));
//    } else {
//      Serial.println(F("OK!"));
//    }
  } else if (strcmp(messageFromPC, "ec") == 0){
//    if (! ec.publish(floatFromPC)) {
//      Serial.println(F("Failed"));
//    } else {
//      Serial.println(F("OK!"));
//    }
  } else {
    Serial.println("invalid data key");
  }

}

void publishVernierMonitor(){
  vernierSerialMonitor.publish(Serial.read());
}

