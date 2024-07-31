#include "main.h"

Bounce2::Button switchButton = Bounce2::Button();
Bounce2::Button actionButton = Bounce2::Button();

static Menu menu = (Menu) ((int) MENU_COUNT - 1); // initial state last menu means first menu is actual first state

static Controller cF("front", PA3, PA2); // front motor controller, USART2
static Controller cR("rear", PA10, PA9); // rear motor controller, USART1

static int16_t avgFeedbackRpm() {
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

static int16_t totalFeedbackCurrent() {
    return (cR.working() ? cR.feedbackDC_CURR : 0) + (cF.working() ? cF.feedbackDC_CURR : 0);
}

static int16_t avgFeedbackBatVoltage() {
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

static void drawHome() {
    // If both controllers have no working feedback, there is no data we can show
    if (!cR.working() && !cF.working()) {
        drawStrFull("no comms");
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
        strcpy(batText, "err rear");
    } else if (!cF.working()) {
        strcpy(batText, "err front");
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
    switch(param_get(cR.pCtrlTyp)) {
        case CTRL_TYP_FOC:
            u8g2.drawStr(0, CH*3, "\xaf\xaf\xaf");
            u8g2.drawStr(0, CH*3 + 4, "smooth");
            break;
        case CTRL_TYP_SIN:
            u8g2.drawStr(0, CH*3, "    \xaf\xaf\xaf");
            u8g2.drawStr(0, CH*3 + 4, "faster");
            break;
    }
    u8g2.sendBuffer();
}

void actionDrive() {
    switch(param_get(cR.pCtrlTyp)) {
        case CTRL_TYP_FOC:
            param_set(cR.pCtrlTyp, CTRL_TYP_SIN);
            param_set(cR.pCtrlMod, CTRL_MOD_VLT);
            param_set(cF.pCtrlTyp, CTRL_TYP_SIN);
            param_set(cF.pCtrlMod, CTRL_MOD_VLT);
            break;
        case CTRL_TYP_SIN:
            param_set(cR.pCtrlTyp, CTRL_TYP_FOC);
            param_set(cR.pCtrlMod, CTRL_MOD_TRQ);
            param_set(cF.pCtrlTyp, CTRL_TYP_FOC);
            param_set(cF.pCtrlMod, CTRL_MOD_TRQ);
            break;
    }

    drawDrive();
}

void drawFieldWeak() {
    u8g2.clearBuffer();

    switch (param_get(cR.pCtrlTyp)) {
        case CTRL_TYP_SIN:
            drawSettingHeader("angle");
            u8g2.drawStr(0, CH*2, "0 15 30 45");
            switch(param_get(cR.pPhaAdvMax)) {
                case  0: u8g2.drawStr(0, CH*3, "\xaf"); break;
                case 15: u8g2.drawStr(0, CH*3, "  \xaf\xaf"); break;
                case 30: u8g2.drawStr(0, CH*3, "     \xaf\xaf"); break;
                case 45: u8g2.drawStr(0, CH*3, "        \xaf\xaf"); break;
            }

            u8g2.drawStr(0, CH*3 + 4, param_get(cR.pPhaAdvMax) ? "more speed" : "standard");
            break;
        case CTRL_TYP_FOC:
            drawSettingHeader("fi weak");
            u8g2.drawStr(0, CH*2, "0 2A 5A 8A");
            switch (param_get(cR.pFiWeakMax)) {
                case 0: u8g2.drawStr(0, CH*3, "\xaf"); break;
                case 2: u8g2.drawStr(0, CH*3, "  \xaf\xaf"); break;
                case 5: u8g2.drawStr(0, CH*3, "     \xaf\xaf"); break;
                case 8: u8g2.drawStr(0, CH*3, "        \xaf\xaf"); break;
            }

            u8g2.drawStr(0, CH*3 + 4, param_get(cR.pFiWeakMax) ? "more power" : "disabled");
            break;
    }

    u8g2.sendBuffer();
}

void actionFieldWeak() {
    int16_t newValue;
    switch (param_get(cR.pCtrlTyp)) {
        case CTRL_TYP_SIN:
            newValue = (param_get(cR.pPhaAdvMax) + 15) % 60;
            param_set(cF.pPhaAdvMax, newValue);
            param_set(cR.pPhaAdvMax, newValue);
            param_set(cF.pFiWeakEna, newValue > 0);
            param_set(cR.pFiWeakEna, newValue > 0);
            break;
        case CTRL_TYP_FOC:
            switch(param_get(cR.pFiWeakMax)) {
                case 0: newValue = 2; break;
                case 2: newValue = 5; break;
                case 5: newValue = 8; break;
                case 8: newValue = 0; break;
            }
            param_set(cF.pFiWeakMax, newValue);
            param_set(cR.pFiWeakMax, newValue);
            param_set(cF.pFiWeakEna, newValue > 0);
            param_set(cR.pFiWeakEna, newValue > 0);
            break;
    }

    drawFieldWeak();
}

void drawCurrent(Controller &c) {
    u8g2.clearBuffer();
    drawSettingHeader("current");
    u8g2.drawStr(0, CH*2, "5A 10A 15A");
    switch(param_get(c.pIMotMax)) {
        case 5:  u8g2.drawStr(0, CH*3, "\xaf\xaf"); break;
        case 10: u8g2.drawStr(0, CH*3, "   \xaf\xaf\xaf"); break;
        case 15: u8g2.drawStr(0, CH*3, "       \xaf\xaf\xaf"); break;
    }
    u8g2.drawStr(0, CH*3 + 4, c.name);
    u8g2.sendBuffer();
}

void actionCurrent(Controller &c) {
    int16_t newCurrent = param_get(c.pIMotMax) + 5;
    if (newCurrent > 15) newCurrent = 5;
    param_set(c.pIMotMax, newCurrent);
    drawCurrent(c);
}

void drawSaving() {
    u8g2.clearBuffer();
    u8g2.drawStr(0, CH, "saving");
    // Later, the save loop will add more information about which parameter is saving
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
            drawCurrent(cF);
            break;
        case MENU_CURRENT_REAR:
            drawCurrent(cR);
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
            actionCurrent(cF);
            break;
        case MENU_CURRENT_REAR:
            actionCurrent(cR);
            break;
        default:
            Serial.println("no action");
    }
}

void setup(void) {
    u8g2.begin();

    u8g2.setFont(FONT_STD);

    drawStrFull("loading");

    switchButton.attach(PB_0, INPUT_PULLUP);
    switchButton.interval(5);
    switchButton.setPressedState(LOW);

    actionButton.attach(PB_1, INPUT_PULLUP);
    actionButton.interval(5);
    actionButton.setPressedState(LOW);

    Serial.begin(SERIAL_BAUD);
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

    if (lastSave + SAVE_INTERVAL > millis()) {
        return;
    }
    lastSave = millis();

    Controller controllers[] = {cR, cF};
    for (Controller &c : controllers) {
        for (param *param : c.params) {
            if (!param->dirty) {
                continue; // can immediately try next
            }

            if (!c.working()) {
                param->dirty = false;
                #ifdef SAVE_DEBUG
                Serial.printf("SAVE %s %s: skip, no comms\n", c.name, param->name, param->value);
                #endif
                continue; // can immediately try next
            }

            drawStrFull(param->name);

            // Show on-screen which parameter is saving
            u8g2.clearBuffer();
            u8g2.drawStr(0, CH, "saving");
            u8g2.drawStr(0, CH*2, c.name);
            u8g2.drawStr(0, CH*3, param->name);
            u8g2.sendBuffer();

            return; // sent serial data, so cannot send anything else this loop
        }
    }

    // All changes saved correctly, go to home menu
    onSwitchButton();
}

void loopValueRefresh() {
    static uint32_t lastRefresh;
    static uint8_t lastRefreshCommand;

    if (lastRefresh + HOME_REFRESH_INTERVAL > millis()) {
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
            get(cF, V_SPD_AVG, &cF.feedbackSPD_AVG);
            break;
        case 4:
            get(cF, V_DC_CURR, &cF.feedbackDC_CURR);
            break;
        case 5:
            get(cF, V_BATV, &cF.feedbackBATV);
            break;
    }
    lastRefreshCommand = (lastRefreshCommand + 1) % 6;

    if (menu == MENU_HOME) {
        drawHome();
    }
}

void loop() {
    // Receive serial data from controllers
    recv(cF);
    recv(cR);

    if (menu == MENU_SAVING) {
        loopSave();
        return; // don't loop buttons or refresh values
    }

    loopButtons();

    loopValueRefresh();
}
