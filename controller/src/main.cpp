#include "main.h"

#ifdef HOVER_SERIAL_FRONT
HardwareSerial HoverSerialFront(PA10, PA9); // USART1
#endif
#ifdef HOVER_SERIAL_REAR
HardwareSerial HoverSerialRear(PA3, PA2); // USART2
#endif

typedef struct {
    uint16_t start;
    int16_t  steer;
    int16_t  speed;
    uint16_t checksum;
} SerialCommand;

void setCommand(SerialCommand *command, int16_t steer, int16_t speed) {
    command->start = (int16_t) START_FRAME;
    command->steer = steer;
    command->speed = speed;
    command->checksum = command->start ^ command->steer ^ command->speed;
}

uint16_t flashInterval;
int16_t speedFeedback;
uint32_t lastSpeedFeedback;

void setup() {
    SerialUSB.begin();

    pinMode(PC13, OUTPUT);

    #ifdef HOVER_SERIAL_FRONT
    HoverSerialFront.begin(HOVER_SERIAL_BAUD);
    #endif

    #ifdef HOVER_SERIAL_REAR
    HoverSerialRear.begin(HOVER_SERIAL_BAUD);
    #endif

    pinMode(PIN_ACCEL_INPUT, INPUT);
    #ifdef PIN_BRAKE_INPUT
    pinMode(PIN_BRAKE_INPUT, INPUT);
    #endif

    #ifdef PIN_BALANCE_INPUT
    pinMode(PIN_BALANCE_INPUT, INPUT);
    #endif

    #ifdef PIN_REVERSE_INPUT
    pinMode(PIN_REVERSE_INPUT, INPUT_PULLUP);
    #endif
}

#ifdef HOVER_SERIAL_RX
typedef struct {
    uint16_t start;
    int16_t  cmd1;
    int16_t  cmd2;
    int16_t  speedR_meas;
    int16_t  speedL_meas;
    int16_t  batVoltage;
    int16_t  boardTemp;
    uint16_t cmdLed;
    uint16_t checksum;
} SerialRx;

typedef struct {
    uint8_t i = 0; // Index for new data pointer
    byte *p; // Pointer declaration for the new received byte. Probably not necessary, can just add index?
    byte prevByte; // For two-byte start marker detection
} SerialRxTemp;

SerialRx rxFront;
SerialRx rxRear;
SerialRxTemp rxTempFront;
SerialRxTemp rxTempRear;

void receive(const char *name, HardwareSerial *serial, SerialRx *rx, SerialRxTemp *rxTemp) {
    // Check for new data availability in the Serial buffer
    if (!serial->available()) {
        return;
    }

    byte *rxp = (byte*) rx; // Pointer to start byte of rx struct
    byte incomingByte = serial->read(); // Read the incoming byte
    uint16_t bufStartFrame = ((uint16_t) (incomingByte) << 8) | rxTemp->prevByte; // Construct the start frame

    // Copy received data
    if (bufStartFrame == START_FRAME) {	// Initialize if new data is detected
        rxTemp->i = 0;
        *(rxp + rxTemp->i++) = rxTemp->prevByte;
        *(rxp + rxTemp->i++) = incomingByte;
    } else if (rxTemp->i >= 2 && rxTemp->i < sizeof(SerialRx)) { // Save the new received data
        *(rxp + rxTemp->i++) = incomingByte;
    }

    // Check if we reached the end of the message
    if (rxTemp->i == sizeof(SerialRx)) {
        uint16_t checksum = rx->start ^ rx->cmd1 ^ rx->cmd2 ^ rx->speedR_meas ^ rx->speedL_meas ^ rx->batVoltage ^ rx->boardTemp ^ rx->cmdLed;

        // Check validity of the new data
        if (checksum == rx->checksum) {
            // Print data to built-in Serial
            speedFeedback = rx->speedL_meas + rx->speedR_meas / 2;
            lastSpeedFeedback = millis();
            #ifdef HOVER_SERIAL_RX_DEBUG
            Serial.print(name);
            Serial.print(" cmd1 ");   Serial.print(rx->cmd1);
            Serial.print(" cmd2 ");   Serial.print(rx->cmd2);
            Serial.print(" speedR "); Serial.print(rx->speedR_meas);
            Serial.print(" speedL "); Serial.print(rx->speedL_meas);
            Serial.print(" volt ");   Serial.print(rx->batVoltage);
            Serial.print(" temp ");   Serial.print(rx->boardTemp);
            Serial.print(" led ");    Serial.println(rx->cmdLed);
            #endif
        } else {
            #ifdef HOVER_SERIAL_RX_DEBUG
            Serial.println("Non-valid data skipped");
            #endif
        }

        // Reset the index (it prevents to enter in this if condition in the next cycle)
        rxTemp->i = 0;
    }

    // Update previous states
    rxTemp->prevByte = incomingByte;
}

void receiveAll() {
    #ifdef HOVER_SERIAL_FRONT
    receive("front", &HoverSerialFront, &rxFront, &rxTempFront);
    #endif
    #ifdef HOVER_SERIAL_REAR
    receive("rear", &HoverSerialRear, &rxRear, &rxTempRear);
    #endif
}
#endif

void sendSerial() {
    #ifdef PIN_BRAKE_INPUT
        const uint32_t brakeRaw = analogRead(PIN_BRAKE_INPUT);
        const int16_t brake = constrain(map(brakeRaw, INPUT_BRAKE_MIN, INPUT_BRAKE_MAX, 0, 1000), 0, 1000);
    #else
        const uint32_t brakeRaw = 0;
        const int16_t brake = 0;
    #endif

    const uint32_t accelRaw = analogRead(PIN_ACCEL_INPUT);
    const int16_t accel = constrain(map(accelRaw, INPUT_ACCEL_MIN, INPUT_ACCEL_MAX, 0, 1000), 0, 1000);

    int16_t speedCmd = 0;

    #ifdef PIN_REVERSE_INPUT
    const bool reverse = digitalRead(PIN_REVERSE_INPUT);
    if (lastSpeedFeedback < millis() - 500) {
        // Error
        Serial.println("error");
        speedCmd = 0;
    } else {
        speedCmd = accel - brake*2;
        if (reverse) {
            if (speedCmd >= 0) {
                // Should accelerate, meaning backwards speed
                speedCmd = -speedCmd;
            } else {
                // Should brake
                if (speedFeedback > 10) {
                    // Should send negative speed, no action needed
                } else if (speedFeedback < -10) {
                    speedCmd = -speedCmd;
                } else {
                    // Almost standstill, no action needed
                    speedCmd = 0;
                }
            }
        } else {
            if (speedCmd >= 0) {
                // Should accelerate, no action needed
            } else {
                // Should brake
                if (speedFeedback > 10) {
                    // Should send negative speed, no action needed
                } else if (speedFeedback < -10) {
                    speedCmd = -speedCmd;
                } else {
                    // Almost standstill, no action needed
                    speedCmd = 0;
                }
            }
        }
    }
    #else
    speedCmd = brake
            ? -map(brake, 0, 1000, 0, SPEED_BACKWARD_MAX)
            : map(accel, 0, 1000, 0, SPEED_FORWARD_MAX);
    #endif

    speedCmd = constrain(speedCmd, -SPEED_BACKWARD_MAX, SPEED_FORWARD_MAX);

    flashInterval = map(1000 - abs(speedCmd), 0, 1000, 25, 250);

    #ifdef INPUT_DEBUG
        Serial.print("accel ");
        Serial.print(accelRaw);
        Serial.print("->");
        Serial.print(accel);

        #ifdef PIN_BRAKE_INPUT
            Serial.print(" | brake ");
            Serial.print(brakeRaw);
            Serial.print("->");
            Serial.print(brake);
        #else
            Serial.print(" | brake disabled");
        #endif

        #ifdef PIN_REVERSE_INPUT
        Serial.print(" | reverse ");
        Serial.print(reverse);
        #endif

        Serial.print(" | speedCmd ");
        Serial.print(speedCmd);
    #endif

    #ifdef PIN_BALANCE_INPUT
    const uint32_t balanceRaw = analogRead(PIN_BALANCE_INPUT);
    const int16_t balance = constrain(map(balanceRaw, INPUT_BALANCE_MIN, INPUT_BALANCE_MAX, -BALANCE_CONST, BALANCE_CONST), -BALANCE_CONST, BALANCE_CONST);
    int16_t balanceFront, balanceRear;
    if (balance > 0) {
        balanceFront = BALANCE_CONST;
        balanceRear = BALANCE_CONST - balance;
    } else {
        balanceFront = BALANCE_CONST + balance;
        balanceRear = BALANCE_CONST;
    }
    Serial.print(" | balanceFront ");
    Serial.print(balanceFront);
    Serial.print(" | balanceRear ");
    Serial.print(balanceRear);
    #else
    const int16_t balanceFront = BALANCE_CONST, balanceRear = BALANCE_CONST;
    #endif

    SerialCommand command;

    #ifdef HOVER_SERIAL_FRONT
        int16_t speedCmdFront = (speedCmd * balanceFront) / BALANCE_CONST;
        #ifdef INPUT_DEBUG
            Serial.print(" | speedCmdFront ");
            Serial.print(speedCmdFront);
        #endif
        setCommand(&command, 0, speedCmdFront);
        HoverSerialFront.write((uint8_t *) &command, sizeof(SerialCommand));
    #endif

    #ifdef HOVER_SERIAL_REAR
        int16_t speedCmdRear = (speedCmd * balanceRear) / BALANCE_CONST;
        #ifdef INPUT_DEBUG
            Serial.print(" | speedCmdRear ");
            Serial.print(speedCmdRear);
        #endif
        setCommand(&command, 0, (speedCmdRear * speedCmd) / ADC_RESOLUTION);
        HoverSerialRear.write((uint8_t *) &command, sizeof(SerialCommand));
    #endif

    Serial.print(" | speedFeedback ");
    Serial.print(speedFeedback);

    Serial.println();
}

unsigned long lastSendTime = 0;
unsigned long lastFlashTime = 0;
uint32_t lastFlashState = 0;

void loop(void) {
    unsigned long timeNow = millis();

    #ifdef HOVER_SERIAL_RX
    receiveAll();
    #endif

    // Flash LED
    if (timeNow - lastFlashTime > flashInterval) {
        lastFlashTime = timeNow;
        lastFlashState = lastFlashState == HIGH ? LOW : HIGH;
        digitalWrite(PC13, lastFlashState);
    }

    if (timeNow - lastSendTime > SEND_INTERVAL) {
        lastSendTime = timeNow;
        sendSerial();
    }
}
