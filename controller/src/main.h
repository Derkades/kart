#include <Arduino.h>

// Pinout
#define PIN_ACCEL_INPUT PA0
#define PIN_BRAKE_INPUT PA1
// #define PIN_REVERSE_INPUT PA2

// Can only be used in torque mode, otherwise wheels will turn at different speeds
// #define PIN_BALANCE_INPUT PB0

/*
 * Connect RX to TX on hoverboard, left sideboard connector next to positive
 * Connect TX to RX on hoverboard, left sideboard connector next to negative
 * RX=red/brown TX=green/blue
 */

// Calibrate analog inputs
#define INPUT_DEBUG
#define INPUT_ACCEL_MIN     300
#define INPUT_ACCEL_MAX     670
#define INPUT_BRAKE_MIN     300
#define INPUT_BRAKE_MAX     800
#define INPUT_BALANCE_MIN   3
#define INPUT_BALANCE_MAX   1020

// Safety limit
#define SPEED_FORWARD_MAX   1000
#define SPEED_BACKWARD_MAX  500

// Hoverboard firmware has 5 ms loop, so any faster does not make sense.
#define SEND_INTERVAL       50
// At 8 bytes per message with 19200 baud max 300 messages per second can be sent (every 3ms). Hoverboard firmware has 5ms loop so faster does not make sense. Arduino firmware has fixed 10ms loop.
#define HOVER_SERIAL_BAUD   9600        // Baud rate for HoverSerial (used to communicate with the hoverboard)
#define HOVER_SERIAL_RX                 // Receive data from HoverSerial
// #define HOVER_SERIAL_RX_DEBUG
#define HOVER_SERIAL_FRONT
// #define HOVER_SERIAL_REAR
#define START_FRAME         0xABCD     	// Start frame definition for reliable serial communication

#define BALANCE_CONST 16 // 1000 * 16 just fits in int16_t

#if defined(PIN_REVERSE_INPUT) && !defined(HOVER_SERIAL_RX)
    #error Serial RX must be enabled to use reverse input
#endif
