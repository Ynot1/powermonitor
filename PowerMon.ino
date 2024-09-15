// Power Monitor

//#Correct board type for the esp-01 is "generic esp8266"
// boards url http://arduino.esp8266.com/stable/package_esp8266com_index.json
//# Programmer AVRISP mk II
#define USING_AXTLS
#include <time.h>

//#include <ESP8266WiFi.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureAxTLS.h>
#include <EEPROM.h>

// EEPROM Space the esp-01 has 512KiB or 1MiB of flash memory
// We need to save 31 * 2 bytes of data for norml and peak mins per 31 calaender days
// lets start at index 100
// Index 101 = day 1 normal, 102 day 2 normal
// Index 201 = day 1 peak, 102 day 2 peak, 231 day 31 peak (just fits in a byte)
int EEPROMIndex = 101;
byte TestByte100;
byte TestByte101;
byte TestByte102;
int Heatime = 0;

//
using namespace axTLS;
#define STACODE "amzn1.ask.account.AFHAAQFF43O6XBUSKHQ537XX66QPLCHNUASFIJV6DS4R3Q2NFCMQDCXQLNKFRTM2RQ7SDI6U5RNR6IRXPUYDR4GEYKDJBV6XZKWOT4ZLLG3XRTAJBFLQHEGQZJDKX5YGTWWMJJ64XTHX7ASX4UZ6GFNJQICQI3UTIOQJUQCXSGXTFE4MULS6S7UJVRXAXVDPWQXICV6NKFB4QHQ" // see www.virtualbuttons.com for more information

//Version History

//ALL new & current references here
String eightspaces = "&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; ";
String fourspaces = "&nbsp; &nbsp; &nbsp; &nbsp; ";
String threespaces = "&nbsp; &nbsp; &nbsp; ";
String twospaces = "&nbsp; &nbsp; ";
/*
  const char* VirtualButtonsHost = "api.virtualbuttons.com";
  const int httpsPort = 443;
  // Root certificate used by VirtualButtons.com is Defined in the "VirtualButtonsCACert" tab.
  extern const unsigned char caCert[] PROGMEM;
  extern const unsigned int caCertLen;
  const String access_code = STACODE;
*/

bool HeatingState = false; // Current State of the water heater
bool PrevHeatingState;
bool SetTimeWasSuccesfull = 0; // not implemented. couldnt figure out how to test if time was valid or not

WiFiServer server(80); // internally facing web page is http.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wdeprecated-declarations"
WiFiClientSecure client;
#pragma GCC diagnostic pop

String header; // Variable to store the HTTP request for the local webserver

const long timeoutTime = 2000; // Define web page timeout time in milliseconds (example: 2000ms = 2s)


boolean wifiConnected = false;
bool DebugMessages = true;

String       RunTimessid = "A_Virtual_Information";
String       RunTimepassword = "BananaRock";

//String RunTimessid = "SmartStuff";
//String RunTimepassword = "Password123456";

// the esp01 that grants ip address .96 is the one to use in prod. the one one that gets .128/.129 gives false results (lookslike mains sensor is flackey)


//This already needs tiding up

// Assign variables to GPIO pins
const int LedPin = 1;  // GPIO2 pin - on board LED of some of the ESP-01 variants.

const int HeatingPin = 3;  // RXD is the hot water heater sensor input


/*  WIRING DETAILS
    CPU TXD connects the external diagnotic port for debug log.)
    CPU RXD connects to the main sensor across the element

    LED connected connected to GPIO1 (TXD) on the newer ESP-01S baord


*/
boolean connectWifi();

int WDResponcesArray[25] ; // only 23 visible, 24 used as zero placeholder
const int EventLogArraySize = 100;
String EventLogArray[EventLogArraySize] ; // 10 events each containing Date [0], Time[1], FailedDeviceID [2]. last 3 entries (30,31,32) are headers
String EventLogEntry ;
const int DailyReadingsArraySize = 210;
float DailyReadingsArray[DailyReadingsArraySize] ; //
// first row [0-5] not used. Could be used for variables later?
// 1st day [6-11] each containing Day of month [6],  normal power mins [7], peak power mins [8], normal cost [9], peak cost[10], , total cost[10]
// 2nd day [12-17] each containing Day of month [12], normal power mins[13], peak power mins [14], normal cost [15], peak cost [16], spare [17]

// 15th day [90-95] each containing Day of month [90], normal power mins [91], peak power mins [92], spare [93], spare [94]
// 31st day [186-191 ] Day of month [186], day of week[187], normal power mins [188], peak power mins [189], spare [190], spare [191]
// Average results [192 - 197] spare [192], spare [193], normal pwr total [194], peak pwr total [195], normal costs? [196], peak costs [197]
// Current Month results [198 - 203] spare [198], spare [199], normal pwr total [200], peak pwr total [201], normal costs? [202], peak costs [203]
// Previous Month [204 - ]  spare [204], spare [205], spare [206], spare [207], spare [208], spare [209]
// [210] not used
String LastRebootDate ;
String LastRebootTime ;
float UpTimeDays = 0;

//byte WDResponcesArrayIndex = 0;

int ElementPower = 3;// Kilowatts
float PeakPowerCost = 27.468; // c/kwhr // 82.404 c / kwhr at 3kw // 1.377 c/m at 3kw
float OffPeakPowerCost = 17.767; // 53.301 c/ hr // 0.88 c/

// kwh = 3kw x (mins / 60)
// costs = kw * powercostperhr
// costs in cents = ((ElementPower * (mins /60) * OffPeakPowerCost))

float NormalCostPerKwh = ElementPower * OffPeakPowerCost;
float PeakCostPerKwh = ElementPower * PeakPowerCost;
float NormalCostPerMin = (ElementPower * OffPeakPowerCost) / 60;
float PeakCostPerMin = (ElementPower * PeakPowerCost) / 60;

// off peak times 1100 - 1700 & 2100 - 0700
// peak times 0700 - 1100 & 1700 - 2100

int MorningPeakStart = 420;
int MorningPeakStop =  660;
int EveningPeakStart = 1020;
int EveningPeakStop = 1260;
bool PeakTime = false; // boolean true is PeakTime

int MinutesIntoDay = 0; // minutes so far into the day (gets to 1440 by midnight)
int DayCounter = 0;

int PageMode = 0; // 0 = Status, 1 = EventLog, 2 = unused, 3= DailyReadings

String HeatingStateString = "Not Heating";

String NormalColor = "ForestGreen";
String FaultColor = "FireBrick";
String PeakColor = "DarkOrange";

float DSTOffset = 3600; // it might go negative at runtime...
float PrevDSTOffset = 3600; // it might go negative at runtime...

//bool DSTState = false;// asuumes normal DST time at boot
bool FirstLoop = true;

//int StackedIndex = 0;
byte WebPageMode = 2; // 1 = Setup, 2 = Runtime

unsigned long currentTime = millis();

unsigned long previousTime = 0;
unsigned long previousMillis = 0;

//byte WDResponcesArrayFFSIndex=0;
byte FirstFreeSlot = 0;
byte WDEntry = 0;
byte ProxyRequestID = 0;
String ProxyRequestText ;
String WDEntryText ;
byte WDEntrySlot = 0;
byte WDCall = 0; // If this non zero, then a Status failure has occured. WDCall contains the ID of the failed device
byte WDLoopTimer = 0;
byte WDLoopTimerThreshold = 50; // seconds - was 10seconds, got random triggers from garagedoor
// Set your Static IP address
IPAddress local_IP(192, 168, 1, 96); // fixed IP address for the PowerMon collector (this code)
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);


bool TestHeating = false;
bool PrevTestHeating = false;
bool Debug1 = false;
bool PrevDebug1 = false;
bool Debug2 = false;
bool PrevDebug2 = false;
bool Debug3 = false;
bool PrevDebug3 = false;
byte EventLogArrayIndex = 0;
byte DailyReadingsArrayIndex = 0;

byte currentseconds = 0 ;
byte currentminutes = 55;
byte currenthours = 12 ;
long currentday = 0 ;
byte currentmonth = 0 ;
byte currentmonthPrevious = 0 ;
byte currentdayofweek = 8;
/*
  The evolving requirements and bug list section.



  add previous month data to eeprom (need to store realnumbers not integers)





*/


byte RectHeight = 110;
byte RectWidth = 10;
String RectColor = "Black";

String CompileDate = "16th Sept 2024";


void setup()
{
  FirstLoop = true;
  delay(5000);


  Serial.println("5 sec delay finished ");

  pinMode(LedPin, OUTPUT);


  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.println("void setup starting ");
  Serial.println("Booting PowerMon  16th Sept 2024, ...");
  //delay(2000);
  Serial.println("flashing LED on LedPin...");
  //flash fast a few times to indicate CPU is booting
  digitalWrite(LedPin, LOW);
  delay(100);
  digitalWrite(LedPin, HIGH);
  delay(100);
  digitalWrite(LedPin, LOW);
  delay(100);
  digitalWrite(LedPin, HIGH);
  delay(100);
  digitalWrite(LedPin, LOW);
  delay(100);
  digitalWrite(LedPin, HIGH);

  Serial.println("flashing over Delaying 10 sec...");

  // Initialise wifi connection
  wifiConnected = connectWifi();

  if (wifiConnected)
  {

    Serial.println("flashing slow to indicate wifi connected...");
    //flash slow a few times to indicate wifi connected OK
    digitalWrite(LedPin, LOW);
    delay(1000);
    digitalWrite(LedPin, HIGH);
    delay(1000);
    digitalWrite(LedPin, LOW);
    delay(1000);
    digitalWrite(LedPin, HIGH);
    delay(1000);
    digitalWrite(LedPin, LOW);
    delay(1000);
    digitalWrite(LedPin, HIGH);

    Serial.begin(115200);

    Serial.println("Booting PowerMon  16th Sept 2024, ...");
    Serial.println("Booting PowerMon  16th Sept 2024, ...");
    Serial.println("Booting PowerMon  16th Sept 2024, ...");

    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();


  }
  // populate event log headers
  //String EventLogArray[EventLogArraySize] ; // 10 events each containing Date [0], Time[1], FailedDeviceID [2]. last 3 entries (30,31,32) are headers



  EventLogArray[(EventLogArraySize - 4)] = "Date";
  EventLogArray[(EventLogArraySize - 3)] = "Time";
  EventLogArray[(EventLogArraySize - 2)] = "Event";

  EventLogArray[EventLogArraySize - 7] = "1st Entry date";
  EventLogArray[EventLogArraySize - 6] = "1st Entry time";
  EventLogArray[EventLogArraySize - 5] = "1st Entry Event";


  RotateEventLog();




  WDResponcesArray[24] = 0; // need to put this in setup (only this one) I suspect not used...


  SetTime(); // sync the clock..
  Serial.println(" clock sync completed...");

  // preload the daysintoMinutes counter based on current time


  MinutesIntoDay = ((currenthours * 60) + currentminutes);


  /*
    // Load root certificate in DER format into WiFiClientSecure object
    bool res = client.setCACert_P(caCert, caCertLen);
    if (!res) {
     Serial.println("Failed to load root CA certificate!");
     while (true) {
       yield();
     }
     Serial.println("root CA certificate loaded");
    }
  */
  // Save reboot date/time



  LastRebootDate = (String(currentday) + " / " + String(currentmonth));
  LastRebootTime = (String(currenthours) + ":" + String(currentminutes) + ":" + String(currentseconds));

  // Insert reboot time as first event in event table

  EventLogEntry = "Restarted";
  CreateLogEntry();


  // Populate 0  in all daily reading log slots

  for (DailyReadingsArrayIndex = 0; DailyReadingsArrayIndex < 210; DailyReadingsArrayIndex = DailyReadingsArrayIndex + 1) {

    DailyReadingsArray[DailyReadingsArrayIndex] = 0;// or DailyReadingsArrayIndex;

  }

  // Populate days of month into array
  DailyReadingsArrayIndex = 6;
  for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {

    DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter)] = DayCounter;

  }

  // Read whole month of data from EEPROM to DailyReadings Array as part of restart routine


  // read eprom daily data back
  EEPROM.begin(512);
  EEPROMIndex = 100;
  DailyReadingsArrayIndex = 7; // 1st normal reading is 7 (process adds +1)
  for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {
    Serial.println (" Location " + String (EEPROMIndex + (DayCounter)) + " Contents " + String( EEPROM.read (DayCounter + EEPROMIndex)));
    Serial.println (" Write Address " + String (DailyReadingsArrayIndex) + " Value " + String( EEPROM.read (EEPROMIndex + DayCounter)));
    DailyReadingsArray[DailyReadingsArrayIndex] = EEPROM.read (EEPROMIndex + DayCounter);
    DailyReadingsArrayIndex = DailyReadingsArrayIndex + 6;
  }
  Serial.println("read 1-31 from eprom index 100-131 to serial port and daily reading array");

  DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1)] = EEPROM.read (EEPROMIndex + DayCounter - 1);

  EEPROMIndex = 200;// bigger? normal readings overlap
  DailyReadingsArrayIndex = 8; // 1st peak reading
  for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {
    Serial.println (" Location " + String (EEPROMIndex + (DayCounter)) + " Contents " + String( EEPROM.read(DayCounter + EEPROMIndex)));
    Serial.println (" Write Address " + String (DailyReadingsArrayIndex) + " Value " + String( EEPROM.read (EEPROMIndex + DayCounter)));
    DailyReadingsArray[DailyReadingsArrayIndex] = EEPROM.read (EEPROMIndex + DayCounter);
    DailyReadingsArrayIndex = DailyReadingsArrayIndex + 6;
  }
  Serial.println("read 1-31 from eprom index 200-231 to serial port and daily reading array");



  EEPROM.end();



  EventLogEntry = "Copied months data from EEPROM to Array";
  CreateLogEntry();


  Serial.println("end of void setup... ");
} // void setup end



void loop()
{


  // Serial.println("void loop starting ");

  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    connectWifi();
    return;
  }

  pinMode(HeatingPin, FUNCTION_3);
  pinMode(HeatingPin, INPUT);

  HeatingState = !digitalRead(HeatingPin); // Inverted to suit hardware - LDR and neon across mains. LDR reads about 3kohm when mains on


  // allow test button to simulate heating
  if (TestHeating == 1) {
    HeatingState = HIGH;
  }


  delay(10);
  if (HeatingState == LOW) {
    if (PrevHeatingState == HIGH) {
      Serial.println("Heating Just turned OFF");
      HeatingStateString = "Heater Off";

      if (PeakTime == true) {

        EventLogEntry = "Heater Off (Peak). Running time was " + String(Heatime) + " Mins. Estimated Cost was $" + String((Heatime * PeakCostPerMin) / 100)  ; //
        CreateLogEntry();
      }
      else {
        EventLogEntry = "Heater Off. Running time was " + String(Heatime) + " Mins. Estimated Cost was $" + String((Heatime * NormalCostPerMin) / 100)  ; //
        CreateLogEntry();
      }

    }
    Heatime = 0;
  }

  if (HeatingState == HIGH) {
    if (PrevHeatingState == LOW) {
      Serial.println("Heating Just turned ON");
      HeatingStateString = "Heating";

      if (PeakTime == true) {
        EventLogEntry = "Heater On (Peak)";
        CreateLogEntry();
      }
      else {
        EventLogEntry = "Heater On ";
        CreateLogEntry();
      }
    }
  }
  PrevHeatingState = HeatingState;   // remember prev state for next pass








  if (DSTOffset != PrevDSTOffset) {

    Serial.println("DST was just adjusted");

    EventLogEntry = "DST Adjusted ";
    CreateLogEntry();

  }

  PrevDSTOffset = DSTOffset;   // remember prev state for next pass







  WiFiClient client = server.available();   // Listen for incoming clients
  if (client)
  { // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            Serial.println(header);
            //delay (5); // so you can interpet header contents

            if (header.indexOf("GET /Status") >= 0) {
              WebPageMode = 2; // Runtime
              PageMode = 0; // Status
            }
            if (header.indexOf("GET /EventLog") >= 0) {
              WebPageMode = 2; // Runtime
              PageMode = 1; // EventLog
            }
            if (header.indexOf("GET /DailyReadings") >= 0) {
              WebPageMode = 2; // Runtime
              PageMode = 2; // DailyReadings
            }

            if (header.indexOf("GET /DailyReadings2") >= 0) {
              WebPageMode = 2; // Runtime
              PageMode = 3; // Runtime DailyReadings2
            }

            if (header.indexOf("GET /TestHeatingOn") >= 0) {
              TestHeating = 1;
            }
            if (header.indexOf("GET /TestHeatingOff") >= 0) {
              TestHeating = 0;
            }
            if (header.indexOf("GET /Debug1On") >= 0) {
              Debug1 = 1;
            }
            if (header.indexOf("GET /Debug1Off") >= 0) {
              Debug1 = 0;
            }
            if (header.indexOf("GET /Debug2On") >= 0) {
              Debug2 = 1;
            }
            if (header.indexOf("GET /Debug2Off") >= 0) {
              Debug2 = 0;
            }
            if (header.indexOf("GET /Debug3On") >= 0) {
              Debug3 = 1;
            }
            if (header.indexOf("GET /Debug3Off") >= 0) {
              Debug3 = 0;
            }
            if (header.indexOf("GET /IncDST") >= 0) {
              //DSTState = true;
              DSTOffset = DSTOffset + 3600;
              SetTime(); // resync the clock
            }
            if (header.indexOf("GET /DecDST") >= 0) {
              //DSTState = false;
              DSTOffset = DSTOffset - 3600;
              SetTime(); // resync the clock
            }


            if (header.indexOf("GET /IncAlexaNotifyThreshold") >= 0) {
              //AlexaNotificationThreshold = AlexaNotificationThreshold + 1;
            }
            if (header.indexOf("GET /DecAlexaNotifyThreshold") >= 0) {
              //AlexaNotificationThreshold = AlexaNotificationThreshold - 1;
            }



            switch (WebPageMode ) { // 1 = Setup, 2 = Status

              case 1 : // Re-Setup Mode. Here on virgin birth, wifi not found or someone pressed the Re-Setup button

                break;

              case 2: // Runtime Mode

                //Display the comman parts of the PowerMon web page

                client.println("<!DOCTYPE html><html>");
                client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                client.println("<link rel=\"icon\" href=\"data:,\">");
                // CSS to style the on/off buttons
                // Feel free to change the background-color and font-size attributes to fit your preferences
                //client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                client.println("<style>html { font-family: Courier; display: inline; margin: 0; margin-top:0px; padding: 0; text-align: center;}");

                client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
                client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");

                //client.println(".redrectangle { background-color: red; border: none; color: black; padding: 40px 10px;}");
                //client.println(".purplerectangle { background-color: purple; border: none; color: black; padding: 100px 10px;}");
                client.println(".genericrectangle { background-color: brown; border: none; color: white; padding: " + String(RectHeight) + "px " + String(RectWidth) + "px;}");
                //client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");


                //client.println(".button2 {background-color: #77878A;}");
                client.println(".buttonFault {background-color:" + String(FaultColor) + ";}");
                client.println(".buttonPeak {background-color:" + String(PeakColor) + ";}");
                client.println(".buttonNormal {background-color:" + String(NormalColor) + ";}</style></head>");

                //client.println(".buttonsmall { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
                //client.println("text-decoration: none; font-size: 10px; margin: 2px; cursor: pointer;}");

                // Web Page Heading
                client.println("<body><h1>PowerMonitor Status</h1>");

                // Display current heating state
                //client.println("<p>Current State " + HeatingStateString + "</p>");

                if (HeatingStateString == "Heating") {
                  client.println("<p><a href=\"/2/on\"><button class=\"button buttonFault\">Heating</button></a> </p>");
                } else {
                  client.println("<p><a href=\"/2/off\"><button class=\"button buttonNormal\">Heater Off</button></a></p>");
                }
                client.println("<p>Heater has been running for " + String(Heatime) + " Minutes</p>");

                if (PeakTime == true) {
                  client.println("<p><a href=\"/2/on\"><button class=\"button buttonPeak\">Peak</button></a> </p>");
                } else {
                  client.println("<p><a href=\"/2/off\"><button class=\"button buttonNormal\">Off Peak</button></a></p>");
                }

                // Display day of week
                client.println("<p>Current day of week is " + String(currentdayofweek) + "</p>");






                switch (PageMode) { // { 0 =  Status, 1 = EventLog, 2 = not used, 3 = DailyReadings2

                  case 0: // "Status" :

                    // Display date
                    client.println("<p>Current Date is " + String(currentday) + " / " + String(currentmonth)  + "</p>");
                    // Display current time of day
                    client.println("<p>Current Time is " + String(currenthours) + ":" + String(currentminutes) + ":" + String(currentseconds) + ":" + "</p>");

                    // client.println("<p>Current Minutes into day " + String(MinutesIntoDay) + "</p>");

                    //float NormalCostPerMin = (ElementPower * OffPeakPowerCost)/ 60;
                    //float PeakCostPerMin = (ElementPower * PeakPowerCost) / 60;

                    //client.println("<p>Normal Power cost per min for our 3kw system is 0.888 c/min </p>"); // calculated mannually

                    //client.println("<p>Peak Power cost  for our 3kw system is  1.373 c/min </p>");

                    client.println("<p>Normal Power cost for our 3kw system is " + String(NormalCostPerMin) + " cents per min</p>");

                    client.println("<p>Peak Power cost for our 3kw system is "  + String(PeakCostPerMin) + " cents per min</p>");

                    //client.println("<p>Saving per min if Peak use is removed "  + String(PeakCostPerMin-NormalCostPerMin) + " cents per min</p>");

                    client.println("<p>Saving so far this month if we removed Peak use $"  + String(DailyReadingsArray[200] * (PeakCostPerMin - NormalCostPerMin) / 100) + " </p>");
                    //put this value in array as well
                    DailyReadingsArray[198] = (DailyReadingsArray[200] * (PeakCostPerMin - NormalCostPerMin) / 100);


                    //

                    //Display Status, EventLog, DailyReadings Buttons
                    client.println("<p><a href=\"/Status\"><button class=\"buttonsmall\">Status</button></a> <a href=\"/EventLog\"><button class=\"buttonsmall\">EventLog</button></a> <a href=\"/DailyReadings2\"><button class=\"buttonsmall\">DailyReadings</button></a></p>");


                    // Display Test Heating Button
                    if (TestHeating == 1) {
                      client.println("<p><a href=\"/TestHeatingOff\"><button class=\"buttonsmall\">Test Off</button></a> </p>");
                    } else {
                      client.println("<p><a href=\"/TestHeatingOn\"><button class=\"buttonsmall\">Test On</button></a></p>");
                    }

                    // Display Debug Buttons
                    if (Debug1 == 1) {
                      client.println("<p><a href=\"/Debug1Off\"><button class=\"buttonsmall\">Debug1 Off</button></a> </p>");
                    } else {
                      client.println("<p><a href=\"/Debug1On\"><button class=\"buttonsmall\">Debug1 On (write daily data to eeprom)</button></a></p>");
                    }

                    if (Debug2 == 1) {
                      client.println("<p><a href=\"/Debug2Off\"><button class=\"buttonsmall\">Debug2 Off</button></a> </p>");
                    } else {
                      client.println("<p><a href=\"/Debug2On\"><button class=\"buttonsmall\">Debug2 On (write August 24 summary data to previous month )</button></a></p>");
                    }

                    if (Debug3 == 1) {
                      client.println("<p><a href=\"/Debug3Off\"><button class=\"buttonsmall\">Debug3 Off</button></a> </p>");
                    } else {
                      client.println("<p><a href=\"/Debug3On\"><button class=\"buttonsmall\">Debug3 On (populate sept24 data to daily array)</button></a></p>");
                    }

                    // Display DST INC & Dec Buttons
                    //if (DSTState == true) {
                    client.println("<p><a href=\"/IncDST\"><button class=\"buttonsmall\">Inc DST</button></a> <a href=\"/DecDST\"><button class=\"buttonsmall\">Dec DST</button></a> </p>");
                    //} else {
                    //client.println("<p><a href=\"/DecDST\"><button class=\"buttonsmall\">Dec DST</button></a></p>");
                    //}


                    break;
                  case 1: // "EventLog" :


                    // Display date
                    client.println("<p>Current Date is " + String(currentday) + " / " + String(currentmonth)  + "</p>");

                    // Display current time of day
                    client.println("<p>Current Time is " + String(currenthours) + ":" + String(currentminutes) + ":" + String(currentseconds) + ":" + "</p>");

                    // Display last Reboot Time
                    client.println("<p>Last Restart was " + LastRebootTime + " on " + LastRebootDate + " which was " + String(UpTimeDays)  + " days ago" + "</p>");

                    // Display Compile Date
                    client.println("<p>Compile Date was " + CompileDate + "</p>");


                    //Display Status, EventLog, DailyReadings Buttons
                    client.println("<p><a href=\"/Status\"><button class=\"buttonsmall\">Status</button></a> <a href=\"/EventLog\"><button class=\"buttonsmall\">EventLog</button></a> <a href=\"/DailyReadings2\"><button class=\"buttonsmall\">DailyReadings</button></a></p>");


                    // Display complete log

                    for (EventLogArrayIndex = (EventLogArraySize - 4); EventLogArrayIndex > 0; EventLogArrayIndex = EventLogArrayIndex - 3) {

                      client.println("<p>" + (EventLogArray[EventLogArrayIndex]) + "       " + (EventLogArray[(EventLogArrayIndex + 1)]) + " " + (EventLogArray[(EventLogArrayIndex + 2)]) +  "</p>");


                    }



                    //Display Status, EventLog, DailyReadings Buttons
                    client.println("<p><a href=\"/Status\"><button class=\"buttonsmall\">Status</button></a> <a href=\"/EventLog\"><button class=\"buttonsmall\">EventLog</button></a> <a href=\"/DailyReadings2\"><button class=\"buttonsmall\">DailyReadings</button></a></p>");


                    break;

                  case 2: // "DailyReadings" :


                  case 3: // "DailyReadings2":
                    // display proxy log
                    // Display date
                    client.println("<p>Current Date is " + String(currentday) + " / " + String(currentmonth)  + "</p>");


                    // Display current time of day
                    client.println("<p>Current Time is " + String(currenthours) + ":" + String(currentminutes) + ":" + String(currentseconds) + ":" + "</p>");

                    //Display Status, EventLog Buttons, DailyReadings Button
                    client.println("<p><a href=\"/Status\"><button class=\"buttonsmall\">Status</button></a> <a href=\"/EventLog\"><button class=\"buttonsmall\">EventLog</button></a> <a href=\"/DailyReadings2\"> <button class=\"buttonsmall\">DailyReadings</button></a></p>");

                    client.println("<p> day of month &nbsp; &nbsp; &nbsp; &nbsp;  normal mins &nbsp; &nbsp; peak mins &nbsp; &nbsp;  Normal Cost $ &nbsp; &nbsp;  Peak Cost $ &nbsp; &nbsp; Total Cost $ </p>");

                    //client.println("<p>Current Date is " + String(currentday) + " / " + String(currentmonth)  + "</p>");


                    for (DailyReadingsArrayIndex = 0; DailyReadingsArrayIndex < 191; DailyReadingsArrayIndex = DailyReadingsArrayIndex + 6) {

                      client.println("<p>" + String(DailyReadingsArray[DailyReadingsArrayIndex]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 1)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 2)]) +  eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 3)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 4)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 5)]) + "</p>");

                    }

                    client.println("<p> Average &nbsp; &nbsp; &nbsp; &nbsp;  normal mins &nbsp; &nbsp; peak mins &nbsp; &nbsp;  Normal Cost $ &nbsp; &nbsp;  Peak Cost $ &nbsp; &nbsp; Total Cost $ </p>");

                    DailyReadingsArrayIndex = 192;
                    client.println("<p>" + String(DailyReadingsArray[DailyReadingsArrayIndex]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 1)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 2)]) +  eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 3)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 4)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 5)]) + "</p>");

                    client.println("<p> Month Potential Savings &nbsp; &nbsp; &nbsp; &nbsp;  normal mins &nbsp; &nbsp; peak mins &nbsp; &nbsp;  Normal Cost $ &nbsp; &nbsp;  Peak Cost $ &nbsp; &nbsp; Total Cost $ </p>");
                    DailyReadingsArrayIndex = 198;
                    client.println("<p>" + String(DailyReadingsArray[DailyReadingsArrayIndex]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 1)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 2)]) +  eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 3)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 4)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 5)]) + "</p>");

                    client.println("<p> Prev Month Potential Savings &nbsp; &nbsp; &nbsp; &nbsp;  normal mins &nbsp; &nbsp; peak mins &nbsp; &nbsp;  Normal Cost $ &nbsp; &nbsp;  Peak Cost $ &nbsp; &nbsp; Total Cost $ </p>");
                    DailyReadingsArrayIndex = 204;
                    client.println("<p>" + String(DailyReadingsArray[DailyReadingsArrayIndex]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 1)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 2)]) +  eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 3)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 4)]) + eightspaces + String(DailyReadingsArray[(DailyReadingsArrayIndex + 5)]) + "</p>");


                    // }

                    //Display Status, EventLog Buttons, DailyReadings Button
                    client.println("<p><a href=\"/Status\"><button class=\"buttonsmall\">Status</button></a> <a href=\"/EventLog\"><button class=\"buttonsmall\">EventLog</button></a> <a href=\"/DailyReadings2\"> <button class=\"buttonsmall\">DailyReadings</button></a></p>");


                    break;

                }

                break;


            }

            // the closing part of every page

            client.println("</body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  } // if a new client connects

  // Count Standing / Sitting Time
  if (millis() >= (previousMillis)) {
    //Serial.print(" millis = ");
    //Serial.print(String (millis())); // takes about 1300 millis in dev, 431 in prod.
    previousMillis = previousMillis + 1000;
    //Serial.print(" prevmillis = ");
    //Serial.print(String (previousMillis));
    // should be here every second....


    //maintain 10 second timer and call rotate WD

    WDLoopTimer = WDLoopTimer + 1;

    if (WDLoopTimer > WDLoopTimerThreshold) {

      WDLoopTimer = 0;
      //RotateAndCheckWDArray();

      //Serial.println(" WDCall = ");
      //Serial.println(String (WDCall));
    }
    /*
      int MorningPeakStart = 420;
      int MorningPeakStop =  660;
      int EveningPeakStart = 1020;
      int EveningPeakStop = 1140;

      int MinutesIntoDay = 0 ; // minutes so far into the day (gets to 1440 by midnight)
      bool PeakTime = false; // boolean true is PeakTime
    */


    // Maintain RTC

    currentseconds = currentseconds + 1;
    if (currentseconds == 60)
    {
      // Things to do every minute here
      currentseconds = 0;
      currentminutes = currentminutes + 1;
      //Serial.println("another minute has passed");
      MinutesIntoDay = MinutesIntoDay + 1;


      // increment Daily Reading array if heating


      // bool PeakTime = false; // boolean true is PeakTime

      if (HeatingState == HIGH) { // accumulate usage only if heating right now
        if (PeakTime == false) { // Not Peak
          DailyReadingsArray[(currentday * 6) + 1] = DailyReadingsArray[(currentday * 6) + 1] + 1;
        }
        else { // Peak
          DailyReadingsArray[(currentday * 6) + 2] = DailyReadingsArray[(currentday * 6) + 2] + 1;
        }
        Heatime = Heatime + 1; //
      }// accumulate usage - only if heating

      // calculate costs for current day

      //  DailyReadingsArray[(currentday *6) + 3] = (DailyReadingsArray[(currentday *6) + 1] * NormalCostPerMin)/100;// c / kwm
      // DailyReadingsArray[(currentday *6) + 4] = (DailyReadingsArray[(currentday *6) + 2] * PeakCostPerMin)/100; // c / kwm
      // DailyReadingsArray[(currentday *6) + 5] = DailyReadingsArray[(currentday *6) + 3] + DailyReadingsArray[(currentday *6) + 4];


      // calculate costs for every day (allows past vaules to be poked into the array
      // costs in cents = ((ElementPower * (mins /60) * OffPeakPowerCost))


      for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {


        DailyReadingsArray[(DayCounter * 6) + 3] = (DailyReadingsArray[(DayCounter * 6) + 1] * NormalCostPerMin) / 100; // c / kwm
        DailyReadingsArray[(DayCounter * 6) + 4] = (DailyReadingsArray[(DayCounter * 6) + 2] * PeakCostPerMin) / 100; // c / kwm
        DailyReadingsArray[(DayCounter * 6) + 5] = DailyReadingsArray[(DayCounter * 6) + 3] + DailyReadingsArray[(DayCounter * 6) + 4];
      }

      //EventLogEntry = "New Minute Rollover";
      //CreateLogEntry();



      //update monthly average - todo
      //index 192

      //update monthly totals
      //index 198



      DailyReadingsArray[199] = 0;// nuke monthly normal power total every min and recalculate
      DailyReadingsArrayIndex = 7 ; // 1st normal reading
      for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {

        DailyReadingsArray[199] = (DailyReadingsArray[199] + (DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1)]));
      }

      DailyReadingsArray[200] = 0;// nuke monthly peak power total every min and recalculate
      DailyReadingsArrayIndex = 8 ; // 1st peak reading

      //      DailyReadingsArray[200] = DailyReadingsArray[8] 1st reading

      // update monthly costs
      for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {

        DailyReadingsArray[200] = (DailyReadingsArray[200] + DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - ((DayCounter - 1) * 2)]);

        DailyReadingsArray[201] = (DailyReadingsArray[199] * NormalCostPerMin) / 100; // c / kwm
        DailyReadingsArray[202] = (DailyReadingsArray[200] * PeakCostPerMin) / 100; // c / kwm
        DailyReadingsArray[203] = DailyReadingsArray[201] + DailyReadingsArray[202];

      }





    } // end of things to do each minute


    if (currentminutes == 60)
    {


      // Things to do every hour here
      currentminutes = 0;
      currenthours = currenthours + 1;
      UpTimeDays = UpTimeDays + 0.0417 ; // (1/24)

      //Serial.println("UpTimeDays =  " + String(UpTimeDays) );



    }// end of things to do every hour

    if (((currenthours == 23 ) || (currenthours == 06 ) || (currenthours == 12 ) || (currenthours == 18 )) && (currentminutes == 0) && (currentseconds == 01))  { // every 6 hrs

      // write todays data so far to EEPROM



      //ideally read justthe neccessary bytes here and write that with a much smaller array. but complex to do

      // writing all daily data to EEPROM instead

      EEPROM.begin(512);
      EEPROMIndex = 100;
      DailyReadingsArrayIndex = 7; // 1st normal reading

      for (DayCounter = 1; DayCounter <= 32; DayCounter = DayCounter + 1) {
        //Serial.println("Write Loop EEPROMIndex " + String(EEPROMIndex) + " DailyReadingsArrayIndex " + String(DailyReadingsArrayIndex) + " DayCounter " + String(DayCounter));
        EEPROM.write ((EEPROMIndex + (DayCounter)), (DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1)]));

        Serial.println("EEPROM Write Address " + String(EEPROMIndex + (DayCounter)) + " Content " + String(DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1)]) );
      }
      EEPROMIndex = 200; // Peak readings EEPROM Array
      DailyReadingsArrayIndex = 8 ; // 1st peak reading

      for (DayCounter = 1; DayCounter <= 32; DayCounter = DayCounter + 1) {

        EEPROM.write ((EEPROMIndex + (DayCounter)), (DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - ((DayCounter - 1) * 2)]));
        Serial.println("EEPROM Write Address " + String(EEPROMIndex + (DayCounter)) + " Content " + String(DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1) * 2]) );

      }
      EEPROM.commit();


      EEPROM.end();

      EventLogEntry = "Daily data committed to EEPROM";
      CreateLogEntry();




    } // end of things every 6 hours


    if (currenthours == 24)
    {
      // Things to do every day here
      currentseconds = 0;
      currentminutes = 0;
      currenthours = 0;
      MinutesIntoDay = 0;
      PeakTime = false;



      SetTime (); // resync the clock




    }

    // Check if in Peak or Off Peak period

    //PeakTime = false;

    if (MinutesIntoDay >= MorningPeakStart) {
      PeakTime = true;
    }

    if (MinutesIntoDay >= MorningPeakStop) {
      PeakTime = false;
    }

    if (MinutesIntoDay >= EveningPeakStart) {
      PeakTime = true;
    }

    if (MinutesIntoDay >= EveningPeakStop) {
      PeakTime = false;
    }

    if (currentdayofweek == 0) { // 0 days since sunday
      PeakTime = false;
    }

    if (currentdayofweek == 6) { // 6 days since sunday
      PeakTime = false;
    }

    // Detect a new month rollover

    // byte currentmonth = 0 ;
    // byte currentmonthPrevious



    if (FirstLoop == false) { // dont do this on 1st pass - it triggers new month activities
      if (currentmonth != currentmonthPrevious) { // != is not equal
        // a new month has rolled over
        Serial.println("a new month has rolled over");
        EventLogEntry = "New Month Rollover";
        CreateLogEntry();
        // Copy monthly totals to previous Month


        // Current Month results [198 - 203] cost savings this month [198], spare [199], normal pwr total [200], peak pwr total [201], normal costs? [202], peak costs [203]
        // Previous Month [204 - ]  cost savings prev month [204], spare [205], spare [206], spare [207], spare [208], spare [209]
        // [210] not used
        DailyReadingsArray[204] = DailyReadingsArray[198];

        DailyReadingsArray[205] = DailyReadingsArray[199];
        DailyReadingsArray[206] = DailyReadingsArray[200];
        DailyReadingsArray[207] = DailyReadingsArray[201];
        DailyReadingsArray[208] = DailyReadingsArray[202];
        DailyReadingsArray[209] = DailyReadingsArray[203];


        EventLogEntry = "Copied monthly data to previous month";
        CreateLogEntry();

        // Populate 0  in all daily reading log slots

        for (DailyReadingsArrayIndex = 0; (DailyReadingsArrayIndex < 198); DailyReadingsArrayIndex = DailyReadingsArrayIndex + 1) {

          DailyReadingsArray[DailyReadingsArrayIndex] = 0;// or DailyReadingsArrayIndex;

          // need to consider eeprom content as well
        }

        EventLogEntry = "Daily Data Cleared";
        CreateLogEntry();
        // clearing EEPROM

        EEPROM.begin(512);
        EEPROMIndex = 100;
        DailyReadingsArrayIndex = 7; // 1st normal reading

        for (DayCounter = 1; DayCounter <= 32; DayCounter = DayCounter + 1) {
          //Serial.println("Write Loop EEPROMIndex " + String(EEPROMIndex) + " DailyReadingsArrayIndex " + String(DailyReadingsArrayIndex) + " DayCounter " + String(DayCounter));
          EEPROM.write ((EEPROMIndex + DayCounter), 0);
          //EEPROM.write ((EEPROMIndex+(DayCounter)),(DayCounter));
          Serial.println("EEPROM Write Address " + String(EEPROMIndex + (DayCounter)) + " Content " + String(0));
        }
        EEPROMIndex = 200; // Peak readings EEPROM Array
        DailyReadingsArrayIndex = 8 ; // 1st peak reading

        for (DayCounter = 1; DayCounter <= 32; DayCounter = DayCounter + 1) {

          EEPROM.write ((EEPROMIndex + DayCounter), 0);
          Serial.println("EEPROM Write Address " + String(EEPROMIndex + (DayCounter)) + " Content " + String(0));

        }




        EEPROM.commit();


        EEPROM.end();
        EventLogEntry = "daily EEPROM data Cleared, previous month data stored";
        CreateLogEntry();







        // Populate days of month into array (data cleaning removes these)
        DailyReadingsArrayIndex = 6;
        for (DayCounter = 1; DayCounter <= 31; DayCounter = DayCounter + 1) {

          DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter)] = DayCounter;

        }













      } // end of monthly tasks
    }// first pass check
    currentmonthPrevious = currentmonth;


  } // end 1 second




  if (TestHeating == 1) {
    if (PrevTestHeating == 0) {

      //  do something else other than test heating here



    }
  } // if Testheating = 1


  PrevTestHeating = TestHeating;


  if (Debug1 == 1) {
    if (PrevDebug1 == 0) {
      // here on first push of debug button




      // writing all daily data to EEPROM

      EEPROM.begin(512);
      EEPROMIndex = 100;
      DailyReadingsArrayIndex = 7; // 1st normal reading

      for (DayCounter = 1; DayCounter <= 32; DayCounter = DayCounter + 1) {
        //Serial.println("Write Loop EEPROMIndex " + String(EEPROMIndex) + " DailyReadingsArrayIndex " + String(DailyReadingsArrayIndex) + " DayCounter " + String(DayCounter));
        EEPROM.write ((EEPROMIndex + (DayCounter)), (DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1)]));
        //EEPROM.write ((EEPROMIndex+(DayCounter)),(DayCounter));
        Serial.println("EEPROM Write Address " + String(EEPROMIndex + (DayCounter)) + " Content " + String(DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1)]) );
      }
      EEPROMIndex = 200; // Peak readings EEPROM Array
      DailyReadingsArrayIndex = 8 ; // 1st peak reading

      for (DayCounter = 1; DayCounter <= 32; DayCounter = DayCounter + 1) {

        EEPROM.write ((EEPROMIndex + (DayCounter)), (DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - ((DayCounter - 1) * 2)]));
        Serial.println("EEPROM Write Address " + String(EEPROMIndex + (DayCounter)) + " Content " + String(DailyReadingsArray[(DailyReadingsArrayIndex * DayCounter) - (DayCounter - 1) * 2]) );

      }

      // write last month data to eeprom


      EEPROM.commit();


      EEPROM.end();










    }

  }

  PrevDebug1 = Debug1;

  if (Debug2 == 1) {
    if (PrevDebug2 == 0) {
      // here on first push of debug button2

      // Previous Month [204 - ]  cost savings [204], normal mins [205], peak mins [206], normal $ [207], peak $ [208], total costs [209]
      DailyReadingsArray[204] = 7.62; // Previous Month savings
      DailyReadingsArray[205] = 1692; // Previous Month Normal
      DailyReadingsArray[206] = 1570; // Previous Month peak
      DailyReadingsArray[207] = 15.04; // Previous Month Normal$
      DailyReadingsArray[208] = 21.56; // Previous Month peak$
      DailyReadingsArray[209] = 36.60; // Previous total$


      EventLogEntry = "Populated August 2024 previous month data into Daily Array (extrapolated)";
      CreateLogEntry();
      /*
            // trigger month changeover with test button

           //if (TestHeating == 1){
            Serial.println(" new month simulated");
                Serial.println(" first pass current month" + String(currentmonth));
              currentmonth = currentmonth +1;

               Serial.println(" new current month" + String(currentmonth));
              Serial.println(" previouscurrent month" + String(currentmonthPrevious));
              //TestHeating = 0;
            //}
      */









    }

  }

  PrevDebug2 = Debug2;


  if (Debug3 == 1) {
    if (PrevDebug3 == 0) {
      // here on first push of debug button3


      // here on first push of test button

      //populate array with data from august 2024 if test button pushed
      // first row [0-5] not used. Could be used for variables later?
      // 1st day [6-11] each containing Day of month [6],  normal power mins [7], peak power mins [8], normal cost [9], peak cost[10], , total cost[10]
      // 2nd day [12-17] each containing Day of month [12], normal power mins[13], peak power mins [14], normal cost [15], peak cost [16], spare [17]

      // 15th day [90-95] each containing Day of month [90], normal power mins [91], peak power mins [92], spare [93], spare [94]
      // 31st day [186-191 ] Day of month [186], day of week[187], normal power mins [188], peak power mins [189], spare [190], spare [191]
      // Average results [192 - 197] spare [192], spare [193], normal pwr total [194], peak pwr total [195], normal costs? [196], peak costs [197]
      // Current Month results [198 - 203] cost savings [198], spare [199], normal pwr total [200], peak pwr total [201], normal costs? [202], peak costs [203]
      // Previous Month [204 - ]  cost savings [204], normal mins [205], peak mins [206], normal $ [207], peak $ [208], total costs [209]
      // [210] not used


      // Sept 2024 data

      DailyReadingsArray[7] = 80; // 1 day normal
      DailyReadingsArray[8] = 0; // 1 day peak

      DailyReadingsArray[13] = 0; // 2 day normal
      DailyReadingsArray[14] = 67; // 2 day peak

      DailyReadingsArray[19] = 35; // 3 day normal
      DailyReadingsArray[20] = 38; // 3 day peak

      DailyReadingsArray[25] = 19; // 4 day normal
      DailyReadingsArray[26] = 66; // 4 day peak

      DailyReadingsArray[31] = 86; // 5 day normal
      DailyReadingsArray[32] = 158; //  day peak

      DailyReadingsArray[37] = 34; // 6 day normal
      DailyReadingsArray[38] = 0; //  day peak

      DailyReadingsArray[43] = 29; // 7 day normal
      DailyReadingsArray[44] = 0; //  day peak

      DailyReadingsArray[49] = 0; // 8 day normal
      DailyReadingsArray[50] = 63; //  day peak

      DailyReadingsArray[55] = 0; // 9 day normal
      DailyReadingsArray[56] = 32; //  day peak

      DailyReadingsArray[61] = 40; // 10 day normal
      DailyReadingsArray[62] = 52; // day peak

      DailyReadingsArray[67] = 5; // 11 day normal
      DailyReadingsArray[68] = 73; // day peak

      DailyReadingsArray[73] = 81; // 12 day normal
      DailyReadingsArray[74] = 85; // day peak

      DailyReadingsArray[79] = 17; // 13 day normal
      DailyReadingsArray[80] = 15; //  day peak

      DailyReadingsArray[85] = 66; // 14 day normal
      DailyReadingsArray[86] = 0; // day peak

      DailyReadingsArray[91] = 107; // 15 day normal
      DailyReadingsArray[92] = 0; // day peak

      DailyReadingsArray[97] = 24; // 16 day normal
      DailyReadingsArray[98] = 39; // day peak

      DailyReadingsArray[103] = 0; // 17 day normal
      DailyReadingsArray[104] = 0; //  day peak

      DailyReadingsArray[109] = 0; // 18 day normal
      DailyReadingsArray[110] = 0; //  day peak

      DailyReadingsArray[115] = 0; // 19 day normal
      DailyReadingsArray[116] = 0; //  day peak

      DailyReadingsArray[121] = 0; // 20 day normal
      DailyReadingsArray[122] = 0; //  day peak

      DailyReadingsArray[127] = 0; // 21 day normal
      DailyReadingsArray[128] = 0; //  day peak

      DailyReadingsArray[133] = 0; // 22 day normal
      DailyReadingsArray[134] = 0; //  day peak

      DailyReadingsArray[139] = 0; // 23 day normal
      DailyReadingsArray[140] = 0; //  day peak

      DailyReadingsArray[145] = 0; // 24 day normal
      DailyReadingsArray[146] = 0; //  day peak

      DailyReadingsArray[151] = 0; // 25 day normal
      DailyReadingsArray[152] = 0; //  day peak

      DailyReadingsArray[157] = 0; // 26 day normal
      DailyReadingsArray[158] = 0; // day peak

      DailyReadingsArray[163] = 0; // 27 day normal
      DailyReadingsArray[164] = 0; // day peak

      DailyReadingsArray[169] = 0; // 28 day normal
      DailyReadingsArray[170] = 0; // day peak

      DailyReadingsArray[175] = 0; // 29 day normal
      DailyReadingsArray[176] = 0; //  day peak

      DailyReadingsArray[181] = 0; // 30 day normal
      DailyReadingsArray[182] = 0; // day peak

      DailyReadingsArray[187] = 0; // 31 day normal
      DailyReadingsArray[188] = 0; // day peak





      EventLogEntry = "Poplated Sept 24 data into daily array";
      CreateLogEntry();

    }

  }

  PrevDebug3 = Debug3;





  // dummy up some data for testing

  FirstLoop = false;
} // end void loop




void TwitchLED () {


  digitalWrite(LedPin, LOW);
  delay(10);
  digitalWrite(LedPin, HIGH);
}

// connect to wifi – returns true if successful or false if not
boolean connectWifi() {
  boolean state = true;
  int i = 0;

  WiFi.mode(WIFI_STA);

  // Configures static IP address
  //if (!WiFi.config(local_IP, gateway, subnet)) {
  //Serial.println("STA Failed to configure");
  // this locked in ip address makes the clock sync fail
  // fixed by locking mac address to ip address in the router config instead
  //}

  WiFi.begin(RunTimessid, RunTimepassword);
  Serial.println("");
  Serial.println("Connecting to WiFi Network");

  Serial.println(RunTimessid);

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.print(".");
    if (i > 50) {
      state = false;
      break;
    }
    i++;
  }

  if (state) {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(RunTimessid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed. Bugger");
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return state;
}



void CreateLogEntry () {

  EventLogArray[EventLogArraySize - 7] = (String(currentday) + " / " + String(currentmonth));
  EventLogArray[EventLogArraySize - 6] = (String(currenthours) + ":" + String(currentminutes) + ":" + String(currentseconds));
  EventLogArray[EventLogArraySize - 5] = EventLogEntry;

  RotateEventLog ();

}

void RotateEventLog () {
  // Rotate EventLog towards index 0 each log entry is 3 entries

  for (EventLogArrayIndex = 0; EventLogArrayIndex < (EventLogArraySize - 4); EventLogArrayIndex = EventLogArrayIndex + 1) {

    EventLogArray[EventLogArrayIndex] = EventLogArray[(EventLogArrayIndex + 3)];
  }
}


void SetTime () {
  SetTimeWasSuccesfull = 0;

  Serial.println(" trying to synch clock...");
  // Synchronize time using SNTP. This is necessary to verify that
  // the TLS certificates offered by the server are currently valid. Also used by the graph plotting routines
  Serial.println("Setting time using SNTP");


  configTime(-13 * 3600, DSTOffset, "pool.ntp.org", "time.nist.gov"); // set localtimezone, DST Offset, timeservers, timeservers...



  time_t now = time(nullptr);
  //while (now < 8 * 3600 * 2) { // would take 8 hrs to fall thru. Thats a long time....
  while (now < 100) {
    delay(200);
    Serial.print(".");
    Serial.print(String (now));
    now = time(nullptr);
  }
  Serial.println("");


  struct tm * timeinfo; //http://www.cplusplus.com/reference/ctime/tm/
  time(&now);
  timeinfo = localtime(&now);
  Serial.println (timeinfo ->tm_wday);// days since sunday [0-6]
  Serial.println(timeinfo->tm_mon);
  Serial.println(timeinfo->tm_mday);
  Serial.println(timeinfo->tm_hour);
  Serial.println(timeinfo->tm_min);
  Serial.println(timeinfo->tm_sec);
  currentseconds = timeinfo->tm_sec ;
  currentminutes = timeinfo->tm_min;
  currenthours = timeinfo->tm_hour ;
  currentday = timeinfo->tm_mday;
  currentmonth = timeinfo->tm_mon;
  currentmonth = currentmonth + 1; // month counted from 0
  currentdayofweek = timeinfo ->tm_wday;
  SetTimeWasSuccesfull = 1;
  EventLogEntry = "Clock Sync Completed";
  CreateLogEntry();



}// SetTime






