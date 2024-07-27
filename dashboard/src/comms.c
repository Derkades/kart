#include "comms.h"

// waits for OK response for a specific parameter/value
static bool recv(Controller &controller, const char *param, int16_t *value_p) {
    const uint32_t start = millis();
    char buf[CONTROL_SERIAL_RECV_BUF_SIZE];

    while (start + CONTROL_SERIAL_TIMEOUT_MS > millis()) {
        // Wait for serial to be available
        if (!controller.serial.available()) {
            continue;
        }

        // Read a line from serial and add trailing null byte
        size_t size = controller.serial.readBytesUntil('\n', (uint8_t*) buf, CONTROL_SERIAL_RECV_BUF_SIZE - 1);
        buf[size] = '\0';

        #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
        Serial.print("\nRX_raw:");
        Serial.println(buf);
        #endif

        // This line contains a value reponse?
        if (memcmp("# name:", buf, MIN(7, size)) == 0) {
            char name[32];
            int16_t value;

            // Extract parameter name and value
            char *startQuote = strstr(buf, "\"");
            if (!startQuote) {
                Serial.println(" RX_err_startQuote");
                continue;
            }
            char *endQuote = strstr(startQuote + 1, "\"");
            if (!endQuote) {
                Serial.println(" RX_err_endQuote");
                continue;
            }
            memcpy(name, startQuote + 1, MIN(endQuote - startQuote - 1, 31));
            name[MIN(endQuote - startQuote - 1, 31)] = '\0';

            char *startValue = strstr(buf, "value:");
            if (!startValue) {
                Serial.println(" RX_err_startValue");
                continue;
            }

            value = (int16_t) strtol(startValue + strlen("value:"), NULL, 10);

            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.print(" RX_name:");
            Serial.print(name);
            Serial.print(" RX_val:");
            Serial.print(value);
            #endif

            // Is this the value we were looking for?
            if (strcmp(name, param) != 0) {
                #ifdef CONTROL_SERIAL_RX_DEBUG
                Serial.print(" RX_ignore");
                #endif
                continue;
            }

            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.print(" RX_ok");
            #endif

            if (value_p != NULL) {
                *value_p = value;
            }

            controller.lastWorking = millis();

            return true;
        }
    }

    #ifdef CONTROL_SERIAL_RX_DEBUG
    Serial.print(" RX_timeout");
    #endif
    return false; // timeout
}

static void send(Controller &controller, char *command) {
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.print("send ");
    Serial.print(controller.name);
    Serial.print(": ");
    Serial.write((uint8_t*) command, strlen(command) - 1);  // send command to debug serial, excluding newline
    #endif

    controller.serial.write(command);
}

bool set(Controller &controller, const char *param, const int16_t value) {
    char command[32];
    snprintf(command, 32, "$SET %s %i\n", param, value);

    send(controller, command);

    bool success = recv(controller, param, NULL);
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.println(success ? " OK" : " FAIL");
    #endif
    return success;
}

bool get(Controller &controller, const char *param, int16_t *value_p) {
    char command[32];
    snprintf(command, 32, "$GET %s\n", param);

    send(controller, command);

    bool success = recv(controller, param, value_p);
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.println(success ? " OK" : " FAIL");
    #endif
    return success;
}

static void getBlocking(Controller &controller, const char *param, int16_t *value_p) {
    while (!get(controller, param, value_p)) {
        #ifdef CONTROL_SERIAL_TX_DEBUG
        Serial.println("blocking retry");
        #endif
        continue;
    }
}
