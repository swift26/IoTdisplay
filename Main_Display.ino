/* *********************************************************************************************
 * Project:     IoT based display 
 * 
 * Description: This project is based on ESP8266 module. It's main operation is to fetch the 
 *              Json data from prescribed link and display the content on display unit.
 * Basic consideration:
 *              1. SSID and PASSWORD of wifi modem would be hardcoded in code.
 *              2. Link for server would be hard coded and appended with MAC address of the 
 *                 module.
 *              3. If battery mode then the display should blink(blank out and display) in every 
 *                 30 sec. (Is the charger detection mechanism is been implemented on HW??)
 *              4. Between number change play a sound.
 *              5. Button would be provided for start stop increment and decrement.
 * Flow :
 *        Boot Flow:
 *              Step 1: Boot with display led all on.
 *              Step 2: Connect the Wifi modem
 *                        a. If no connection available then display round scrolling led
 *                        b. If connected try to connect to provided URL
 *                            a. If URL available
 *                                Try to get the number to display.
 *                                    a. if no data display "---"
 *                                    b. If available then display number.
 *                            b. If not available show "!" mark.
 *              Step 3. If connection not available then try to retry and fall back to step 2.
 * Version 0.1: 
 *        Nov 2 2018: Added all the basic functionality
 *                      1. Wifi frame work for connection and re connection.
 *                      2. OTA upgrade over Wifi.
 *                      3. Display elements on Serial port.(until LED module available)
 * 
 ***********************************************************************************************/

/***********************************************************************************************
 * All include fies
 ***********************************************************************************************/
#include <ESP8266WiFi.h> //For Wifi enable
#include <SimpleTimer.h> //For Timer events
#include <ESP8266mDNS.h> //For OTA upgrade over wifi
#include <WiFiUdp.h>     //For OTA upgrade over wifi
#include <ArduinoOTA.h>  //For OTA upgrade over wifi
#include <ESP8266HTTPClient.h> //Httpclient
#include <ArduinoJson.h> //For JSON file formats
#include <WiFiClient.h>  //for Wifi TCP client
/***********************************************************************************************/
/***********************************************************************************************
 * Macro definations
 ***********************************************************************************************/
#define DEBUG 1
#define URL "http://jsonplaceholder.typicode.com/users/1"
/***********************************************************************************************/
/***********************************************************************************************
 * Global variables defines
 ***********************************************************************************************/
SimpleTimer IoTConnectionHandlerTimer;

// WiFi credentials.
// Set password to "" for open networks.
char wifi_ssid[] = "Airtel-B310-90C5";
char wifi_pass[] = "A445F97458F";

typedef enum {
  IOT_CONNECT_TO_WIFI,
  IOT_AWAIT_WIFI_CONNECTION,
  IOT_CONNECT_TO_URL,
  IOT_AWAIT_URL_CONNECTION,
  IOT_MAINTAIN_CONNECTIONS,
  IOT_AWAIT_DISCONNECT
} IOT_CONNECTION_STATE;

IOT_CONNECTION_STATE ConnectionState;
uint8_t ConnectionCounter;
HTTPClient http;  //Object of class HTTPClient
WiFiClient client;
/***********************************************************************************************/
/***********************************************************************************************
 * Function: IoT_URL_ServerHandler
 * 
 * Description: 
 *           This function handles the server data retrival
 *           Can we use Websocket in place of always asking to server data?? It will use two way           
 *           communication and reduce the overhead of continous get call??
 * Parameters: void
 * Return : void
 ***********************************************************************************************/
 void IoT_URL_ServerHandler(void){
  int httpCode = http.GET();
  //Check the returning code                                                                  
  if (httpCode > 0) {
    // Parsing
    const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root = jsonBuffer.parseObject(http.getString());
    // Parameters
    int id = root["id"];
    const char* name = root["name"];
    const char* username = root["username"];
    const char* email = root["email"];
#if DEBUG
    // Output to serial monitor
    Serial.print("Name:");
    Serial.println(name);
    Serial.print("Username:");
    Serial.println(username);
    Serial.print("Email:"); 
    Serial.println(email);
#endif
  }
 }

/***********************************************************************************************/
/***********************************************************************************************
 * Function: IoT_ConnectionHandler
 * 
 * Description: 
 *           This function is used for handling the connection states.
 *           It handles below two steps of Project
 *           Step 2: Connect the Wifi modem
 *                        a. If no connection available then display round scrolling led
 *                        b. If connected try to connect to provided URL
 *                            a. If URL available
 *                                Try to get the number to display.
 *                                    a. if no data display "---"
 *                                    b. If available then display number.
 *                            b. If not available show "!" mark.
 *           Step 3. If connection not available then try to retry and fall back to step 2.
 *           
 * Parameters: void
 * Return : void
 ***********************************************************************************************/
void IoT_ConnectionHandler(void) {
  switch (ConnectionState) {
  case IOT_CONNECT_TO_WIFI:
#if DEBUG
    Serial.printf("Connecting to %s.\n", wifi_ssid);
#endif
    WiFi.begin(wifi_ssid, wifi_pass);
    ConnectionState = IOT_AWAIT_WIFI_CONNECTION;
    ConnectionCounter = 0;
    break;

  case IOT_AWAIT_WIFI_CONNECTION:
    if (WiFi.status() == WL_CONNECTED) {
#if DEBUG
      Serial.printf("Connected to %s\n", wifi_ssid);
#endif
      ConnectionState = IOT_CONNECT_TO_URL;
    }
    else if (++ConnectionCounter == 50) {
#if DEBUG
      Serial.printf("Unable to connect to %s. Retry connection.\n", wifi_ssid);
#endif
      WiFi.disconnect();
      ConnectionState = IOT_AWAIT_DISCONNECT;
      ConnectionCounter = 0;
    }
    break;

  case IOT_CONNECT_TO_URL:
#if DEBUG
    Serial.printf("Attempt to connect to URL server.\n");
#endif
    // Make a HTTP request:
    
    ConnectionState = IOT_AWAIT_URL_CONNECTION;
    ConnectionCounter = 0;
    break;

  case IOT_AWAIT_URL_CONNECTION:
    if (http.begin(URL)) {
#if DEBUG
      Serial.printf("Connected to URL server.\n");
#endif
      ConnectionState = IOT_MAINTAIN_CONNECTIONS;
    }
    else if (++ConnectionCounter == 50) {
#if DEBUG
      Serial.printf("Unable to connect to URL server. Retry connection.\n");
#endif
      /*Both wifi and url not available disconnect and try reconnecting after retry*/
      http.end();
      WiFi.disconnect();
      ConnectionState = IOT_AWAIT_DISCONNECT;
      ConnectionCounter = 0;
    }
    break;

  case IOT_MAINTAIN_CONNECTIONS:
    if (WiFi.status() != WL_CONNECTED) {
#if DEBUG
      Serial.printf("Wifi connection lost. Reconnect.\n");
#endif
      /*Both wifi and url not available disconnect and try reconnecting after retry*/
      http.end();
      WiFi.disconnect();
      ConnectionState = IOT_AWAIT_DISCONNECT;
      ConnectionCounter = 0;
    }
    else  if (!http.begin(URL)) {
#if DEBUG
      Serial.printf("URL server connection lost. Reconnect.\n");
#endif
      http.end();
      ConnectionState = IOT_CONNECT_TO_URL;
    }
    else {
      /*Call the log for handling data from server*/
      IoT_URL_ServerHandler();
    }
    break;

  case IOT_AWAIT_DISCONNECT:
    if (++ConnectionCounter == 10) {
      ConnectionState = IOT_CONNECT_TO_WIFI;
    }
    break;
  }
}

/***********************************************************************************************/
/***********************************************************************************************
 * Function: setup
 * 
 * Description: 
 *           This function is used for handling initial setup of project.
 *           1. Serial setup for Debug
 *           2. Setup for OTA upgrade
 *           3. Setup for LED display
 *           4. Setup Connection handler for Wifi and URL
 *           
 * Parameters: void
 * Return : void
 ***********************************************************************************************/
 
void setup()
{
  Serial.begin(9600);
  IoTConnectionHandlerTimer.setInterval(100, IoT_ConnectionHandler);
  ConnectionState = IOT_CONNECT_TO_WIFI;

  String hostname("IoTDisplay-OTA-");
  hostname += String(ESP.getChipId(), HEX);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
}
/***********************************************************************************************/
/***********************************************************************************************
 * Function: loop
 * 
 * Description: 
 *           This function handles the logic of OTA upgrade and run the handler timer.
 *           
 * Parameters: void
 * Return : void
 ***********************************************************************************************/
void loop()
{
  ArduinoOTA.handle();
  IoTConnectionHandlerTimer.run();
}
