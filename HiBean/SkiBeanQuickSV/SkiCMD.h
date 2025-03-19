// -----------------------------------------------------------------------------
// External variables
// -----------------------------------------------------------------------------
extern PID myPID;
extern double pInput, pOutput, pSetpoint;
extern double Kp, Ki, Kd;
extern int pMode, pSampleTime, manualHeatLevel;

// -----------------------------------------------------------------------------
// Command Strings
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
    String message = "# Active channels set to 2100\r\n";
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
          String(sendBuffer[VENT_BYTE]) + "\r\n";
    //D_print("READ Output: ");
    //D_println(readMsg);

    notifyBLEClient(readMsg);
    lastEventTime = micros();
}

void handleHEAT(uint8_t value) {
    if (value <= 100) {
        setValue(&sendBuffer[HEAT_BYTE], value);
    }
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
    lastEventTime = micros();
}

void handleDRUM(uint8_t value) {
    if (value != 0) {
        setValue(&sendBuffer[DRUM_BYTE], 100);
    } else {
        setValue(&sendBuffer[DRUM_BYTE], 0);
    }
    lastEventTime = micros();
}

void handleFILTER(uint8_t value) {
    if (value >= 0 && value <= 4 ) {
        setValue(&sendBuffer[FILTER_BYTE], value); //0 off; 1 fastest -> 4 slowest
    }
    lastEventTime = micros();
}

void handleCOOL(uint8_t value) {
    if (value <= 100) {
        setValue(&sendBuffer[COOL_BYTE], value);
        handleFILTER(value);
    }
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
        handleHEAT((int) pOutput);
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
        } else if (subcommand == "KP") {
            D_print("Setting KP to: ");
            D_println(param.toDouble());
            Kp = param.toDouble();
            myPID.SetTunings(Kp, Ki, Kd, pMode); // apply the pid params to running config
        } else if (subcommand == "KI") {
            D_print("Setting KI to: ");
            D_println(param.toDouble());
            Ki = param.toDouble();
            myPID.SetTunings(Kp, Ki, Kd, pMode); // apply the pid params to running config
        } else if (subcommand == "KD") {
            D_print("Setting KD to: ");
            D_println(param.toDouble());
            Kd = param.toDouble();
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
        } else if (subcommand == "ST") {
            D_print("Setting Sample Time to: ");
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