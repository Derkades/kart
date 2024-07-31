#include "comms.h"

static void process_response(Controller &controller, char *name, int16_t value) {
    // See if received name is a parameter
    for (param *param : controller.params) {
        if (strcmp(param->name, name) != 0) {
            continue; // received different parameter
        }

        if (param->value != value) {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.printf("RX %s %s val %i not match %i\n", controller.name, name, value, param->value);
            #endif
            continue; // received different value
        }

        // Correct value is set, parameter is no longer dirty
        param->dirty = false;

        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s %s OK\n", controller.name);
        #endif
        return;
    }

    // Otherwise maybe a variable
    if (strcmp(name, V_DC_CURR) == 0) {
        controller.feedbackDC_CURR = value;
    } else if (strcmp(name, V_SPD_AVG) == 0) {
        controller.feedbackSPD_AVG = value;
    } else if (strcmp(name, V_BATV) == 0) {
        controller.feedbackBATV = value;
    } else {
        // Unknown name
        Serial.printf("RX %s %s unknown\n", controller.name, name);
    }
}

// Deal with incoming data from controller, must be called in loop()
void recv(Controller &controller) {
    const uint32_t start = millis();
    char buf[CONTROL_SERIAL_RECV_BUF_SIZE];

    if (!controller.serial.available()) {
        return;
    }

    // Read a line from serial
    size_t size = controller.serial.readBytesUntil('\n', (uint8_t*) buf, CONTROL_SERIAL_RECV_BUF_SIZE);

    #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
    Serial.print("RX ");
    Serial.print(controller.name);
    Serial.print("RAW ||");
    Serial.write(buf, size);
    Serial.println("||");
    #endif

    if (size < 1 || buf[size-1] != '\r') {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR end\n", controller.name);
        #endif
        return;
    }

    // Drop \r and add terminating null byte
    buf[size-1] = '\0';

    // We are receiving data, so controller is working
    controller.lastWorking = millis();

    // Does this line contains a value reponse?
    if (strstr(buf, "# name:") == NULL) {
        Serial.printf("RX %s ignore %s\n", controller.name, buf);
        return;
    }

    char name[32];
    int16_t value;

    // Extract parameter name and value
    char *startQuote = strstr(buf, "\"");
    if (!startQuote) {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR startQuote", controller.name);
        #endif
        return;
    }

    char *endQuote = strstr(startQuote + 1, "\"");
    if (!endQuote) {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR endQuote", controller.name);
        #endif
        return;
    }

    memcpy(name, startQuote + 1, MIN(endQuote - startQuote - 1, 31));
    name[MIN(endQuote - startQuote - 1, 31)] = '\0';

    char *startValue = strstr(buf, "value:");
    if (!startValue) {
        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.printf("RX %s ERR startValue", controller.name);
        #endif
        return;
    }

    value = (int16_t) strtol(startValue + strlen("value:"), NULL, 10);

    #ifdef CONTROL_SERIAL_RX_DEBUG
    Serial.printf("RX %s name:%s val:%i\n", controller.name, value);
    #endif

    process_response(controller, name, value);
}

static void send(Controller &controller, char *command) {
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.printf("TX %s: %s\n", controller.name, command);
    #endif

    controller.serial.write(command);
}

void set(Controller &controller, const char *param, const int16_t value) {
    char command[32];
    snprintf(command, 32, "$SET %s %i\n", param, value);
    send(controller, command);
}

void get(Controller &controller, const char *param, int16_t *value_p) {
    char command[32];
    snprintf(command, 32, "$GET %s\n", param);
    send(controller, command);
}
