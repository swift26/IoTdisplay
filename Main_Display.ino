/*
*------------------------------------------------------------------------------
*                           _             _
*   ___  __ _ _ __ ___  ___| | ___   __ _(_)_  __
*  / __|/ _` | '_ ` _ \/ __| |/ _ \ / _` | \ \/ /
*  \__ \ (_| | | | | | \__ \ | (_) | (_| | |>  <
*  |___/\__,_|_| |_| |_|___/_|\___/ \__, |_/_/\_\
*                                   |___/
*
* Copyright (c) 2013, All Right Reserved, www.samslogix.com
*
* The copyright notice above does not evidence any
* actual or intended publication of such source code.
*
*
*------------------------------------------------------------------------------
*/

/*
*------------------------------------------------------------------------------
*   __
*  |  |                      _
*  |  |     __   __ __   __ | |_  
*  |  |___  \ \_/ / \ \_/ / |  _\
*  | ____ |  \   /   \___/  | |_
*           |___/           |___\   
*
* Copyright (c) 2019, All Right Reserved, www.lyvtheory.com
*
* The copyright notice above does not evidence any
* actual or intended publication of such source code.
*
*
*------------------------------------------------------------------------------
*/

/*  
*  ---------------------------------------------------------------------------
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
*                   Commited by: TK
* Version 1.0:
*        Dec 6 2018: Added display module code
*                      1. Pin details.
*                      2. Keypress code
*                      3. Display buffer and code for display.
*                   Commited by: SAM
* 
* Version 1.1:
*        Feb 4 2019: 1. removed time server/ added NTP server.
*                    2. Added logic for display number whose current status is active.
*----------------------------------------------------------------------------------------------
*/

/*
 *----------------------------------------------------------------------------------------------
 * All include fies
 *----------------------------------------------------------------------------------------------
*/
#include <ESP8266WiFi.h> //For Wifi enable
#include <SimpleTimer.h> //For Timer events
#include <ESP8266mDNS.h> //For OTA upgrade over wifi
#include <WiFiUdp.h>     //For OTA upgrade over wifi
#include <ArduinoOTA.h>  //For OTA upgrade over wifi
#include <ESP8266HTTPClient.h> //Httpclient
#include <ArduinoJson.h> //For JSON file formats
#include <WiFiClient.h>  //for Wifi TCP client
#include <TimeLib.h>
/*
*------------------------------------------------------------------------------
* Private Defines
*------------------------------------------------------------------------------
*/

/*
 *----------------------------------------------------------------------------------------------
 * Macro definations
 *----------------------------------------------------------------------------------------------
*/
#define DEBUG 1
#define URL "http://pkmcjr6nhmkq.cloud.wavemakeronline.com/repo_15s/services/fifteens/queryExecutor/queries/appointmentQuery?date1=2018-12-23&date2=2019-01-23"

#define  MAX_DATA_LEN               (6)
#define  HB_INTERVAL                (1000)   /* 1000msec ON/OFF .*/

/* Port allocation. */
#define  HB_LED_OUTPUT              (5)  // D1
#define  BUZ_DRV_OUTPUT             (4)  // D2

/* Display interface ports. */
#define  LT_CLK                     (14)  // D5
#define  SER_DIN                    (12)  // D6
#define  SH_CLK                     (13)  // D7
#define  OP_ENB                     (16)  // D0

/* Key input ports. */
#define  KEY_UP                     (0)   // TX
#define  KEY_DN                     (2)   // D8

/*
*------------------------------------------------------------------------------
* Private Data Types
*------------------------------------------------------------------------------
*/

/*
*------------------------------------------------------------------------------
* Public Variables
*------------------------------------------------------------------------------
*/

/*
*------------------------------------------------------------------------------
* Private Variables (static)
*------------------------------------------------------------------------------
*/
unsigned int      frequency = 1000;
unsigned int      duration = 1000;
unsigned int      count = 0;
unsigned char     DisplayBuff[MAX_DATA_LEN];
unsigned long     HBInterval = 0;
unsigned long     prevMill = 0;
unsigned long     currMill = 0;
unsigned char     buttonStateInc;              // the current reading from the input pin
unsigned char     lastbuttonStateInc = LOW;    // the previous reading from the input pin
unsigned long     lastDebounceTimeInc = 0;     // the last time the output pin was toggled
unsigned char     buttonStateDec;              // the current reading from the input pin
unsigned char     lastbuttonStateDec = LOW;    // the previous reading from the input pin
unsigned long     lastDebounceTimeDec = 0;     // the last time the output pin was toggled
unsigned long     debounceDelay = 50;       // the debounce time; increase if the output flickers
unsigned int      current_apmt_count = 0;

unsigned char     CHAR_CODE[]
= {
    0b01111011,0b01101111,    //0
    0b00100100,0b10010010,    //1
    0b01110011,0b11100111,    //2
    0b01111001,0b11100111,    //3  
    0b01001001,0b11101101,    //4
    0b01111001,0b11001111,    //5
    0b01111011,0b11001111,    //6
    0b01001001,0b00100111,    //7
    0b01111011,0b11101111,    //8
    0b01111001,0b11101111,    //9
  };

SimpleTimer IoTConnectionHandlerTimer;

// NTP Servers:
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 5; //For india + 5:30

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

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

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

/*
*------------------------------------------------------------------------------
*  unsigned char ReadkeyUp(void)
*
*  Summary: Read increment key
*
*  Input  : None
*
*  Output : 0 - if key pressed. 1 - if no key pressed.
*
*------------------------------------------------------------------------------
*/
unsigned char ReadkeyUp(void)
{
  /* read the state of the pushbutton value */
  return digitalRead(KEY_UP);
}

/*
*------------------------------------------------------------------------------
*  unsigned char ReadkeyUp(void)
*
*  Summary: Read decrement key
*
*  Input  : None
*
*  Output : 0 - if key pressed. 1 - if no key pressed.
*
*------------------------------------------------------------------------------
*/
unsigned char ReadkeyDn(void)
{
  /* read the state of the pushbutton value */
  return digitalRead(KEY_DN);
}

/*
*------------------------------------------------------------------------------
*  void WiriteDispVal(unsigned int)
*
*  Summary: Write value to display 
*
*  Input  : unsigned int val  ( valure range from 0 - 999)
*
*  Output : None
*
*------------------------------------------------------------------------------
*/
void WiriteDispVal(unsigned int val)
{
    unsigned int Qut, Rem;
    unsigned int dig2,dig1, dig0;
    if(val < 1000)
    {
      Rem = val % 10;
      dig0 = Rem;           // digit 0
      Qut = val / 10;
    
      Rem = Qut % 10;
      dig1 = Rem;           // digit 1
      Qut = Qut / 10;
    
      Rem = Qut % 10;
      dig2 = Rem;           // digit 2

      DisplayBuff[5] = CHAR_CODE[(dig2 * 2) + 1];  // digit 2
      DisplayBuff[4] = CHAR_CODE[(dig2 * 2) + 0];
      DisplayBuff[3] = CHAR_CODE[(dig1 * 2) + 1];  // digit 1
      DisplayBuff[2] = CHAR_CODE[(dig1 * 2) + 0];
      DisplayBuff[1] = CHAR_CODE[(dig0 * 2) + 1];  // digit 0
      DisplayBuff[0] = CHAR_CODE[(dig0 * 2) + 0];   
      UpdateDisplay(DisplayBuff);      
    }
}
/*
*------------------------------------------------------------------------------
*  void UpdateDisplay(unsigned char *data)
*
*  Summary: Updated 32 bit Level Out put data.
*
*  Input  : unsigned char* data to send
*
*  Output : None
*
*------------------------------------------------------------------------------
*/
void UpdateDisplay(unsigned char *data)
{
    unsigned char count = 0, jcount = 0;

    /* Disable o/p. */
    digitalWrite(OP_ENB, 1);      
    /*Bring the Latch low */
    digitalWrite(LT_CLK, 0);       
    /*Shift data bits out one at a time */
    for (count = 0; count < 48; count++)
    { 
      if((8 == count) || (16 == count) || (24 == count) || (32 == count) || (40 == count) || (40 == count))
      {
        jcount = jcount + 1;
      }

      /*Set the data bit 0 or 1 */
      if ((data[jcount] & 0x80) == 0x80)
      {
        digitalWrite(SER_DIN, 1);          
      }
      else
      {
        digitalWrite(SER_DIN, 0);           
      }
    
      /* Clock the bit into the shift register. */
      digitalWrite(SH_CLK, 0);        
      digitalWrite(SH_CLK, 0);  
      //delay(1);
      digitalWrite(SH_CLK, 1);        
      digitalWrite(SH_CLK, 1);  
      data[jcount] <<= 1;
    }
    
    /*Latch the data we just sent */
    digitalWrite(LT_CLK, 1);    
    /* Enable output.*/
    digitalWrite(OP_ENB, 0);
}

/*
*------------------------------------------------------------------------------
* void updateHeartBeat(void)
*
* Summary : Function is called frequently with respect or pre defined 
*                 intervals to show the board alive status. 
*
* Input   : None
*
* Output  : None
*
*------------------------------------------------------------------------------
*/
void updateHeartBeat(void)
{
    static bool state = 0;
    if((millis() - HBInterval) > HB_INTERVAL)
    {
      HBInterval = millis();
      state = (state)?0:1;
      digitalWrite(HB_LED_OUTPUT, state);
      //digitalWrite(BUZ_DRV_OUTPUT, state);      
    }
    /*------------------------------------END---------------------------------------*/
}

/*
*------------------------------------------------------------------------------
* Function: LedInitialWalk
* 
* Description: 
*           This function initialize the led and handles LED display walk.
* Parameters: void
* Return : void
*
*------------------------------------------------------------------------------
*/
 void LedInitialWalk(void){
  int count=0;
    
  /* initialize the HB LED driver */
  pinMode(HB_LED_OUTPUT, OUTPUT);
  digitalWrite(HB_LED_OUTPUT, HIGH);
  
  /* initialize the buzzer drive. */
  pinMode(BUZ_DRV_OUTPUT, OUTPUT);
  digitalWrite(BUZ_DRV_OUTPUT, LOW);
  
  /* initialise display ports. */
  pinMode(SH_CLK,   OUTPUT);
  pinMode(LT_CLK,   OUTPUT);
  pinMode(SER_DIN,  OUTPUT);
  pinMode(OP_ENB,   OUTPUT);
  //digitalWrite(SH_CLK,  LOW);
  digitalWrite(LT_CLK,  LOW);
  digitalWrite(SER_DIN, LOW);
  digitalWrite(OP_ENB,  LOW);

  /* initialise key input ports. */
  pinMode(KEY_UP, INPUT);
  pinMode(KEY_DN, INPUT);
  
  prevMill = millis();
  HBInterval = 0;

  memset(DisplayBuff, 0x00, sizeof(DisplayBuff));
  UpdateDisplay(DisplayBuff);  
  WiriteDispVal(0);
    
  for(;count<1000; count=count+111){
    WiriteDispVal(count);
    delay(100);
    Serial.println(count);
  }
  WiriteDispVal(0);
}

/*
*------------------------------------------------------------------------------
* Function: IoT_URL_ServerHandler
* 
* Description: 
*           This function handles the server data retrival
*           Can we use Websocket in place of always asking to server data?? It will use two way           
*           communication and reduce the overhead of continous get call??
* Parameters: void
* Return : void
*
*------------------------------------------------------------------------------
*/
 void IoT_URL_ServerHandler(void){
  String Payload;
  int no_of_appointment = 0;
  String current_date_time;
  static int i=0;

  int httpCode = http.GET();
  //Check the returning code                                                                  
  if (httpCode > 0) {
    // Parsing
    const size_t bufferSize = JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(6) + JSON_OBJECT_SIZE(9) + 6*JSON_OBJECT_SIZE(10) + 1620;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    Payload = http.getString();
    //Serial.println(Payload); 
    
    JsonObject& root = jsonBuffer.parseObject(Payload);
    String sappointmentNumber = root["size"];
    no_of_appointment = sappointmentNumber.toInt();

    //Find first valid appointment first with current time.
    current_date_time = String(year())+"-"+String(month())+"-"+String(day())+"T"+String(hour())+":"+String(minute())+":"+String(second());
    Serial.print(current_date_time);
    Serial.println();

    
    String appointmentNumber = root["content"][i]["appointmentNumber"];
    String appointmentDate = root["content"][i]["appointmentDate"];
    current_apmt_count = appointmentNumber.toInt();
    
#if DEBUG
    // Output to serial monitor
    Serial.print("appointmentNumber:");
    Serial.println(appointmentNumber);
    Serial.print("appointmentDate:");
    Serial.println(appointmentDate);
#endif
    //Update the display number with appointment number which match to current time slot
    // put if condition here
    WiriteDispVal(current_apmt_count);
    i++;
    if(i >= no_of_appointment)
      i = 0;
  }
 }


/*
*-------------------------------------------------------------------------------
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
*
*------------------------------------------------------------------------------
*/
void IoT_ConnectionHandler(void) {
  switch (ConnectionState) {
  case IOT_CONNECT_TO_WIFI:
#if DEBUG
    Serial.printf("Connecting to %s.\n", wifi_ssid);
#endif
	  WiFi.mode(WIFI_STA);
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
      http.setAuthorization("admin","admin");
#if DEBUG
      Serial.printf("Connected to URL server.\n");
#endif
      ConnectionState = IOT_MAINTAIN_CONNECTIONS;
    }
    else if (++ConnectionCounter == 150) {
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
      
      /*Call the log for handling data from server in every 5 sec interval timer close the timer and start again timer in every 5 sec*/
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

/*
 * -------------------------------------------------------------------------------------------
 * 
 * NTP code here
 * 
 * -------------------------------------------------------------------------------------------
 */
time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR + (30 * 60) ; //India zone is 5.30 minutes
    }
  }
  
#if DEBUG
  Serial.println("No NTP Response :-( Reconnect.\n");
#endif
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
 

/*
*---------------------------------------------------------------------------------------------- 
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
*
*------------------------------------------------------------------------------
*/
 
void setup()
{
  int count=0;
  Serial.begin(9600);
  LedInitialWalk();
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
    WiriteDispVal(count++); //While trying to connect display 0 1 2 3 4 5 etc .. needs to be replaced with moving led pattern.
  }
  count = 0;
  ConnectionState = IOT_AWAIT_WIFI_CONNECTION;
  String hostname("IoTDisplay-OTA-");
  hostname += String(ESP.getChipId(), HEX);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.begin();
  IoTConnectionHandlerTimer.setInterval(2000, IoT_ConnectionHandler);
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(200);
  delay(500);
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
  updateHeartBeat();
  if(0 == ReadkeyUp())
  {
    current_apmt_count++;
    WiriteDispVal(current_apmt_count);
    //add code to push to server to increment appointment count
    delay(400);      
  }
  
  if(0 == ReadkeyDn())
  {
    current_apmt_count--;
    WiriteDispVal(current_apmt_count);  
    //add code to push to server to increment appointment count    
    delay(400);
  }    
}
