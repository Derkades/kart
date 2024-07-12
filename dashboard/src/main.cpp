#include "main.h"

Bounce2::Button switchButton = Bounce2::Button();
Bounce2::Button actionButton = Bounce2::Button();

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
            return lastWorking + CONTROL_SERIAL_WORKING_TIMEOUT_MS > millis();
        }
};

static Menu menu = (Menu) ((int) MENU_COUNT - 1); // initial state last menu means first menu is actual first state

// Setting state
static Drive drive = DRIVE_FOC;
static uint8_t motorMaxCurrentFront = 5;
static uint8_t motorMaxCurrentRear = 5;
static uint8_t phaseAdvanceAngle = 0;
static uint8_t fieldWeakCurrent = 0;
static uint8_t fieldWeakEnabled = 0;
static uint16_t changed;

static Controller cR("rear", PA10, PA9); // rear motor controller, USART1
static Controller cF("front", PA3, PA2); // front motor controller, USART2

// waits for OK response for a specific parameter/value
bool recv(Controller &controller, const char *param, int16_t *value_p) {
    const uint32_t start = millis();
    char buf[CONTROL_SERIAL_RECV_BUF_SIZE];

    while (start + CONTROL_SERIAL_TIMEOUT_MS > millis()) {
        // Wait for serial to be available
        if (!Serial.available()) {
            continue;
        }

        // Read a line from serial and add trailing null byte
        size_t size = controller.serial.readBytesUntil('\n', (uint8_t*) buf, CONTROL_SERIAL_RECV_BUF_SIZE - 1);
        buf[size] = '\0';

        #ifdef CONTROL_SERIAL_RX_DEBUG
        Serial.print("recv:");
        Serial.println(recvBuf);
        #endif

        // This line contains a value reponse?
        if (memcmp("# name:", buf, MIN(7, size)) == 0) {
            char name[32];
            int16_t value;

            // Extract parameter name and value
            char *startQuote = strstr(buf, "\"");
            if (!startQuote) {
                Serial.println("err startQuote");
                continue;
            }
            char *endQuote = strstr(startQuote + 1, "\"");
            if (!endQuote) {
                Serial.println("err endQuote");
                continue;
            }
            memcpy(name, startQuote + 1, MIN(endQuote - startQuote - 1, 31));
            name[MIN(endQuote - startQuote - 1, 31)] = '\0';

            char *startValue = strstr(buf, "value:");
            if (!startValue) {
                Serial.println("err startValue");
                continue;
            }

            value = (int16_t) strtol(startValue + strlen("value:"), NULL, 10);

            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.print("name:");
            Serial.print(name);
            Serial.print(" value:");
            Serial.print(value);
            #endif

            // Is this the value we were looking for?
            if (strcmp(name, param) != 0) {
                #ifdef CONTROL_SERIAL_RX_DEBUG
                Serial.println(" IGNORE");
                #endif
                continue;
            }

            #ifdef CONTROL_SERIAL_RX_DEBUG
            Serial.println(" OK");
            #endif

            if (value_p != NULL) {
                *value_p = value;
            }

            controller.lastWorking = millis();

            return true;
        }
    }

    return false; // timeout
}

bool set(Controller &controller, const char *param, const int16_t value) {
    char command[32];
    snprintf(command, 32, "$SET %s %i\n", param, value);

    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.print("send ");
    Serial.print(controller.name);
    Serial.print(": ");
    Serial.write((uint8_t*) command, strlen(command) - 1);
    controller.serial.write(command);
    #endif

    bool success = recv(controller, param, NULL);
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.println(success ? " OK" : "FAIL");
    #endif
    return success;
}

bool get(Controller &controller, const char *param, int16_t *value_p) {
    char command[32];
    snprintf(command, 32, "$GET %s\n", param);

    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.print("send ");
    Serial.print(controller.name);
    Serial.print(": ");
    Serial.write((uint8_t*) command, strlen(command) - 1);
    controller.serial.write(command);
    #endif

    bool success = recv(controller, param, value_p);
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.println(success ? " OK" : "FAIL");
    #endif
    return success;
}

void getBlock(Controller &controller, const char *param, int16_t *value_p) {
    while (!get(controller, param, value_p)) {
        #ifdef CONTROL_SERIAL_TX_DEBUG
        Serial.println("blocking retry");
        #endif
        continue;
    }
}

int16_t avgFeedbackRpm() {
    if (cR.working() && cF.working()) {
        return (cR.feedbackSPD_AVG + cF.feedbackSPD_AVG) / 2;
    } else if (cR.working()) {
        return cR.feedbackSPD_AVG;
    } else if (cF.working()) {
        return cF.feedbackSPD_AVG;
    } else {
        return 0;
    }
}

int16_t totalFeedbackCurrent() {
    return (cR.working() ? cR.feedbackDC_CURR : 0) + (cF.working() ? cF.feedbackDC_CURR : 0);
}

int16_t avgFeedbackBatVoltage() {
    if (cR.working() && cF.working()) {
        return (cR.feedbackBATV + cF.feedbackBATV) / 2;
    } else if (cR.working()) {
        return cR.feedbackBATV;
    } else if (cF.working()) {
        return cF.feedbackBATV;
    } else {
        return 0;
    }
}

void drawHome() {
    // If both controllers have no working feedback, there is no data we can show
    if (!cR.working() && !cF.working()) {
        u8g2.clearBuffer();
        drawStrCentered2("no comms");
        u8g2.sendBuffer();
        return;
    }

    const int16_t rpm = avgFeedbackRpm();
    const float current = totalFeedbackCurrent() * 1e-2f;
    const float voltage = avgFeedbackBatVoltage() * 1e-2f;

    char speedText[16], powerText[16], batText[16];

    // Speed
    const int8_t speed = (uint8_t) (6e-5f * rpm * WHEEL_CIRCUMFERENCE_MM);
    snprintf(speedText, 16, "%i km/h", speed);

    // Power
    const float power = current * voltage;
    snprintf(powerText, 16, "%iA %iW", (int) current, (int) power);

    // Battery, or error message if one of the two controllers can't be reached
    if (!cR.working()) {
        strcpy(batText, "no comms 1");
    } else if (!cF.working()) {
        strcpy(batText, "no comms 2");
    } else {
        const float batVoltage = 1e-2f * voltage;
        const uint8_t batPercent = (uint8_t) ((voltage - BAT_VOLT_EMPTY) * 100 / (BAT_VOLT_FULL - BAT_VOLT_EMPTY));
        snprintf(batText, 16, "%.1fV %i%%", batVoltage, batPercent);
    }

    // Write to screen
    u8g2.clearBuffer();
    drawStrCentered(CH, speedText);
    drawStrCentered(CH*2, powerText);
    drawStrCentered(CH*3, batText);
    u8g2.sendBuffer();
}

void drawDrive() {
    u8g2.clearBuffer();
    drawSettingHeader("drive");
    u8g2.drawStr(0, CH*2, "FOC SIN");
    switch(drive) {
        case DRIVE_FOC:
            u8g2.drawStr(0, CH*3, "\xaf\xaf\xaf");
            u8g2.drawStr(0, CH*3 + 4, "smooth");
            break;
        case DRIVE_SIN:
            u8g2.drawStr(0, CH*3, "    \xaf\xaf\xaf");
            u8g2.drawStr(0, CH*3 + 4, "faster");
            break;
    }
    u8g2.sendBuffer();
}

void actionDrive() {
    switch(drive) {
        case DRIVE_FOC: drive = DRIVE_SIN; break;
        case DRIVE_SIN: drive = DRIVE_FOC; break;
    }
    changed |= CHANGED_DRIVE_FRONT | CHANGED_DRIVE_REAR;

    drawDrive();
}

void drawFieldWeak() {
    u8g2.clearBuffer();
    // deg symbol is \xb0
    if (drive == DRIVE_SIN) {
        drawSettingHeader("angle");
        u8g2.drawStr(0, CH*2, "0 15 30 45");
        switch(phaseAdvanceAngle) {
            case  0: u8g2.drawStr(0, CH*3, "\xaf"); break;
            case 15: u8g2.drawStr(0, CH*3, "  \xaf\xaf"); break;
            case 30: u8g2.drawStr(0, CH*3, "     \xaf\xaf"); break;
            case 45: u8g2.drawStr(0, CH*3, "        \xaf\xaf"); break;
        }

        u8g2.drawStr(0, CH*3 + 4, phaseAdvanceAngle == 0 ? "standard" : "more speed");
    } else if (drive == DRIVE_FOC) {
        drawSettingHeader("fi weak");
        u8g2.drawStr(0, CH*2, "0 2A 5A 8A");
        switch (fieldWeakCurrent) {
            case 0: u8g2.drawStr(0, CH*3, "\xaf"); break;
            case 2: u8g2.drawStr(0, CH*3, "  \xaf\xaf"); break;
            case 5: u8g2.drawStr(0, CH*3, "     \xaf\xaf"); break;
            case 8: u8g2.drawStr(0, CH*3, "        \xaf\xaf"); break;
        }

        u8g2.drawStr(0, CH*3 + 4, fieldWeakCurrent == 0 ? "disabled" : "more power");
    }

    out:
    u8g2.sendBuffer();
}

void actionFieldWeak() {
    if (drive == DRIVE_SIN) {
        phaseAdvanceAngle = (phaseAdvanceAngle + 15) % 60;
        fieldWeakEnabled = phaseAdvanceAngle != 0;
        changed |= CHANGED_ANGLE_REAR | CHANGED_ANGLE_FRONT | CHANGED_WEAK_ENA_REAR | CHANGED_WEAK_ENA_FRONT;
    } else if (drive == DRIVE_FOC) {
        switch(fieldWeakCurrent) {
            case 0: fieldWeakCurrent = 2; break;
            case 2: fieldWeakCurrent = 5; break;
            case 5: fieldWeakCurrent = 8; break;
            case 8: fieldWeakCurrent = 0; break;
        }
        fieldWeakEnabled = fieldWeakCurrent != 0;
        changed |= CHANGED_ANGLE_REAR | CHANGED_ANGLE_FRONT | CHANGED_WEAK_ENA_REAR | CHANGED_WEAK_ENA_FRONT;
    }

    drawFieldWeak();
}

void drawCurrent(const char *name, uint8_t &motorMaxCurrent) {
    u8g2.clearBuffer();
    drawSettingHeader("current");
    u8g2.drawStr(0, CH*2, "5A 10A 15A");
    switch(motorMaxCurrent) {
        case 5:  u8g2.drawStr(0, CH*3, "\xaf\xaf"); break;
        case 10: u8g2.drawStr(0, CH*3, "   \xaf\xaf\xaf"); break;
        case 15: u8g2.drawStr(0, CH*3, "       \xaf\xaf\xaf"); break;
    }
    u8g2.drawStr(0, CH*3 + 4, name);
    u8g2.sendBuffer();
}

void actionCurrent(const char *name, uint8_t *motorMaxCurrent, uint8_t changed_flag) {
    *motorMaxCurrent += 5;
    if (*motorMaxCurrent > 15) *motorMaxCurrent = 5;
    changed |= changed_flag;

    drawCurrent(name, *motorMaxCurrent);
}

void drawSaving() {
    u8g2.clearBuffer();
    drawStrCentered2("saving");
    u8g2.sendBuffer();
}

void onSwitchButton() {
    menu = (Menu) ((int) (menu + 1) % MENU_COUNT);

    switch(menu) {
        case MENU_HOME:
            drawHome();
            break;
        case MENU_DRIVE:
            drawDrive();
            break;
        case MENU_FIELD_WEAK:
            drawFieldWeak();
            break;
        case MENU_CURRENT_FRONT:
            drawCurrent("front", motorMaxCurrentFront);
            break;
        case MENU_CURRENT_REAR:
            drawCurrent("rear", motorMaxCurrentRear);
            break;
        case MENU_SAVING:
            drawSaving();
            break;
        default:
            Serial.println("error");
    }
}

void onActionButton() {
    switch(menu) {
        case MENU_DRIVE:
            actionDrive();
            break;
        case MENU_FIELD_WEAK:
            actionFieldWeak();
            break;
        case MENU_CURRENT_FRONT:
            actionCurrent("front", &motorMaxCurrentFront, CHANGED_CURRENT_FRONT);
            break;
        case MENU_CURRENT_REAR:
            actionCurrent("rear", &motorMaxCurrentRear, CHANGED_CURRENT_REAR);
            break;
        default:
            Serial.println("no action");
    }
}

void setup(void) {
    u8g2.begin();

    u8g2.setFont(FONT_STD);
    drawStrCentered2("loading");
    u8g2.sendBuffer();

    switchButton.attach(PB_0, INPUT_PULLUP );
    switchButton.interval(5);
    switchButton.setPressedState(LOW);

    actionButton.attach(PB_1, INPUT_PULLUP );
    actionButton.interval(5);
    actionButton.setPressedState(LOW);

    Serial.begin(115200);
    cR.serial.begin(CONTROL_SERIAL_BAUD);
    cF.serial.begin(CONTROL_SERIAL_BAUD);

    onSwitchButton(); // show home menu
}

void loopButtons() {
    switchButton.update();
    actionButton.update();

    if (switchButton.pressed()) {
        Serial.println("button: switch");
        onSwitchButton();
    }

    if (actionButton.pressed()) {
        Serial.println("button: action");
        onActionButton();
    }
}

void loopSave() {
    static uint32_t lastSave;

    if (lastSave + 500 > millis()) {
        return;
    }
    lastSave = millis();
    // TODO set VLT/TRQ mode

    if ((changed & CHANGED_DRIVE_FRONT)) {
        if (set(cF, P_CTRL_TYP, drive)) changed ^= CHANGED_DRIVE_FRONT;
        return;
    } else if ((changed & CHANGED_DRIVE_REAR)) {
        if (set(cR, P_CTRL_TYP, drive)) changed ^= CHANGED_DRIVE_REAR;
        return;
    } else if ((changed & CHANGED_WEAK_ENA_FRONT)) {
        if (set(cF, P_FI_WEAK_ENA, fieldWeakEnabled)) changed ^= CHANGED_WEAK_ENA_FRONT;
        return;
    } else if ((changed & CHANGED_WEAK_ENA_REAR)) {
        if (set(cR, P_FI_WEAK_ENA, fieldWeakEnabled)) changed ^= CHANGED_WEAK_ENA_REAR;
        return;
    } else if (fieldWeakEnabled && (changed & CHANGED_WEAK_FRONT) ) {
        if (set(cF, P_FI_WEAK_MAX, fieldWeakCurrent)) changed ^= CHANGED_WEAK_FRONT;
        return;
    } else if (fieldWeakEnabled && (changed & CHANGED_WEAK_REAR)) {
        if (set(cR, P_FI_WEAK_MAX, fieldWeakCurrent)) changed ^= CHANGED_WEAK_REAR;
        return;
    } else if (fieldWeakEnabled && (changed & CHANGED_ANGLE_FRONT)) {
        if (set(cF, P_PHA_ADV_MAX, phaseAdvanceAngle)) changed ^= CHANGED_ANGLE_FRONT;
        return;
    } else if (fieldWeakEnabled && (changed & CHANGED_ANGLE_REAR)) {
        if (set(cR, P_PHA_ADV_MAX, phaseAdvanceAngle)) changed ^= CHANGED_ANGLE_REAR;
        return;
    } else if ((changed & CHANGED_CURRENT_FRONT)) {
        if (set(cF, P_I_MOT_MAX, motorMaxCurrentFront) || !cF.working()) changed ^= CHANGED_CURRENT_FRONT;
        return;
    } else if ((changed & CHANGED_CURRENT_REAR) ) {
        if (set(cR, P_I_MOT_MAX, motorMaxCurrentRear) || !cR.working()) changed ^= CHANGED_CURRENT_REAR;
        return;
    } else if (changed == 0) {
        // All changes saved, go to home menu
        onSwitchButton();
    }

    return; // Pressing buttons not allowed
}

void loopValueRefresh() {
    static uint32_t lastRefresh;
    static uint8_t lastRefreshCommand;

    if (lastRefresh + 500 > millis()) {
        return;
    }

    int16_t value;
    lastRefresh = millis();
    switch(lastRefreshCommand) {
        case 0:
            get(cR, V_SPD_AVG, &cR.feedbackSPD_AVG);
            break;
        case 1:
            get(cR, V_DC_CURR, &cR.feedbackDC_CURR);
            break;
        case 2:
            get(cR, V_BATV, &cR.feedbackBATV);
            break;
        case 3:
            get(cF, V_SPD_AVG, &cR.feedbackSPD_AVG);
            break;
        case 4:
            get(cF, V_DC_CURR, &cR.feedbackDC_CURR);
            break;
        case 5:
            get(cF, V_BATV, &cR.feedbackBATV);
            break;
    }
    lastRefreshCommand = (lastRefreshCommand + 1) % 6;

    if (menu == MENU_HOME) {
        drawHome();
    }
}

void loop() {
    if (menu == MENU_SAVING) {
        loopSave();
        return; // Pressing buttons not allowed in save menu
    }

    loopButtons();

    loopValueRefresh();
}
