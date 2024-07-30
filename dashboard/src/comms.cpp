#include "comms.h"

static void flush(Controller &controller) {
    #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
    Serial.print("flush START");
    #endif
    while (controller.serial.available()) {
        #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
        Serial.write(controller.serial.read());
        #else
        controller.serial.read();
        #endif
    }
    #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
    Serial.println("END");
    #endif
}

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
        size_t size = controller.serial.readBytesUntil('\n', (uint8_t*) buf, CONTROL_SERIAL_RECV_BUF_SIZE);

        if (size < 1 || buf[size-1] != '\r') {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.println("RX_err_end: ");
            Serial.println(buf);
            #endif
            continue;
        }

        // Drop \r
        buf[size-1] = '\0';

        #ifdef CONTROL_SERIAL_RX_DEBUG_RAW
        Serial.print("RX_raw||");
        Serial.print(buf);
        Serial.println("||");
        #endif

        // This line contains a value reponse?
        // if (memcmp("# name:", buf, MIN(7, size)) == 0) {
        if (strstr(buf, "# name:") == NULL) {
            Serial.println("RX_missing_name");
            continue;
        }

        char name[32];
        int16_t value;

        // Extract parameter name and value
        char *startQuote = strstr(buf, "\"");
        if (!startQuote) {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.println("RX_err_startQuote");
            #endif
            continue;
        }
        char *endQuote = strstr(startQuote + 1, "\"");
        if (!endQuote) {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.println("RX_err_endQuote");
            #endif
            continue;
        }
        for (int i = 0; i < 32; i ++) {
            name[i] = '\0';
        }
        memcpy(name, startQuote + 1, MIN(endQuote - startQuote - 1, 31));
        name[MIN(endQuote - startQuote - 1, 31)] = '\0';

        char *startValue = strstr(buf, "value:");
        if (!startValue) {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.println("RX_err_startValue");
            #endif
            continue;
        }

        value = (int16_t) strtol(startValue + strlen("value:"), NULL, 10);

        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.print("RX_name:");
        Serial.print(name);
        Serial.print(" RX_val:");
        Serial.println(value);
        #endif

        // Is this the value we were looking for?
        if (strcmp(name, param) != 0) {
            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.println("RX_other");
            #endif
            continue;
        }

        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.println("RX_ok");
        #endif

        if (value_p != NULL) {
            *value_p = value;
        }

        controller.lastWorking = millis();

        return true;
    }

    #ifdef CONTROL_SERIAL_RX_DEBUG
    Serial.println("RX_timeout");
    #endif
    return false; // timeout
}

static void send(Controller &controller, char *command) {
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.print("send ");
    Serial.print(controller.name);
    Serial.print(": ");
    Serial.write((uint8_t*) command, strlen(command));
    #endif

    controller.serial.write(command);
    controller.serial.flush(5000);
}

bool set(Controller &controller, const char *param, const int16_t value) {
    char command[32];
    snprintf(command, 32, "$SET %s %i\n", param, value);

    flush(controller);

    send(controller, command);

    bool success = recv(controller, param, NULL);
    // #ifdef CONTROL_SERIAL_TX_DEBUG
    // Serial.println(success ? " OK" : " FAIL");
    // #endif
    return success;
}

bool get(Controller &controller, const char *param, int16_t *value_p) {
    char command[32];
    snprintf(command, 32, "$GET %s\n", param);

    flush(controller);

    send(controller, command);

    bool success = recv(controller, param, value_p);
    // #ifdef CONTROL_SERIAL_TX_DEBUG
    // Serial.println(success ? " OK" : " FAIL");
    // #endif
    return success;
}

void getBlocking(Controller &controller, const char *param, int16_t *value_p) {
    while (!get(controller, param, value_p)) {
        // #ifdef CONTROL_SERIAL_TX_DEBUG
        // Serial.println("blocking retry");
        // #endif
        continue;
    }
}
