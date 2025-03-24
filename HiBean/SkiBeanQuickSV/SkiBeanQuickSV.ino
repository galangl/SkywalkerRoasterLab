/***************************************************
 * HiBean ESP32 BLE Roaster Control
 *
 * Libraries Required: MedianFilterLib 1.0.1, PID 1.2.0
 ***************************************************/

#include <Arduino.h>
#include <MedianFilterLib.h>
#include <PID_v1.h>
#include "SkiPinDefns.h"
#include "SerialDebug.h"
#include "SkiBLE.h"
#include "SkiComms.h"
#include "SkiLED.h"
#include "SkiCMD.h"

// -----------------------------------------------------------------------------
// Current Sketch and Release Version (for BLE)
// -----------------------------------------------------------------------------
String firmWareVersion = String("1.0.1");
String sketchName = String(__FILE__).substring(String(__FILE__).lastIndexOf('/')+1);

// -----------------------------------------------------------------------------
// Global Bean Temperature Variable
// -----------------------------------------------------------------------------
double temp          = 0.0;           // Filtered temperature

// -----------------------------------------------------------------------------
// Define PID variables
// -----------------------------------------------------------------------------
double pInput, pOutput;
double pSetpoint = 0.0; // Desired temperature (adjustable on the fly)
int pMode = P_ON_M; // http://brettbeauregard.com/blog/2017/06/introducing-proportional-on-measurement/
// was 20,1,3 at 1sec
double Kp = 20.0, Ki = 0.5, Kd = 4.0; // pid calibrations (adjustable on the fly)
int pSampleTime = 1000; //ms (adjustable on the fly)
int manualHeatLevel = 50;
PID myPID(&pInput, &pOutput, &pSetpoint, Kp, Ki, Kd, pMode, DIRECT);  //pid instance with our default values

void setup() {
    rgbLedWrite(LED_PIN, LED_GREEN[0], LED_GREEN[1], LED_GREEN[2]);
    Serial.begin(115200);
    D_println("Starting HiBean ESP32 BLE Roaster Control.");
    delay(3000); //let fw upload finish before we take over hwcdc serial tx/rx

    D_println("Serial SERIAL_DEBUG ON!");
    
    #if SERIAL_DEBUG == 0
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, HIGH);
    pinMode(RX_PIN, INPUT);
    #endif

    initBLE();
	
    // interrupt that flags when roaster preamble is found
    attachInterrupt(RX_PIN, watchRoasterStart, FALLING);

    // Set PID to start in MANUAL mode
    myPID.SetMode(MANUAL);

    // clamp output limits to 0-100(% heat), set sample interval 
    myPID.SetOutputLimits(0.0,100.0);
    myPID.SetSampleTime(pSampleTime);

    // Ensure heat starts at 0% for safety
    manualHeatLevel = 0;
    handleHEAT(manualHeatLevel);

    shutdown();
}

void loop() {
    // roaster shut down, clear our buffers   
    if (itsbeentoolong()) { shutdown(); }

    // roaster message start found, go get it
    if (roasterStartFound) { getRoasterMessage(); }

    // send roaster commands if any
    sendRoasterMessage();

    // Ensure PID or manual heat control is handled
    handlePIDControl();
    
    // update the led so user knows we're running
    handleLED();
}
