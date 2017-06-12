/*********************************************
 * File : SmartTag.ino
 * Author: Shenli Yuan
 * Date: June 6th 2017
 * Hardware: Particle Electron 
 * Function: firmware for Trace tag; 
 * 			 Stanford ME310
 *			 2016 - 2017 school year
 *			 Team Volvo CE
 *********************************************/

#include <AssetTracker.h>
#include <ArduinoJson.h>

/* SCREEN */
#if defined(PARTICLE)
 #include <Adafruit_mfGFX.h>
 #include "Adafruit_ILI9341.h"
 // For the Adafruit shield, these are the default.
    #define TFT_CS D3   // if several devices share SPI otherwise CS = SS
    #define TFT_DC D2
    #define TFT_RST A0  // RST can be set to -1 if you tie it to Arduino's reset
#else
 #include "SPI.h"
 #include <Adafruit_mfGFX.h>
 #include "Adafruit_ILI9341.h"
 // For the Adafruit shield, these are the default.
    #define TFT_CS D3   // if several devices share SPI otherwise CS = SS
    #define TFT_DC D2
    #define TFT_RST A0  // RST can be set to -1 if you tie it to Arduino's reset
#endif
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, 0);
bool showBattery = false;
bool showTracking = false;
unsigned long startScreenTimer;

/* RESET*/
#define DELAY_BEFORE_REBOOT (5 * 1000)
unsigned int rebootDelayMillis = DELAY_BEFORE_REBOOT;
unsigned long rebootSync = millis();
bool resetFlag = false;


/* INFO */
String coreID = "tag3";
String setOwnerString = "";
String setSubString = "";
String setCellString = "";
String setItemString = "";
String setStartString = "";
String setEndString = "";

/* BATTERY */
FuelGauge fuel;
float fuelLevel;

// int transmittingData = 1;

int i = 0;

/* GPS */
AssetTracker t = AssetTracker();
long lastPublish = 0;
int transmittingData = 1;
String lastLocation = "";

/* IMU */
int accelThreshold = 12000;

/* STATE MACHINE */
const int TAG_CHARGING = 0;
const int TAG_SETUP = 1;
const int TAG_TRACKING = 2;
// enum int { CHARGING, SETUP, DISPLAY }tagState;
int currentState = TAG_CHARGING;

/* BUTTON */
#define BUTTON_PIN D1   // To digital read button
int current;         // Current state of the button
                     // (LOW is pressed b/c i'm using the pullup resistors)
long millis_held;    // How long the button was held (milliseconds)
long secs_held;      // How long the button was held (seconds)
long prev_secs_held; // How long the button was held in the previous check
byte previous = HIGH;
unsigned long firstTime; // how long since the button was first pressed 
int SHORT_PRESS = 1;
int MED_PRESS = 2;
int LONG_PRESS = 3;
int thisPress;


void setup() {
    Serial.begin(9600);
    /* GPS Setup */
    t.begin();
    t.gpsOn();
    Particle.function("gps", gpsPublish);
    /* Button Setup */
    pinMode(BUTTON_PIN, INPUT);
    /* Screen Setup */
    Serial.println("SPI pins");
    Serial.print("SS   : "); Serial.println(SS);
    Serial.print("SCK  : "); Serial.println(SCK);
    Serial.print("MOSI : "); Serial.println(MOSI);
    Serial.print("MISO : "); Serial.println(MISO);
    Serial.println();
    Serial.println("HX8357D Test!");
    //SPI.setClockSpeed(8, MHZ);
    tft.begin();
    // read diagnostics (optional but can help debug problems)
    uint8_t x = tft.readcommand8(ILI9341_RDMODE);
    Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDMADCTL);
    Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDPIXFMT);
    Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDIMGFMT);
    Serial.print("Image Format: 0x"); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDSELFDIAG);
    Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX); 
    Serial.println(F("Benchmark                Time (microseconds)"));
    delay(10);
    tft.setRotation(1);
    /* Web Setup */
    Particle.variable("lastLocation", lastLocation);
    Particle.function("reset",cloudResetFunction);
    Particle.function("switchState",switchState);
    Particle.subscribe("tagsetup", myHandler);
    
    Serial.println(F("Done!"));
}


void loop(void) {
    checkReset();
    thisPress = buttonCheck();
    switch (currentState) {
        case TAG_CHARGING:
            if (!showBattery) {
                displayBattery();
                showBattery = true;
            }
            // serialPrintTracking();   // FOR DEBUGGING
            if ((setOwnerString != "") && (setSubString != "") && (setCellString != "") && (setItemString != "") && (setStartString != "") && (setEndString != "")) {
                currentState = TAG_TRACKING;
                tft.fillScreen(ILI9341_BLACK);
                showBattery = false;
            }
            break;

        case TAG_TRACKING:
            if (thisPress == SHORT_PRESS) {
                startScreenTimer = millis();
            }
            if ( (millis() - startScreenTimer) < 10000) {   // screen on for 10 sec
                if (!showTracking) {
                    displayTracking();
                    showTracking = true;
                    Serial.println("turning on");
                }
            } else if (showTracking) {
                tft.fillScreen(ILI9341_BLACK);
                showTracking = false;
                Serial.println("turning off");
            }
            if (thisPress == MED_PRESS) {
                currentState = TAG_CHARGING;
                tft.fillScreen(ILI9341_BLACK);
                showTracking = false;
                Serial.println("switch to charging state");
                showBattery = false;
                updateStrings(true);
            }
            /* Triggered by IMU */
            /*
            String pubAccel = String::format("%d,%d,%d", t.readX(), t.readY(), t.readZ());
    		Serial.println(pubAccel);
    		if (t.readXYZmagnitude() > accelThreshold) {
        		Serial.println("moved!");
        		gpsLocation();
    		}
    		*/
    		/* Not Triggered by IMU (DEMO mode)*/
    		gpsLocation();
            break;
    }
}

void updateStrings(bool toClear) {
    if (toClear) {
        setOwnerString = "";
        setSubString = "";
        setCellString = "";
        setItemString = "";
        setStartString = "";
        setEndString = "";
    } else {
        setOwnerString = "Shenli";
        setSubString = "Stanford";
        setCellString = "6666666666";
        setItemString = "1";
        setStartString = "now";
        setEndString = "later";
    }
}

void displayBattery(){
    fuelLevel = fuel.getSoC();
    tft.setRotation(2);
    tft.setCursor(0, 0);
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);  
    tft.setTextSize(5);
    tft.println("");
    tft.println("Battery");
    tft.println("");
    tft.setTextColor(ILI9341_GREEN);  
    tft.print(" ");
    tft.print(fuelLevel);
    tft.println("%");
}

int gpsPublish(String command) {
    if (t.gpsFix()) {
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);
        lastLocation = t.readLatLon();
        // uncomment next line if you want a manual publish to reset delay counter
        // lastPublish = millis();
        return 1;
    } else {
      return 0;
    }
}

void displayTracking() {
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_WHITE);  
    tft.setTextSize(3);
    tft.print("TAG ID: ");
    tft.println(coreID);
    tft.print("OWNER: ");
    tft.println(setOwnerString);
    tft.print("SC: ");
    tft.println(setSubString);
    tft.print("CELL: ");
    tft.println(setCellString);
    tft.print("ITEM: ");
    tft.println(setItemString);
    tft.print("STRT: ");
    tft.println(setStartString);
    tft.print("END: ");
    tft.println(setEndString);
}
    
void serialPrintTracking() {
    Serial.print("currentState: ");
    Serial.println(currentState);
    Serial.print(setOwnerString);
    Serial.print(", ");
    Serial.print(setSubString);
    Serial.print(", ");
    Serial.print(setCellString);
    Serial.print(", ");
    Serial.print(setItemString);
    Serial.print(", ");
    Serial.print(setStartString);
    Serial.print(", ");
    Serial.println(setEndString);
}

/* Button debounce function */
int buttonCheck() {
    int returnVal = 0;
    current = digitalRead(BUTTON_PIN);
    if (current == HIGH && previous == LOW && (millis() - firstTime) > 200) {
        firstTime = millis();
    }
    millis_held = (millis() - firstTime);
    secs_held = millis_held / 1000;
    // Serial.println(secs_held);
    if (millis_held > 50) {
        if (current == HIGH && secs_held > prev_secs_held) {
            // Serial.println("one sec");
        }
        if (current == LOW && previous == HIGH) {
            // Button pressed for less than 1 second, one long LED blink
            if (secs_held <= 0) {
                Serial.print("current state: ");
                Serial.println(currentState);
                Serial.print("GPS fix: ");
                Serial.println(t.gpsFix());
                Serial.println("short");
                returnVal = SHORT_PRESS;
                Serial.print("OWNER: ");
                Serial.println(setOwnerString);
                Serial.print("SC: ");
                Serial.println(setSubString);
                Serial.print("CELL: ");
                Serial.println(setCellString);
                Serial.print("ITEM: ");
                Serial.println(setItemString);
                Serial.print("STRT: ");
                Serial.println(setStartString);
                Serial.print("END: ");
                Serial.println(setEndString);
            }
            // If the button was held for 3-6 seconds blink LED 10 times
            if (secs_held >= 1 && secs_held < 3) {
                Serial.println("med");
                returnVal = MED_PRESS;
            }
            // Button held for 1-3 seconds, print out some info
            if (secs_held >= 3) {
                Serial.println("long");
                returnVal = LONG_PRESS;
            }
        }
    }
    previous = current;
    prev_secs_held = secs_held;
    return returnVal;
}

/* Function to upload GPS location */
void gpsLocation() {
    // You'll need to run this every loop to capture the GPS output
    t.updateGPS();

    // if the current time - the last time we published is greater than your set delay...
    if (millis()-lastPublish > 60000) {
        // Remember when we published
        lastPublish = millis();
        String pubAccel = String::format("%d,%d,%d", t.readX(), t.readY(), t.readZ());
        Serial.println(pubAccel);
        Particle.publish("A", pubAccel, 60, PRIVATE);
        // Dumps the full NMEA sentence to serial in case you're curious
        Serial.println(t.preNMEA());
        // GPS requires a "fix" on the satellites to give good data,
        // so we should only publish data if there's a fix
        if (t.gpsFix()) {
            // Only publish if we're in transmittingData mode 1;
            if (transmittingData) {
                // Short publish names save data!
                Particle.publish("G", t.readLatLon(), 60, PRIVATE);
                lastLocation = t.readLatLon();
            }
            // but always report the data over serial for local development
            Serial.println(t.readLatLon());
        }
    }
}

/* reset debounce */
void checkReset() {
    if ((resetFlag) && (millis() - rebootSync >=  rebootDelayMillis)) {
        // do things here  before reset and then push the button
        Particle.publish("Debug","Remote Reset Initiated",300,PRIVATE);
        System.reset();
    }
}

/* Internet function : remote reset */
int cloudResetFunction(String command) {
       resetFlag = true;
       rebootSync=millis();
       return 0;            
}

/* Internet function : switch states */
int switchState(String command) {
    int inputCommand = atoi(command);
       if (inputCommand == TAG_TRACKING) {
           updateStrings(false);
           showTracking = false;
           tft.fillScreen(ILI9341_BLACK);
           currentState = TAG_TRACKING;
       }
       if (inputCommand == TAG_CHARGING) {
           showBattery = false;
           updateStrings(true);
           currentState = TAG_CHARGING;
       }
       return currentState;            
}

/* parsing information from web app */
void myHandler(const char *event, const char *data)
{
  i++;
  Serial.print(i);
  Serial.print(event);
  Serial.print(", data: ");
  Serial.println(data);
  String allData = data;
  Serial.println(allData);
  int ind12 = allData.indexOf(':');
  int ind13 = allData.indexOf(',');
  String particleID = allData.substring(ind12 + 2, ind13);
  if (particleID == coreID) {
        Serial.println("ID matched!!");
        int ind0 = allData.indexOf(':', ind12 + 1);
        int ind1 = allData.indexOf(',', ind13 + 1); 
        setOwnerString = allData.substring(ind0 + 2, ind1);
        Serial.println(setOwnerString);
        int ind2 = allData.indexOf(':', ind0 + 1);
        int ind3 = allData.indexOf(',', ind1 + 1); 
        setSubString = allData.substring(ind2 + 2, ind3);
        Serial.println(setSubString);
        int ind4 = allData.indexOf(':', ind2 + 1);
        int ind5 = allData.indexOf(',', ind3 + 1); 
        setCellString = allData.substring(ind4 + 2, ind5);
        Serial.println(setCellString);
        int ind6 = allData.indexOf(':', ind4 + 1);
        int ind7 = allData.indexOf(',', ind5 + 1); 
        setItemString = allData.substring(ind6 + 2, ind7);
        Serial.println(setItemString);
        int ind8 = allData.indexOf(':', ind6 + 1);
        int ind9 = allData.indexOf(',', ind7 + 1); 
        setStartString = allData.substring(ind8 + 2, ind9);
        Serial.println(setStartString);
        int ind10 = allData.indexOf(':', ind8 + 1);
        int ind11 = allData.indexOf(',', ind9 + 1); 
        setEndString = allData.substring(ind10 + 2, ind11);
        Serial.println(setEndString);
  }
}



