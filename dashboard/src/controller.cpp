#include "controller.h"

bool Controller::working() {
    return this->lastWorking && (this->lastWorking + CONTROL_SERIAL_WORKING_TIMEOUT_MS > millis());
}
