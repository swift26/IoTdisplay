# IoTdisplay
It is an led display project based on ESP8266 


 Project:   
 
               IoT based display 
  
 Description: 
 
               This project is based on ESP8266 module. It's main operation is to fetch the 
               Json data from prescribed link and display the content on display unit.
               
 Basic consideration:
 
               1. SSID and PASSWORD of wifi modem would be hardcoded in code.
               2. Link for server would be hard coded and appended with MAC address of the 
                  module.
               3. If battery mode then the display should blink(blank out and display) in every 
                  30 sec. (Is the charger detection mechanism is been implemented on HW??)
               4. Between number change play a sound.
               5. Button would be provided for start stop increment and decrement.
               
 Flow :
 
         Boot Flow:
         
               Step 1: Boot with display led all on.
               Step 2: Connect the Wifi modem
                         a. If no connection available then display round scrolling led
                         b. If connected try to connect to provided URL
                             a. If URL available
                                 Try to get the number to display.
                                     a. if no data display "---"
                                     b. If available then display number.
                             b. If not available show "!" mark.
               Step 3. If connection not available then try to retry and fall back to step 2.
               
 Version 0.1: 
 
         Nov 2 2018: Added all the basic functionality
                       1. Wifi frame work for connection and re connection.
                       2. OTA upgrade over Wifi.
                       3. Display elements on Serial port.(until LED module available)
