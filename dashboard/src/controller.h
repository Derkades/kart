#ifndef CONTROLLER_H
#define CONTROLLER_H
#ifdef __cplusplus

#include <Arduino.h>
#include "config.h"

class Controller {
    public:
        Controller(const char *name, uint32_t rx, uint32_t tx) : serial(rx, tx), name{name} {}
        const char *name;
        HardwareSerial serial;
        uint32_t lastWorking;
        int16_t feedbackSPD_AVG; // average motor speed RPM
        int16_t feedbackBATV; // battery voltage * 100
        int16_t feedbackDC_CURR; // total DC link current A * 100

        bool working() {
            return lastWorking && (lastWorking + CONTROL_SERIAL_WORKING_TIMEOUT_MS > millis());
        }
};

#endif
#endif
