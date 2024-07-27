#include "main.h"

Bounce2::Button switchButton = Bounce2::Button();
Bounce2::Button actionButton = Bounce2::Button();

static Menu menu = (Menu) ((int) MENU_COUNT - 1); // initial state last menu means first menu is actual first state

// Setting state
// static Drive drive = DRIVE_FOC;
static int16_t ctrlMod = CTRL_MOD_VLT;
static int16_t ctrlTyp = CTRL_TYP_SIN;
static int16_t motorMaxCurrentFront = 5;
static int16_t motorMaxCurrentRear = 5;
static int16_t fieldWeakEnabled = 0;
static int16_t phaseAdvanceAngle = 0;
static int16_t fieldWeakCurrent = 0;
// static uint16_t changed;

static Controller cF("front", PA3, PA2); // front motor controller, USART2
static Controller cR("rear", PA10, PA9); // rear motor controller, USART1

// Must be kept in sync with changes array
#define CHANGE_TYPE_FRONT       0
#define CHANGE_TYPE_REAR        1
#define CHANGE_MODE_FRONT       2
#define CHANGE_MODE_REAR        3
#define CHANGE_CURRENT_FRONT    4
#define CHANGE_CURRENT_REAR     5
#define CHANGE_WEAK_ENA_FRONT   6
#define CHANGE_WEAK_ENA_REAR    7
#define CHANGE_ANGLE_FRONT      8
#define CHANGE_ANGLE_REAR       9
#define CHANGE_WEAK_FRONT       10
#define CHANGE_WEAK_REAR        11
#define CHANGES_COUNT           12

struct change {
    bool changed;
    const char *parameter_name;
    int16_t &value;
    Controller &controller;
};

static change changes[] = {
    {false,     P_CTRL_MOD,     ctrlMod,                cF},
    {false,     P_CTRL_MOD,     ctrlMod,                cR},
    {false,     P_CTRL_TYP,     ctrlTyp,                cF},
    {false,     P_CTRL_TYP,     ctrlTyp,                cR},
    {false,     P_I_MOT_MAX,    motorMaxCurrentFront,   cF},
    {false,     P_I_MOT_MAX,    motorMaxCurrentRear,    cR},
    {false,     P_FI_WEAK_ENA,  fieldWeakEnabled,       cF},
    {false,     P_FI_WEAK_ENA,  fieldWeakEnabled,       cR},
    {false,     P_FI_WEAK_MAX,  phaseAdvanceAngle,      cF},
    {false,     P_FI_WEAK_MAX,  phaseAdvanceAngle,      cR},
    {false,     P_PHA_ADV_MAX,  fieldWeakCurrent,       cF},
    {false,     P_PHA_ADV_MAX,  fieldWeakCurrent,       cR},
};

static inline void set_changed(size_t change) {
    changes[change].changed = true;
}

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
    switch(ctrlTyp) {
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
    switch(ctrlTyp) {
        case CTRL_TYP_FOC:
            ctrlTyp = CTRL_TYP_SIN;
            ctrlMod = CTRL_MOD_VLT;
            break;
        case CTRL_TYP_SIN:
            ctrlTyp = CTRL_TYP_FOC;
            ctrlMod = CTRL_MOD_TRQ;
            break;
    }

    set_changed(CHANGE_TYPE_FRONT);
    set_changed(CHANGE_TYPE_REAR);
    set_changed(CHANGE_MODE_FRONT);
    set_changed(CHANGE_MODE_REAR);

    drawDrive();
}

void drawFieldWeak() {
    u8g2.clearBuffer();

    switch (ctrlTyp) {
        case CTRL_TYP_SIN:
            drawSettingHeader("angle");
            u8g2.drawStr(0, CH*2, "0 15 30 45");
            switch(phaseAdvanceAngle) {
                case  0: u8g2.drawStr(0, CH*3, "\xaf"); break;
                case 15: u8g2.drawStr(0, CH*3, "  \xaf\xaf"); break;
                case 30: u8g2.drawStr(0, CH*3, "     \xaf\xaf"); break;
                case 45: u8g2.drawStr(0, CH*3, "        \xaf\xaf"); break;
            }

            u8g2.drawStr(0, CH*3 + 4, phaseAdvanceAngle == 0 ? "standard" : "more speed");
            break;
        case CTRL_TYP_FOC:
            drawSettingHeader("fi weak");
            u8g2.drawStr(0, CH*2, "0 2A 5A 8A");
            switch (fieldWeakCurrent) {
                case 0: u8g2.drawStr(0, CH*3, "\xaf"); break;
                case 2: u8g2.drawStr(0, CH*3, "  \xaf\xaf"); break;
                case 5: u8g2.drawStr(0, CH*3, "     \xaf\xaf"); break;
                case 8: u8g2.drawStr(0, CH*3, "        \xaf\xaf"); break;
            }

            u8g2.drawStr(0, CH*3 + 4, fieldWeakCurrent == 0 ? "disabled" : "more power");
            break;
    }

    u8g2.sendBuffer();
}

void actionFieldWeak() {
        switch (ctrlTyp) {
        case CTRL_TYP_SIN:
            phaseAdvanceAngle = (phaseAdvanceAngle + 15) % 60;
            fieldWeakEnabled = phaseAdvanceAngle != 0;
            set_changed(CHANGE_WEAK_ENA_FRONT);
            set_changed(CHANGE_WEAK_ENA_REAR);
            set_changed(CHANGE_ANGLE_FRONT);
            set_changed(CHANGE_ANGLE_REAR);
            break;
        case CTRL_TYP_FOC:
            switch(fieldWeakCurrent) {
                case 0: fieldWeakCurrent = 2; break;
                case 2: fieldWeakCurrent = 5; break;
                case 5: fieldWeakCurrent = 8; break;
                case 8: fieldWeakCurrent = 0; break;
            }
            fieldWeakEnabled = fieldWeakCurrent != 0;
            set_changed(CHANGE_WEAK_ENA_FRONT);
            set_changed(CHANGE_WEAK_ENA_REAR);
            set_changed(CHANGE_WEAK_FRONT);
            set_changed(CHANGE_WEAK_REAR);
            break;
    }

    drawFieldWeak();
}

void drawCurrent(const char *name, int16_t &motorMaxCurrent) {
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

void actionCurrent(const char *name, int16_t &motorMaxCurrent, size_t change) {
    motorMaxCurrent += 5;
    if (motorMaxCurrent > 15) motorMaxCurrent = 5;
    set_changed(change);

    drawCurrent(name, motorMaxCurrent);
}

void drawSaving() {
    drawStrFull("saving");
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
            actionCurrent("front", motorMaxCurrentFront, CHANGE_CURRENT_FRONT);
            break;
        case MENU_CURRENT_REAR:
            actionCurrent("rear", motorMaxCurrentRear, CHANGE_CURRENT_REAR);
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

    if (lastSave + SAVE_INTERVAL > millis()) {
        return;
    }
    lastSave = millis();

    for (size_t i = 0; i < CHANGES_COUNT; i++) {
        change c = changes[i];

        #ifdef SAVE_DEBUG
        Serial.print("save ");
        Serial.print(c.parameter_name);
        Serial.print(" ");
        Serial.print(c.controller.name);
        Serial.print(": ");
        #endif

        if (!c.changed) {
            #ifdef SAVE_DEBUG
            Serial.println("skip, unchanged");
            #endif
            continue; // can immediately try next
        }

        if (!c.controller.working()) {
            // c.changed = false;
            #ifdef SAVE_DEBUG
            Serial.println("skip, no comms");
            #endif
            continue; // can immediately try next
        }

        if (set(c.controller, c.parameter_name, c.value)) {
            c.changed = false;
            #ifdef SAVE_DEBUG
            Serial.println("success");
            #endif
        } else {
            #ifdef SAVE_DEBUG
            Serial.println("failed");
            #endif
        }

        return; // must wait for next loop
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
    if (menu == MENU_SAVING) {
        loopSave();
        return; // Pressing buttons not allowed in save menu
    }

    loopButtons();

    loopValueRefresh();
}
