
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "<ssid>"
#define WLAN_PASS       "<password>"

#define WLAN2_SSID       "<ssid>"
#define WLAN2_PASS       "<password>"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "<username>"
#define AIO_KEY         "<key>"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/
Adafruit_MQTT_Publish foodprobe = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/foodprobe");

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
boolean connectedToBroker = false;
boolean connectingToBroker = false;
boolean connectedToWifi = false;
boolean publishedValue = false;
float temperature;
int mqtt_time_counter = 0;

void setup(void) {

  /* Setup oled display */
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,24);
  display.println("IoT Probe 1.0");
  display.display();
  delay(2000);
  Serial.begin(9600);

  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  /* Try to connect to first SSID  */
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  int counter = 1;
  while (WiFi.status() != WL_CONNECTED) {
    counter++;

    display.clearDisplay();
    printWifi(4);
    display.clearDisplay();
    printWifi(3);
    display.clearDisplay();
    printWifi(2);
    display.clearDisplay();
    printWifi(1);

    if (counter > 15) {
      Serial.println("Could not connect to WIFI. Aborting..");
      display.clearDisplay();
      printWifiDisconnected();
      break;
    }
  }

  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN2_SSID);

  /* Change SSID if not connected */
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WLAN2_SSID, WLAN2_PASS);
    counter = 1;
  }

  while (WiFi.status() != WL_CONNECTED) {
    counter++;
    display.clearDisplay();
    printWifi(4);
    display.clearDisplay();
    printWifi(3);
    display.clearDisplay();
    printWifi(2);
    display.clearDisplay();
    printWifi(1);

    if (counter > 15) {
      Serial.println("Could not connect to WIFI. Aborting..");
      display.clearDisplay();
      printWifiDisconnected();
      break;
    }
  }


  if (WiFi.status() == WL_CONNECTED) {
      connectedToWifi = true;
  } else {
      connectedToWifi = false;
  }

}

void loop(void) {

  int rawvoltage= analogRead(0);
  float millivolts= (rawvoltage/1023.0) * 3300;
  temperature = (millivolts - 300) / 27;

  if (temperature < -10) {
       temperature = 0;
  }

  /* Publish temperature to broker each 30 times */
  if (mqtt_time_counter == 30) {
      if (connectedToWifi) {
          connectingToBroker = true;
          printDisplay();
          delay(300);
          MQTT_connect();
          printDisplay();
          delay(300);

          if (connectedToBroker) {
              Serial.print(F("\nSending foodprobe value "));
              if (! foodprobe.publish(temperature++)) {
                Serial.println(F("Failed"));
                publishedValue = false;
              } else {
                Serial.println(F("OK!"));
                publishedValue = true;
              }
          }
          printDisplay();
          delay(300);
      }

  } else if (mqtt_time_counter > 30) {
      connectedToBroker = false;
      connectingToBroker = false;
      publishedValue = false;
      mqtt_time_counter = 0;
  }
  mqtt_time_counter++;
  printDisplay();

  delay(1500);

}

void printDisplay() {

    display.clearDisplay();

    /* Print temperature */
    display.setTextSize(2);
    display.setCursor(0,18);
    String strTemperature = String(temperature, 1);
    display.println(strTemperature + "c");

    Serial.print(strTemperature);
    Serial.println(" celsius");

    /* WIFI/MQTT Status */
    if (publishedValue) {
        display.drawCircle(59, 4, 4, WHITE);
    } else if (connectedToBroker) {
        display.drawCircle(59, 4, 2, WHITE);
    } else if (connectingToBroker) {
        display.drawCircle(59, 4, 1, WHITE);
    } else {
        if (connectedToWifi) {
            display.drawCircle(59, 4, 4, WHITE);
            display.drawCircle(59, 4, 2, WHITE);
            display.drawCircle(59, 4, 1, WHITE);
        } else {
            display.drawCircle(59, 4, 4, WHITE);
        }
    }
    display.display();
}

void printWifi(int circle) {
    display.drawCircle(59, 4, circle, WHITE);
    delay(300);
    display.display();
}

void printWifiDisconnected() {
    display.clearDisplay();
    display.drawCircle(59, 4, 4, WHITE);
    display.display();
}

void MQTT_connect() {

  if (mqtt.connected()) {
    Serial.println("MQTT Already Connected");
    connectedToBroker = true;
    connectingToBroker = false;
    return;
  }

  int8_t connectionStatus = mqtt.connect();
  Serial.print("MQTT Connection status: ");
  Serial.println(connectionStatus);

  if (connectionStatus == 0 ) {
      Serial.println("MQTT Connected");
      connectedToBroker = true;
      connectingToBroker = false;
  } else {
      connectedToBroker = false;
      connectingToBroker = false;
  }
}
