// -----------------------------------------------------------------------------
// Message parser for messages FROM roaster (bean temperature)
// -----------------------------------------------------------------------------

extern char CorF;

class SkyRoasterParser {
public:
    SkyRoasterParser() : debug(false) {}

    void begin(uint8_t pin);
    bool msgAvailable();
    void getMessage(uint8_t *dest);
    bool validate(const uint8_t *buf);

    // --- Debug ---
    void enableDebug(bool en) { debug = en; }

    // --- Structured Fields ---
    double getTemperature(uint8_t *buf); // in °C units

private:
    static void IRAM_ATTR edgeISR();
    void handleEdge();

    bool debug;
    int pin;

    // Protocol constants
    static const uint8_t  MSG_BYTES     = 7;
    static const uint8_t  BITS_PER_BYTE = 8;
    static const unsigned long START_MIN_US = 7000;
    static const unsigned long START_MAX_US = 10000;
    static const unsigned long BIT0_MAX_US  = 900;
    static const unsigned long BIT1_MIN_US  = 1200;
    static const unsigned long BIT1_MAX_US  = 2000;

    // State
    enum RxState { IDLE, RECEIVING };
    volatile RxState rxState = IDLE;

    volatile unsigned long lastEdgeTime = 0;
    volatile bool lastEdgeWasLow = false;

    uint8_t bitCount = 0;
    uint8_t byteIndex = 0;
    volatile uint8_t currentByte = 0;
    volatile uint8_t messageBuf[MSG_BYTES];
    volatile bool newMessage = false;

    static SkyRoasterParser *instance;
};

SkyRoasterParser* SkyRoasterParser::instance = nullptr;

void SkyRoasterParser::begin(uint8_t pin) {
    instance = this;
    rxState = IDLE;
    this->pin = pin;
    pinMode(pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pin), SkyRoasterParser::edgeISR, CHANGE);
}

bool SkyRoasterParser::msgAvailable() {
    return newMessage;
}

void SkyRoasterParser::getMessage(uint8_t *dest) {
    noInterrupts();
    for (uint8_t i = 0; i < MSG_BYTES; i++) dest[i] = messageBuf[i];
    newMessage = false;
    interrupts();
}

bool SkyRoasterParser::validate(const uint8_t *buf) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < MSG_BYTES - 1; i++) sum += buf[i];
    return (sum == buf[MSG_BYTES - 1]);
}

double SkyRoasterParser::getTemperature(uint8_t *buf) {
    // Combine the first 4 bytes into a 16-bit integer (Little Endian)
    uint16_t rawTempX = ((buf[0] << 8) + buf[1]);
    uint16_t rawTempY = ((buf[2] << 8) + buf[3]);

    double x = 0.001 * rawTempX;
    double y = 0.001 * rawTempY;
    double temperature;

    if (rawTempX > 836 || rawTempY > 221) {
        temperature = -224.2 * y * y * y + 385.9 * y * y - 327.1 * y + 171;
    } else {
        temperature = -278.33 * x * x * x + 491.944 * x * x - 451.444 * x + 310.668;
    }

    if (CorF == 'F') {
        temperature = 1.8 * temperature + 32.0;
    }
    return temperature;
}

// --- Static ISR trampoline ---
void IRAM_ATTR SkyRoasterParser::edgeISR() {
    if (instance) instance->handleEdge();
}

// --- Edge handler ---
void SkyRoasterParser::handleEdge() {
    unsigned long now = micros();
    bool pinIsLow = (digitalRead(digitalPinToInterrupt(this->pin)) == LOW);

    if (pinIsLow) {
        lastEdgeTime = now;
        lastEdgeWasLow = true;
    } else {
        if (!lastEdgeWasLow) return;
        unsigned long lowDur = now - lastEdgeTime;
        lastEdgeWasLow = false;

        if(debug) {
            D_print("Low pulse: "); D_println(lowDur);
        }

        switch (rxState) {
        case IDLE:
            if (lowDur >= START_MIN_US && lowDur <= START_MAX_US) {
                byteIndex = 0; bitCount = 0; currentByte = 0;
                rxState = RECEIVING;
                if(debug) D_println("Start detected");
            }
            break;

        case RECEIVING:
            uint8_t bitVal = 0xFF;
            if (lowDur < BIT0_MAX_US) bitVal = 0;
            else if (lowDur >= BIT1_MIN_US && lowDur <= BIT1_MAX_US) bitVal = 1;
            else { rxState = IDLE; if(debug) D_println("Invalid pulse, abort"); return; }

            currentByte |= (bitVal << bitCount);
            if(debug) D_print(bitVal); D_print(" ");

            if (++bitCount >= BITS_PER_BYTE) {
                messageBuf[byteIndex++] = currentByte;
                currentByte = 0;
                bitCount = 0;
                if(byteIndex >= MSG_BYTES) {
                    newMessage = true;
                    rxState = IDLE;
                    if(debug) D_println("\nMessage complete");
                }
            }
            break;
        }
    }
}