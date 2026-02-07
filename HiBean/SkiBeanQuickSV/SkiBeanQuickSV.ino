/***************************************************
 * HiBean ESP32 BLE Roaster Control
 *
 * Libraries Required: PID 1.2.0
 ***************************************************/

#include <Arduino.h>
#include <PID_v1.h>
#include "SkiPinDefns.h"
#include "SerialDebug.h"
#include "SkiBLE.h"
#include "SkiLED.h"
#include "SkiCMD.h"
#include "SkiParser.h"

// -----------------------------------------------------------------------------
// Current Sketch and Release Version (for BLE device info)
// -----------------------------------------------------------------------------
String firmWareVersion = String("1.1.7");
String sketchName = String(__FILE__).substring(String(__FILE__).lastIndexOf('/')+1);

// -----------------------------------------------------------------------------
// Global Bean Temperature Variable
// -----------------------------------------------------------------------------
double temp          = 0.0; // temperature
char CorF = 'C';            // default units

// -----------------------------------------------------------------------------
// Instantiate Parser for read messages from roaster
// -----------------------------------------------------------------------------
SkyRoasterParser roaster;

// -----------------------------------------------------------------------------
// Track BLE writes from HiBean
// -----------------------------------------------------------------------------
std::queue<String> messageQueue;  // Holds commands written by Hibean to us

// -----------------------------------------------------------------------------
// Define PID variables
// -----------------------------------------------------------------------------
double pInput, pOutput;
double pSetpoint = 0.0; // Desired temperature (adjustable on the fly)
int pMode = P_ON_M; // http://brettbeauregard.com/blog/2017/06/introducing-proportional-on-measurement/
double Kp = 9.5, Ki = 0.3, Kd = 3.0; // pid calibrations for pMode (adjustable on the fly)
int pSampleTime = 500; //ms (adjustable on the fly)
int manualHeatLevel = 50;
PID myPID(&pInput, &pOutput, &pSetpoint, Kp, Ki, Kd, pMode, DIRECT);  //pid instance with our default values

void setup() {
    Serial.begin(115200);
    D_println("Starting HiBean ESP32 BLE Roaster Control.");
    delay(3000); //let fw upload finish before we take over hwcdc serial tx/rx

    D_println("Serial SERIAL_DEBUG ON!");

    // set pinmode on tx for commands to roaster, take it high
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, HIGH);

    // start parser on rx pin for bean temp readings from roaster
    roaster.begin(RX_PIN);
    roaster.enableDebug(false);

    // Start BLE
    initBLE();

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

    // roaster message found, go get it, validate and update temp
    if(roaster.msgAvailable()) {
        uint8_t msg[7];
        roaster.getMessage(msg);

        if(roaster.validate(msg)) {
            temp = roaster.getTemperature(msg);
        } else {
            D_println("Checksum failed!");
        }
    }

    // process incoming ble commands from HiBean, could be read or write
    while (!messageQueue.empty()) {
        String msg = messageQueue.front(); //grab the first one
        messageQueue.pop(); //remove it from the queue
        parseAndExecuteCommands(msg);  // process the command it
    }

    // Ensure PID or manual heat control is handled
    handlePIDControl();
    
    // update the led so user knows we're running
    handleLED();
}
