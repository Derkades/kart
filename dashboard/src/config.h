// Vehicle parameters
#define WHEEL_CIRCUMFERENCE_MM 603
#define BAT_VOLT_EMPTY 3.3 * 12
#define BAT_VOLT_FULL 4.2 * 12

// Serial parameters
#define SERIAL_BAUD 115200

// Serial debug protocol parameters
#define CONTROL_SERIAL_BAUD 9600
#define CONTROL_SERIAL_RX_DEBUG
// #define CONTROL_SERIAL_RX_DEBUG_RAW
#define CONTROL_SERIAL_TX_DEBUG
#define CONTROL_SERIAL_WORKING_TIMEOUT_MS 30000 // TODO lower when it works
#define CONTROL_SERIAL_RECV_BUF_SIZE 64

// Menu parameters
#define SAVE_DEBUG
#define SAVE_INTERVAL 1000 // TODO lower when it works
#define HOME_REFRESH_INTERVAL 1000 // TODO lower when it works
