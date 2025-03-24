// -----------------------------------------------------------------------------
// LED handler
// -----------------------------------------------------------------------------
const extern unsigned int LED_BLUE[3] = { 0, 0, 10 };
const extern unsigned int LED_RED[3] = { 0, 10, 0 };
const extern unsigned int LED_GREEN[3] = { 10, 0, 0 };
const extern unsigned int LED_BLACK[3] = { 0, 0, 0 };

String currentLEDColor = "blue";
unsigned long LED_LAST_ON_MS = 0;
const unsigned long LED_FLASH_DELAY_MS = 1000;

void extern handleLED(void);

// alternates blue-red on boot when no client connected
// when client conncted just flashes blue
void handleLED() {
  unsigned long t_now = millis();
  // flash led depending on state
  if( (t_now - LED_LAST_ON_MS) >= LED_FLASH_DELAY_MS ) {
    if(currentLEDColor == "blue" && deviceConnected) {
      rgbLedWrite(LED_PIN, LED_BLACK[0], LED_BLACK[1], LED_BLACK[2]);
      currentLEDColor = "black";
    } else if (currentLEDColor == "blue") {
      rgbLedWrite(LED_PIN, LED_RED[0], LED_RED[1], LED_RED[2]);
      currentLEDColor = "red";
      LED_LAST_ON_MS = t_now;
    } else {
      rgbLedWrite(LED_PIN, LED_BLUE[0], LED_BLUE[1], LED_BLUE[2]);
      currentLEDColor = "blue";
      LED_LAST_ON_MS = t_now;
    }
  }
}