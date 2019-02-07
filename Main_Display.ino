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
* Version 1.2:
*  Feb 4 2019: 1. Untested logic for display number
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
#include <NTPClient.h>
#include <WiFiUdp.h>
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

#define URL_UPDATE  "http://pkmcjr6nhmkq.cloud.wavemakeronline.com/repo_15s/services/fifteens/queryExecutor/queries/aUpdate"
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
unsigned int      currentappointmentid = 0;
unsigned int      previousappointmentID = 0;
unsigned int      nextappointmentID = 1;
unsigned int      serial_byte = 0;
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
HTTPClient http_put;

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;

//String appointmentDate;
String appointmentday;
String appointmentstarttime;

//String appointmentEndTime;
String appointmentendtime;

//String appointmentStatus;
//String appointmentNumber;

String      appointmenthour;
String      appointmentminute;
String      currentdate;
String      currentyear;
String      currentmonth;
String      currentday;
String      currenttime;
String      currenthour;
String      currentminute;
String      nextdate;
String      appointmentendhour;
String      appointmentendminute;
String url;
String URL_1 = "http://pkmcjr6nhmkq.cloud.wavemakeronline.com/repo_15s/services/fifteens/queryExecutor/queries/appointmentQuery?";



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
    delay(500);
    Serial.println(count);
  }
  WiriteDispVal(0);
}
/*
*------------------------------------------------------------------------------
* Function: ExtractHourMinute
* 
* Description: 
*           This function splits the time and extract hour and minutes
* Parameters: String time
*             int *hour
*             int *minute
* Return : void
*------------------------------------------------------------------------------
*/
void ExtractHourMinute(String time, String &hour,String &minute){
    int split = time.indexOf(":");
    hour = time.substring(0, split);
    minute = time.substring(split+1, time.indexOf(":",split+1));
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
  int i=0;

  int httpCode = http.GET();
  //Check the returning code                                                                  
  if (httpCode > 0) {
    
    /* Step 1. Get the Payload first buffer size limitiation needs to be taken care as we are allocating dynamic buffer*/
    const size_t bufferSize = JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(6) + JSON_OBJECT_SIZE(9) + 6*JSON_OBJECT_SIZE(10) + 1620;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    Payload = http.getString();
    //Serial.println(Payload); //Do not print the payload otherwise system would crash due to size

    /* Step 2. Get the time stamp to compare with fetched data, need to separate out hour, minute and second*/
    getTimeStamp();


    /* Step 3. Read number of record in a day */
    JsonObject& root = jsonBuffer.parseObject(Payload);
    String sappointmentNumber = root["numberOfElements"];
    no_of_appointment = sappointmentNumber.toInt();
    //Serial.println(no_of_appointment);
    /* Step 4. Parse record to find the time stamp , appointment starting time, end time and number*/
    for(i = 0 ; i < no_of_appointment; i++ ){
        /* Parse start date and start time*/
        String appointmentDate = root["content"][i]["appointmentDate"];
        // Extract date
        int splitT = appointmentDate.indexOf("T");
        appointmentday = appointmentDate.substring(0, splitT);
        //Serial.println(appointmentday);
        // Extract time
        appointmentstarttime = appointmentDate.substring(splitT+1, appointmentDate.length()-1);
        //Serial.println(appointmentstarttime);
        ExtractHourMinute(appointmentstarttime,appointmenthour,appointmentminute);

        /* Parse start date and end time*/
        String appointmentEndTime = root["content"][i]["appointmentEndTime"];
        // Extract date
        splitT = appointmentEndTime.indexOf("T");
        appointmentday = appointmentEndTime.substring(0, splitT);
        //Serial.println(appointmentday);
        // Extract time
        appointmentendtime = appointmentEndTime.substring(splitT+1, appointmentEndTime.length()-1);
        //Serial.println(appointmentendtime);

        ExtractHourMinute(appointmentendtime,appointmentendhour,appointmentendminute);
        
        /* Read appointment status and appointment number*/
        String appointmentStatus = root["content"][i]["appointmentStatus"];
        String appointmentNumber = root["content"][i]["appointmentNumber"];

        /* Step 5. If Time stamp is ( equal to current || (above current && minute is below end time)) && appointment is booked
               then display the appointment number of that slot  otherwise display --- or 000 */
        
        /* validate appointment date */
        if(appointmentday == currentdate){
            if( currenthour.toInt() == appointmenthour.toInt()){
                if (currentminute.toInt() >= appointmentminute.toInt()){
                    if(appointmentStatus == "current"){
                        current_apmt_count = appointmentNumber.toInt();
                        String appointmentID = root["content"][i]["appointmentId"];
                        currentappointmentid = appointmentID.toInt();
                        String previousapmtID = root["content"][i-1]["appointmentId"];
                        previousappointmentID = previousapmtID.toInt();
                        String nextapmtID = root["content"][i+1]["appointmentId"];
                        nextappointmentID = nextapmtID.toInt();
                        Serial.printf("appointmentNumber = %d\n", current_apmt_count);
                        WiriteDispVal(current_apmt_count);
                        break;
                    }
                }
            }
            else if ( currenthour.toInt() > appointmenthour.toInt() && currenthour.toInt() < appointmentendhour.toInt() && currenthour.toInt() != appointmentendhour.toInt()){
                if(appointmentStatus == "current"){
                    current_apmt_count = appointmentNumber.toInt();
                    String appointmentID = root["content"][i]["appointmentId"];
                    currentappointmentid = appointmentID.toInt();
                    String previousapmtID = root["content"][i-1]["appointmentId"];
                    previousappointmentID = previousapmtID.toInt();
                    String nextapmtID = root["content"][i+1]["appointmentId"];
                    nextappointmentID = nextapmtID.toInt();
                    Serial.printf("appointmentNumber = %d\n", current_apmt_count);
                    WiriteDispVal(current_apmt_count);
                    break;
                }
            }
            else if (currenthour.toInt() == appointmentendhour.toInt()){
                if (currentminute.toInt() < appointmentendminute.toInt()){
                    if(appointmentStatus == "current"){
                    current_apmt_count = appointmentNumber.toInt();
                    String appointmentID = root["content"][i]["appointmentId"];
                    currentappointmentid = appointmentID.toInt();
                    String previousapmtID = root["content"][i-1]["appointmentId"];
                    previousappointmentID = previousapmtID.toInt();
                    String nextapmtID = root["content"][i+1]["appointmentId"];
                    nextappointmentID = nextapmtID.toInt();
                    Serial.printf("appointmentNumber = %d\n", current_apmt_count);
                    WiriteDispVal(current_apmt_count);
                    break;
                    }
                }
            }
            else{
                WiriteDispVal(000);
                continue;
            }
            if (currenthour.toInt() == 0 && appointmenthour.toInt() == 23 && && appointmentendhour.toInt() == 0 ){
                 if (currentminute.toInt() < appointmentendminute.toInt()){
                    if(appointmentStatus == "current"){
                    current_apmt_count = appointmentNumber.toInt();
                    String appointmentID = root["content"][i]["appointmentId"];
                    currentappointmentid = appointmentID.toInt();
                    String previousapmtID = root["content"][i-1]["appointmentId"];
                    previousappointmentID = previousapmtID.toInt();
                    String nextapmtID = root["content"][i+1]["appointmentId"];
                    nextappointmentID = nextapmtID.toInt();
                    Serial.printf("appointmentNumber = %d\n", current_apmt_count);
                    WiriteDispVal(current_apmt_count);
                    break;
                    }
                }
            }
        }
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
    getTimeStamp();
    getnextdate(currentdate,nextdate);
    url = URL_1 + "date1=" + currentdate + "&" + "date2=" + nextdate;
    if (http.begin(url)) {
      http.setAuthorization("admin","admin");
      http_put.begin(URL_UPDATE);
#if DEBUG
      Serial.println(url);
      http.setAuthorization("admin","admin");
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
    else  if (!http.begin(url)) {
#if DEBUG
      Serial.printf("URL server connection lost. Reconnect.\n");
#endif
      http.end();
      ConnectionState = IOT_CONNECT_TO_URL;
    }
    else {
      
      /*Call the log for handling data from server in every 5 sec interval timer close the timer and start again timer in every 5 sec*/
      //Serial.printf("IoT_URL_ServerHandler called\n");
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

/***********************************************************************************************
 * Function: getnextdate
 * 
 * Description: 
 *           This function get the next date of current date.
 *           
 * Parameters: String &currentdate
 *             String &nextdate
 * Return : void
 ***********************************************************************************************/
void getnextdate(String &currentdate,String &nextdate)
{
    static char daytab[2][13]=
    {
        {0,31,28,31,30,31,30,31,31,30,31,30,31},
        {0,31,29,31,30,31,30,31,31,30,31,30,31}
    };

    int splitY = currentdate.indexOf("-");
    currentyear = currentdate.substring(0, splitY);
    int splitM = currentdate.indexOf("-",splitY+1);
    currentmonth = currentdate.substring(splitY+1, splitM);
    currentday = currentdate.substring(splitM+1, currentdate.length());
    int year = currentyear.toInt();
    int month = currentmonth.toInt();
    int day = currentday.toInt();
//Serial.println(year);
//Serial.println(month);
//Serial.println(day);

    int leap = (year % 4 == 0);
    day++; //increment one day
    if (year % 100 == 0 && year % 400 !=0)
        leap = 0;
    int daytotal = daytab[leap][month];
    if(day > daytotal && month == 12)
    {
        year++;
        day = 1;
        month = 1;
    }
    else if ( day > daytotal && month < 12)
    {
        day = 1;
        month++;
    }
    //create the date string.
    if(month < 10)
      nextdate = String(year) + "-0" + String(month);
    else
      nextdate = String(year) + "-" + String(month);
    if(day < 10)
      nextdate = nextdate + "-0" + String(day);
    else
      nextdate = nextdate + "-" + String(day);
}
/***********************************************************************************************
 * Function: getTimeStamp
 * 
 * Description: 
 *           This function get the NTPclient data display.
 *           
 * Parameters: void
 * Return : void
 ***********************************************************************************************/
// Function to get date and time from NTPClient
void getTimeStamp() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();
  //Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  currentdate = formattedDate.substring(0, splitT);
  //Serial.println(currentdate);
  // Extract time
  currenttime = formattedDate.substring(splitT+1, formattedDate.length()-1);
  //Serial.println(currenttime);

  ExtractHourMinute(currenttime,currenthour,currentminute);

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

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +5.30 = ((3600 * 5)+ (30*60))
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(((3600 * 5)+ (30*60)));
  delay(200);
  getTimeStamp();
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
  if (Serial.available() > 0) {
      // read the incoming byte:
       serial_byte = Serial.read();
  }
      
  if(43 == serial_byte || 0 == ReadkeyUp())
  {
    current_apmt_count++;
    DynamicJsonBuffer  jsonBuffer(50);
    JsonObject& root2 = jsonBuffer.createObject();
    root2["appointmentID"] =   nextappointmentID;
    root2["date1"] = currentdate;
    root2["date2"] = nextdate;
    String postData = "";
    root2.printTo(postData);
    http_put.PUT(postData);
    WiriteDispVal(current_apmt_count);
    //add code to push to server to increment appointment count
    Serial.println(String(serial_byte));
    Serial.println(postData);
    serial_byte = 0; 
    delay(500);      
  }
  
  if(45 == serial_byte || 0 == ReadkeyDn())
  {
    current_apmt_count--;
    DynamicJsonBuffer  jsonBuffer(50);
    JsonObject& root1 = jsonBuffer.createObject();
    root1["appointmentID"] = previousappointmentID;
    root1["date1"] = currentdate;
    root1["date2"] = nextdate;
    String postData = "";
    root1.printTo(postData);
    http_put.PUT(postData);
    WiriteDispVal(current_apmt_count);  
    //add code to push to server to increment appointment count 
    Serial.println(String(serial_byte)); 
    Serial.println(postData);
    serial_byte = 0;  
    delay(500);
  }      
    
}
