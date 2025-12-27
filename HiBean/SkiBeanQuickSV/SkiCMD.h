// -----------------------------------------------------------------------------
// All HiBean commands TO roaster
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// External variables
// -----------------------------------------------------------------------------
extern double temp;
extern char CorF;
extern PID myPID;
extern double pInput, pOutput, pSetpoint;
extern double Kp, Ki, Kd;
extern int pMode, pSampleTime, manualHeatLevel;

// -----------------------------------------------------------------------------
// Timing Constants
// -----------------------------------------------------------------------------
const int PULSE_ONE       = 1200;
const int PULSE_ZERO      = 650;
const int POST_PULSE_DELAY= 750;
const int START_PULSE     = 7500;
const int START_DELAY     = 3800;

// -----------------------------------------------------------------------------
// Allocate buffers
// -----------------------------------------------------------------------------
const int CONTROLLER_LENGTH = 6;   // 6 bytes sent to roaster
uint8_t sendBuffer[CONTROLLER_LENGTH];

// -----------------------------------------------------------------------------
// Control Byte Indices
// -----------------------------------------------------------------------------
enum ControlBytes {
    VENT_BYTE = 0,
    DRUM_BYTE = 3,
    COOL_BYTE = 2,
    FILTER_BYTE = 1,
    HEAT_BYTE = 4,
    CHECK_BYTE = 5
};

// -----------------------------------------------------------------------------
// Command Strings(why declared if not used?)
// https://github.com/greencardigan/TC4-shield/blob/master/applications/Artisan/aArtisan/trunk/src/aArtisan/commands.txt
// -----------------------------------------------------------------------------
const String CMD_READ         = "READ";
const String CMD_HEAT         = "OT1";
const String CMD_VENT         = "OT2";
const String CMD_OFF          = "OFF";
const String CMD_DRUM         = "DRUM";
const String CMD_FILTER       = "FILTER";
const String CMD_COOL         = "COOL";
const String CMD_CHAN         = "CHAN";
const String CMD_UNITS        = "UNITS";

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
void handleCHAN();
void handleOT1(uint8_t value);
void handleREAD();
void handleHEAT(uint8_t value);
void handleVENT(uint8_t value);
void handleDRUM(uint8_t value);
void handleFILTER(uint8_t value);
void handleCOOL(uint8_t value);
void eStop();
void handlePIDControl();
void setPIDMode(bool usePID);
void setValue(uint8_t* bytePtr, uint8_t value);
void sendRoasterMessage();

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------
unsigned long lastEventTime = 0;      // marker for last time we got HiBean message
const unsigned long LAST_EVENT_TIMEOUT = 10UL * 1000000UL; // 10 seconds (micros)

bool itsbeentoolong() {
  unsigned long now = micros();
  unsigned long duration = now - lastEventTime;
  return (duration > LAST_EVENT_TIMEOUT);
}

void shutdown() {
    for (int i = 0; i < CONTROLLER_LENGTH; i++) {
        sendBuffer[i] = 0;
    }
    sendBuffer[CHECK_BYTE] = 0; // Reset checksum
}

// -----------------------------------------------------------------------------
// Command handlers
// -----------------------------------------------------------------------------
void handleCHAN() {
    String message = "# Active channels set to 2100\n";
    D_println(message);
    notifyBLEClient(message);
}

void handleOT1(uint8_t value) {
    if (myPID.GetMode() == MANUAL) {
      manualHeatLevel = constrain(value, 0, 100); // Set manual heat level
      handleHEAT(manualHeatLevel); // Apply the new setting
    } else if (myPID.GetMode() == AUTOMATIC) {
      setPIDMode(false); // Disable PID control
    }
}

void handleREAD() {
    String readMsg = "0," + String(temp, 1) + "," + String(temp, 1) + "," +
          String(sendBuffer[HEAT_BYTE]) + "," +
          String(sendBuffer[VENT_BYTE]) + "\n";
    //D_print("READ Output: ");
    //D_println(readMsg);

    notifyBLEClient(readMsg);
    sendRoasterMessage(); // send heartbeat message to roaster
    lastEventTime = micros();
}

void handleHEAT(uint8_t value) {
    if (value <= 100) {
        setValue(&sendBuffer[HEAT_BYTE], value);
    }
    sendRoasterMessage();
    lastEventTime = micros();
}

void handleVENT(uint8_t value) {
    if (value <= 100) {
        setValue(&sendBuffer[VENT_BYTE], value);
        if (value == 0) {
            handleFILTER(value); // off
        } else {
            handleFILTER((int) round(4-((value-1)*4/100))); //convert 0-100 to inverted 4-1
        }
    }
    sendRoasterMessage();
    lastEventTime = micros();
}

void handleDRUM(uint8_t value) {
    if (value != 0) {
        setValue(&sendBuffer[DRUM_BYTE], 100);
    } else {
        setValue(&sendBuffer[DRUM_BYTE], 0);
    }
    sendRoasterMessage();
    lastEventTime = micros();
}

void handleFILTER(uint8_t value) {
    if (value >= 0 && value <= 4 ) {
        setValue(&sendBuffer[FILTER_BYTE], value); //0 off; 1 fastest -> 4 slowest
    }
    sendRoasterMessage();
    lastEventTime = micros();
}

void handleCOOL(uint8_t value) {
    if (value <= 100) {
        setValue(&sendBuffer[COOL_BYTE], value);
        handleFILTER(value);
    }
    sendRoasterMessage();
    lastEventTime = micros();
}

void eStop() {
    D_println("Emergency Stop Activated! Heater OFF, Vent 100%");
    handleHEAT(0);   // Turn off heater
    handleVENT(100); // Set vent to 100%
}

//PID hControls///
//adjusting the heating power based on PID temperature control
void handlePIDControl() {
    if (myPID.GetMode() == AUTOMATIC) {
        pInput = temp; // give current temperature as input to pid model
        myPID.Compute();
        int roundedHeat = std::round(pOutput / 5.0) * 5;
        handleHEAT(roundedHeat);
    } else {
        handleHEAT(manualHeatLevel);  // Use stored manual heat level
    }
}

void setPIDMode(bool usePID) {
    if (usePID) {
        myPID.SetMode(AUTOMATIC); // Enable PID
        D_println("PID mode set to AUTOMATIC");
    } else {
        myPID.SetMode(MANUAL); // Disable PID
        manualHeatLevel = 0;  // Set heat to 0% for safety
        handleHEAT(manualHeatLevel); // Apply the change immediately
        D_println("PID mode set to MANUAL");
    }
}

void parseAndExecuteCommands(String input) {
    input.trim();
    input.toUpperCase();
 
    //D_println("Parsing command: " + input);

    int split1 = input.indexOf(';');
    String command = "";
    String param = "";
    String subcommand = "";
    
    if (split1 >= 0) {
        command = input.substring(0, split1);
        String remainder = input.substring(split1 + 1);
        int split2 = remainder.indexOf(';');

        if (split2 >= 0) {
            subcommand = remainder.substring(0, split2);
            param = remainder.substring(split2 + 1);
        } else {
            param = remainder;
        }
    } else {
        command = input;
    }

    if (command == "PID") {
        if (param == "ON") {
            setPIDMode(true);  // Enable PID control
        } else if (param == "OFF") {
            setPIDMode(false); // Disable PID control
        } else if (subcommand == "SV") {
            double newSetpoint = param.toDouble();
            if (newSetpoint > 0 && newSetpoint <= 300) {  // Example range check
                pSetpoint = newSetpoint;
                D_print("New Setpoint: ");
                D_println(pSetpoint);
            }
        } else if (subcommand == "T") {
            double pidTune[3]; //pp.p;ii.i;dd.d
            int paramCount = 0;
            while(param.length() > 0) {
                int index = param.indexOf(';');
                if (index == -1) { // No delim found
                    pidTune[paramCount++] = param.toDouble(); //remaining string
                    break;
                } else {
                    pidTune[paramCount++] = param.substring(0, index).toDouble(); //grab first
                    param = param.substring(index+1); //trim string
                }
            }
            Kp = pidTune[0];
            Ki = pidTune[1];
            Kd = pidTune[2];
            
            D_print("Kp: "); D_println(Kp); D_print("Ki: "); D_println(Ki); D_print("Kd: "); D_println(Kd);
            myPID.SetTunings(Kp, Ki, Kd, pMode); // apply the pid params to running config
        } else if (subcommand == "PM") {
            D_print("Setting PMode to: ");
            D_println(param);
            if (param == "M") {
              pMode = P_ON_M;
              myPID.SetTunings(Kp, Ki, Kd, pMode); // apply the pid params to running config
            } else {
              pMode = P_ON_E;
              myPID.SetTunings(Kp, Ki, Kd, pMode); // apply the pid params to running config
            }
        } else if (subcommand == "CT") {
            D_print("Setting Cycle Time to: ");
            D_println(param.toDouble());
            myPID.SetSampleTime(pSampleTime);
        }
    } else if (command == "OT1") {  
        D_println("Setting OT1: " + param);
        handleOT1(param.toInt());  // Manual heater control (only in MANUAL mode)
    } else if (command == "READ") {
        handleREAD();
    } else if (command == "OT2") { 
        D_println("Setting OT2: " + param); 
        handleVENT(param.toInt());  // Set fan duty
    } else if (command == "OFF") {  
        shutdown();  // Shut down system
    } else if (command == "ESTOP") {  
        eStop();  // Emergency stop (heater = 0, vent = 100)
    } else if (command == "DRUM") {  
        D_println("Setting Drum: " + param); 
        handleDRUM(param.toInt());  // Start/stop the drum
    } else if (command == "FILTER") { 
        D_println("Setting Filter: " + param);  
        handleFILTER(param.toInt());  // Turn on/off filter fan
    } else if (command == "COOL") {  
        D_println("Setting Cool: " + param);  
        handleCOOL(param.toInt());  // Cool the beans
    } else if (command == "CHAN") {  
        handleCHAN();  // Handle TC4 init message
    } else if (command == "UNITS") {  
        if (split1 >= 0) CorF = input.charAt(split1 + 1);  // Set temperature units
    }
}

void pulsePin(int pin, int duration) {
  #if SERIAL_DEBUG == 0
    digitalWrite(pin, LOW);
    delayMicroseconds(duration);
    digitalWrite(pin, HIGH);
  #endif
}

// Control Bytes & Checksum
void setControlChecksum() {
    uint8_t sum = 0;
    for (int i = 0; i < (CONTROLLER_LENGTH - 1); i++) {
        sum += sendBuffer[i];
    }
    sendBuffer[CHECK_BYTE] = sum;  // Correct use of CHECK_BYTE
}

void setValue(uint8_t* bytePtr, uint8_t value) {
    *bytePtr = value;
    setControlChecksum();
}

void extern sendRoasterMessage() {
    // Start pulse
    pulsePin(TX_PIN, START_PULSE);
    delayMicroseconds(START_DELAY);

    // Send each byte, bit by bit
    for (int i = 0; i < CONTROLLER_LENGTH; i++) {
        for (int j = 0; j < 8; j++) {
            if (bitRead(sendBuffer[i], j) == 1) {
                // '1' bit
                pulsePin(TX_PIN, 1500);
            } else {
                // '0' bit
                pulsePin(TX_PIN, PULSE_ZERO);
            }
            delayMicroseconds(POST_PULSE_DELAY);
        }
    }
}