#include "controller.h"

bool Controller::working() {
    return this->lastWorking && (this->lastWorking + CONTROL_SERIAL_WORKING_TIMEOUT_MS > millis());
}

void Controller::processResponse(char *name, int16_t value) {
    for (param *p2 : this->params) {
        Serial.printf("testloop %s\n", p2->name);
        break;
    }

    // See if received name is a parameter
    param *p = NULL;

    for (param *tryParam : this->params) {
        if (strcmp(tryParam->name, name) == 0) {
            p = tryParam;
            return;
        }

        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("test %s\n", tryParam->name);
        Serial.printf("RX %s %s not match %s\n", this->name, tryParam->name, name);
        #endif
    }

    // fallback in case of for loop weirdness
    // if (strcmp(name, P_CTRL_TYP) == 0) {
    //     p = &controller.pCtrlTyp;
    // } else if (strcmp(name, P_CTRL_MOD) == 0) {
    //     p = &controller.pCtrlMod;
    // } else if (strcmp(name, P_I_MOT_MAX) == 0) {
    //     p = &controller.pIMotMax;
    // } else if (strcmp(name, P_FI_WEAK_ENA) == 0) {
    //     p = &controller.pFiWeakEna;
    // } else if (strcmp(name, P_FI_WEAK_MAX) == 0) {
    //     p = &controller.pFiWeakMax;
    // } else if (strcmp(name, P_PHA_ADV_MAX) == 0) {
    //     p = &controller.pPhaAdvMax;
    // }

    if (p) {
        if (p->value != value) {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.printf("RX %s %s val %i not match %i\n", this->name, name, value, p->value);
            #endif
            return;
        }

        // Correct value is set, parameter is no longer dirty
        p->dirty = false;

        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s %s OK\n", this->name, p->name);
        #endif
        return;
    }

    // Otherwise maybe a variable
    if (strcmp(name, V_DC_CURR) == 0) {
        this->feedbackDC_CURR = value;
    } else if (strcmp(name, V_SPD_AVG) == 0) {
        this->feedbackSPD_AVG = value;
    } else if (strcmp(name, V_BATV) == 0) {
        this->feedbackBATV = value;
    } else {
        // Unknown name
        Serial.printf("RX %s %s unknown\n", this->name, name);
    }
}

// Deal with incoming data from controller, must be called in loop()
void Controller::recv() {
    const uint32_t start = millis();
    char buf[CONTROL_SERIAL_RECV_BUF_SIZE];

    if (!this->serial.available()) {
        return;
    }

    // Read a line from serial
    size_t size = this->serial.readBytesUntil('\n', (uint8_t*) buf, CONTROL_SERIAL_RECV_BUF_SIZE);

    #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
    Serial.print("RX ");
    Serial.print(controller.name);
    Serial.print("RAW ||");
    Serial.write(buf, size);
    Serial.println("||");
    #endif

    // Every line from the hoverboard should end with \r\n. We read up to \n, so the last character here should be \r.
    if (size < 1 || buf[size-1] != '\r') {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR end\n", this->name);
        #endif
        return;
    }

    // Drop \r and add terminating null byte, so we can use str functions
    buf[size-1] = '\0';

    // We are receiving data, so controller is working
    this->lastWorking = millis();

    // Does this line contains a value reponse?
    if (strstr(buf, "# name:") == NULL) {
        Serial.printf("RX %s ignore %s\n", this->name, buf);
        return;
    }

    char name[32];
    int16_t value;

    // Extract parameter name and value
    char *startQuote = strstr(buf, "\"");
    if (!startQuote) {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR startQuote", this->name);
        #endif
        return;
    }

    char *endQuote = strstr(startQuote + 1, "\"");
    if (!endQuote) {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR endQuote", this->name);
        #endif
        return;
    }

    memcpy(name, startQuote + 1, MIN(endQuote - startQuote - 1, 31));
    name[MIN(endQuote - startQuote - 1, 31)] = '\0';

    char *startValue = strstr(buf, "value:");
    if (!startValue) {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR startValue", this->name);
        #endif
        return;
    }

    value = (int16_t) strtol(startValue + strlen("value:"), NULL, 10);

    #ifdef CONTROL_SERIAL_RX_DEBUG
    Serial.printf("RX %s name:%s val:%i\n", this->name, name, value);
    #endif

    this->processResponse(name, value);
}

void Controller::sendCommand(char *command) {
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.printf("TX %s: %s\n", this->name, command);
    #endif

    this->serial.write(command);
}

void Controller::set(const char *param, const int16_t value) {
    char command[32];
    snprintf(command, 32, "$SET %s %i\n", param, value);
    this->sendCommand(command);
}

void Controller::get(const char *param) {
    char command[32];
    snprintf(command, 32, "$GET %s\n", param);
    this->sendCommand(command);
}
