// -----------------------------------------------------------------------------
// Set SERIAL_DEBUG to 1 to enable serial output
// Trying to serial.print AND do roaster rx/tx is not compatible with s3-zero single usb
// -----------------------------------------------------------------------------
#define SERIAL_DEBUG 0 //set to 1 to turn on

// -----------------------------------------------------------------------------
// Do not change these for debugging unless you know what you're doing
// -----------------------------------------------------------------------------
#if SERIAL_DEBUG == 1
#define D_print(...)    Serial.print(__VA_ARGS__)
#define D_println(...)  Serial.println(__VA_ARGS__)
#else
#define D_print(...)
#define D_println(...)
#endif