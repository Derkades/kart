#include <Arduino.h>
#include <Bounce2.h>

#include "main.h"
#include "screen.h"

// Vehicle parameters
#define WHEEL_CIRCUMFERENCE_MM 603
#define BAT_VOLT_EMPTY 330 * 12
#define BAT_VOLT_FULL 415 * 12

// Serial control protocol parameters
#define CONTROL_SERIAL_BAUD 9600     // TODO try faster baud rate
// #define CONTROL_SERIAL_RX_DEBUG
#define CONTROL_SERIAL_TX_DEBUG
#define CONTROL_SERIAL_TIMEOUT_MS 40

Bounce2::Button switchButton = Bounce2::Button();
Bounce2::Button actionButton = Bounce2::Button();

enum Drive {
    DRIVE_COM,
    DRIVE_SIN,
    DRIVE_FOC
};

enum Menu {
    MENU_HOME,
    MENU_DRIVE,
    MENU_FIELD_WEAK,
    MENU_CURRENT_FRONT,
    MENU_CURRENT_REAR,
    MENU_SAVING,
    MENU_COUNT, // internal use only
};

class Controller {
    public:
        Controller(uint8_t id, uint32_t rx, uint32_t tx) : serial(rx, tx), id{id} {}
        uint8_t id;
        HardwareSerial serial;
        // bool feedbackWorking; // TODO ipv true/false, timestamp bijhouden en als niet working beschouwen bij 5 sec niks
        uint32_t lastWorking;
        int16_t feedbackSPD_AVG; // average motor speed RPM
        uint16_t feedbackBATV; // battery voltage * 100
        int16_t feedbackDC_CURR; // total DC link current A * 100

        bool feedbackWorking() {
            return lastWorking + 30000 > millis();
        }
};

static Menu menu = (Menu) ((int) MENU_COUNT - 1); // initial state last menu means first menu is actual first state

#define CHANGED_DRIVE_FRONT           1
#define CHANGED_DRIVE_REAR           2
#define CHANGED_CURRENT_FRONT   4
#define CHANGED_CURRENT_REAR    8
#define CHANGED_ANGLE_FRONT           16
#define CHANGED_ANGLE_REAR           32
#define CHANGED_WEAK_FRONT            64
#define CHANGED_WEAK_REAR            128
#define CHANGED_WEAK_ENA_FRONT             256
#define CHANGED_WEAK_ENA_REAR        512

// Setting state
static Drive drive = DRIVE_FOC;
static uint8_t motorMaxCurrentFront = 5;
static uint8_t motorMaxCurrentRear = 5;
static uint8_t phaseAdvanceAngle = 0;
static uint8_t fieldWeakCurrent = 0;
static uint8_t fieldWeakEnabled = 0;
static uint16_t changed;

static Controller controller1(1, PA10, PA9); // rear drive, USART1
static Controller controller2(2, PA3, PA2); // front drive, USART2

bool controlSerialRecv(Controller &controller) {
    char recvBuf[64];

    uint16_t i = 0;
    while (!controller.serial.available()) {
        delay(1);
        if (i++ > CONTROL_SERIAL_TIMEOUT_MS) {
            return false;
        }
    }

    size_t size = controller.serial.readBytesUntil('\n', (uint8_t*) recvBuf, 64);
    recvBuf[MIN(63, size)] = '\0';

    #ifdef CONTROL_SERIAL_RX_DEBUG
    Serial.print("recv:");
    Serial.println(recvBuf);
    #endif

    if (memcmp("OK", recvBuf, 2) == 0) {
        #ifdef CONTROL_SERIAL_TX_DEBUG
        Serial.println("recv_ok");
        #endif
        return true;
    }

    if (memcmp("# name:", recvBuf, MIN(7, size)) == 0) {
        char name[32];
        int16_t value;

        char *startQuote = strstr(recvBuf, "\"");
        if (!startQuote) {
            Serial.println("err startQuote");
            goto next;
        }
        char *endQuote = strstr(startQuote + 1, "\"");
        if (!endQuote) {
            Serial.println("err endQuote");
            goto next;
        }
        memcpy(name, startQuote + 1, MIN(endQuote - startQuote - 1, 31));
        name[MIN(endQuote - startQuote - 1, 31)] = '\0';

        char *startValue = strstr(recvBuf, "value:");
        if (!startValue) {
            Serial.println("err startValue");
            goto next;
        }

        value = (int16_t) strtol(startValue + strlen("value:"), NULL, 10);

        #ifdef CONTROL_SERIAL_TX_DEBUG
        Serial.print("name:");
        Serial.print(name);
        Serial.print(" value:");
        Serial.println(value);
        #endif

        if (strcmp(name, "SPD_AVG") == 0) {
            controller.feedbackSPD_AVG = value;
        } else if (strcmp(name, "BATV") == 0) {
            controller.feedbackBATV = value;
        } else if (strcmp(name, "DC_CURR") == 0) {
            controller.feedbackDC_CURR = value;
        }
    }

    next:
    return controlSerialRecv(controller);
}

bool _sendCommand(Controller &controller, char *command) {
    // Add newline
    int i;
    while (controller.serial.availableForWrite() < 32) {
        delay(1);
        if (i++ > CONTROL_SERIAL_TIMEOUT_MS) {
            Serial.println("send_timeout");
            return false;
        }
    }
    #ifdef CONTROL_SERIAL_TX_DEBUG
    Serial.print("send command to ");
    Serial.print(controller.id);
    Serial.print(": ");
    Serial.print(command);
    controller.serial.write(command);
    #endif

    return controlSerialRecv(controller); // Waits for command to be received
}

void addNewline(char *buf) {
    size_t len = strlen(buf);
    buf[len] = '\n';
    buf[len+1] = '\0';
}

bool sendCommand(Controller &controller, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buf[32];
    vsnprintf(buf, 31, format, args); // limit is one less because of added newline
    addNewline(buf);
    va_end(args);

    return _sendCommand(controller, buf);
}

// bool sendCommandWorking(const char *format, ...) {
//     va_list args;
//     va_start(args, format);
//     char buf[32];
//     vsnprintf(buf, 31, format, args); // limit is one less because of added newline
//     addNewline(buf);
//     va_end(args);

//     bool success1 = true, success2 = true;
//     if (controller1.feedbackWorking()) {
//         success1 = _sendCommand(controller1, buf);
//         delay(1000);
//     }
//     if (controller2.feedbackWorking()) {
//         success2 = _sendCommand(controller2, buf);
//         delay(1000);
//     }
//     return success1 && success2;
// }

void drawHome() {
    // If both controllers have no working feedback, there is no data we can show
    if (!controller1.feedbackWorking() && !controller2.feedbackWorking()) {
        u8g2.clearBuffer();
        drawStrCentered2("no comms");
        u8g2.sendBuffer();
        return;
    }

    // Calculate averages and totals of both controllers or just one
    const int16_t avgFeedbackRpm =
        controller1.feedbackWorking() && controller2.feedbackWorking() ? (controller1.feedbackSPD_AVG + controller1.feedbackSPD_AVG) / 2
        : controller1.feedbackWorking() ? controller1.feedbackSPD_AVG
        : controller2.feedbackWorking() ? controller2.feedbackSPD_AVG
        : 0;
    const int16_t totFeedbackCurrent =
            (controller1.feedbackWorking() ? controller1.feedbackDC_CURR : 0) +
            (controller2.feedbackWorking() ? controller2.feedbackDC_CURR : 0);
    const uint16_t avgFeedbackBatVoltage =
        controller1.feedbackWorking() && controller2.feedbackWorking() ? (controller1.feedbackBATV + controller1.feedbackBATV) / 2
        : controller1.feedbackBATV ? controller1.feedbackBATV
        : controller2.feedbackBATV ? controller2.feedbackBATV
        : 0;

    char speedText[16], powerText[16], batText[16];

    // Speed
    const int8_t speed = (uint8_t) (6e-5f * avgFeedbackRpm * WHEEL_CIRCUMFERENCE_MM);
    snprintf(speedText, 16, "%i km/h", speed);

    // Power
    const float current = 1e-2f * totFeedbackCurrent;
    const float power = current * avgFeedbackBatVoltage * 1e-2f;
    snprintf(powerText, 16, "%iA %iW", (int) current, (int) power);

    // Battery, or error message if one of the two controllers can't be reached
    if (!controller1.feedbackWorking()) {
        strcpy(batText, "no comms 1");
    } else if (!controller2.feedbackWorking()) {
        strcpy(batText, "no comms 2");
    } else {
        const float batVoltage = 1e-2f * avgFeedbackBatVoltage;
        const uint8_t batPercent = (uint8_t) ((avgFeedbackBatVoltage - BAT_VOLT_EMPTY) * 100 / (BAT_VOLT_FULL - BAT_VOLT_EMPTY));
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
        changed |= CHANGED_ANGLE_REAR | CHANGED_ANGLE_FRONT;
        changed |= CHANGED_WEAK_ENA_FRONT | CHANGED_WEAK_ENA_REAR;

        // if (!phaseAdvanceAngle && fieldWeakEnabled) {
        //     fieldWeakEnabled = 0;
        //     changed |= CHANGED_WEAK_ENA;
        // } else if (phaseAdvanceAngle && !fieldWeakEnabled) {
        //     fieldWeakEnabled = 1;
        //     changed |= CHANGED_WEAK_ENA;
        // }
    } else if (drive == DRIVE_FOC) {
        switch(fieldWeakCurrent) {
            case 0: fieldWeakCurrent = 2; break;
            case 2: fieldWeakCurrent = 5; break;
            case 5: fieldWeakCurrent = 8; break;
            case 8: fieldWeakCurrent = 0; break;
        }

        changed |= CHANGED_ANGLE_REAR | CHANGED_ANGLE_FRONT;
        changed |= CHANGED_WEAK_ENA_FRONT | CHANGED_WEAK_ENA_REAR;

        // changed |= CHANGED_WEAK;

        // if (!fieldWeakCurrent && fieldWeakEnabled) {
        //     fieldWeakEnabled = 0;
        //     changed |= CHANGED_WEAK_ENA;
        // } else if (fieldWeakCurrent && !fieldWeakEnabled) {
        //     fieldWeakEnabled = 1;
        //     changed |= CHANGED_WEAK_ENA;
        // }
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
            // force mark all as changed
            // #ifdef FORCE_SAVE
            // changed = 65535;
            // #endif
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
    controller1.serial.begin(CONTROL_SERIAL_BAUD);
    controller2.serial.begin(CONTROL_SERIAL_BAUD);

    onSwitchButton(); // show home menu
}

uint32_t lastMenuRefresh;
uint8_t lastMenuRefreshCommand;
uint32_t lastSave;

void loop() {
    switchButton.update();
    actionButton.update();

    if (menu == MENU_SAVING && lastSave + 500 < millis()) {
        // delay(1000);

        // Clear receive queue
        // while (controlSerialRecv(controller1)) {}
        // while (controlSerialRecv(controller2)) {}

        lastSave = millis();
        // TODO set VLT/TRQ mode

        if ((changed & CHANGED_DRIVE_FRONT)) {
            if (sendCommand(controller2, "$SET CTRL_TYP %i", drive)) {
                changed ^= CHANGED_DRIVE_FRONT;
            }
            return;
        } else if ((changed & CHANGED_DRIVE_REAR)) {
            if (sendCommand(controller1, "$SET CTRL_TYP %i", drive)) {
                changed ^= CHANGED_DRIVE_REAR;
            }
            return;
        } else if ((changed & CHANGED_WEAK_ENA_FRONT)) {
            if ( sendCommand(controller2, "$SET FI_WEAK_ENA %i", fieldWeakEnabled)) {
                changed ^= CHANGED_WEAK_ENA_FRONT;
            }
            return;
        } else if ((changed & CHANGED_WEAK_ENA_REAR)) {
            if (sendCommand(controller1, "$SET FI_WEAK_ENA %i", fieldWeakEnabled)) {
                changed ^= CHANGED_WEAK_ENA_REAR;
            }
            return;
        } else if (fieldWeakEnabled && (changed & CHANGED_WEAK_FRONT) ) {
            if (sendCommand(controller2, "$SET FI_WEAK_MAX %i", fieldWeakCurrent)) {
                changed ^= CHANGED_WEAK_FRONT;

            }
            return;
        } else if (fieldWeakEnabled && (changed & CHANGED_WEAK_REAR)) {
            if (sendCommand(controller2, "$SET FI_WEAK_MAX %i", fieldWeakCurrent)) {
                changed ^= CHANGED_WEAK_REAR;

            }
            return;
        } else if (fieldWeakEnabled && (changed & CHANGED_ANGLE_FRONT)) {
            if (sendCommand(controller2, "$SET PHA_ADV_MAX %i", phaseAdvanceAngle)) {
                changed ^= CHANGED_ANGLE_FRONT;
            }
            return;
        } else if (fieldWeakEnabled && (changed & CHANGED_ANGLE_REAR)) {
            if (sendCommand(controller1, "$SET PHA_ADV_MAX %i", phaseAdvanceAngle)) {
            changed ^= CHANGED_ANGLE_REAR;

            }
            return;
        } else if ((changed & CHANGED_CURRENT_FRONT)) {
            if ((sendCommand(controller2, "$SET I_MOT_MAX %i", motorMaxCurrentFront) || !controller2.feedbackWorking())) {
                changed ^= CHANGED_CURRENT_FRONT;
            }
            return;
        } else if ((changed & CHANGED_CURRENT_REAR) ) {
            if ((sendCommand(controller1, "$SET I_MOT_MAX %i", motorMaxCurrentRear) || !controller1.feedbackWorking())) {
                changed ^= CHANGED_CURRENT_REAR;
            }
            return;
        } else if (changed == 0) {
            // All changes saved, go to home menu
            onSwitchButton();
        }

        return; // Pressing buttons not allowed
    }

    if (switchButton.pressed()) {
        Serial.println("button: switch");
        onSwitchButton();
    }

    if (actionButton.pressed()) {
        Serial.println("button: action");
        onActionButton();
    }

    // Periodically refresh home menu
    if (menu == MENU_HOME && lastMenuRefresh + 300 < millis()) {
        lastMenuRefresh = millis();
        switch(lastMenuRefreshCommand) {
            case 0:
                if (sendCommand(controller1, "$GET SPD_AVG")) {
                    controller1.lastWorking = millis();
                }
                break;
            case 1:
                if (sendCommand(controller1, "$GET DC_CURR")) {
                    controller1.lastWorking = millis();
                }
                break;
            case 2:
                if (sendCommand(controller1, "$GET BATV")) {
                    controller1.lastWorking = millis();
                }
                break;
            case 3:
                if (sendCommand(controller2, "$GET SPD_AVG")) {
                    controller2.lastWorking = millis();
                }
                break;
            case 4:
                if (sendCommand(controller2, "$GET DC_CURR")) {
                    controller2.lastWorking = millis();
                }
                break;
            case 5:
                if (sendCommand(controller2, "$GET BATV")) {
                    controller2.lastWorking = millis();
                }
                break;
        }
        lastMenuRefreshCommand = (lastMenuRefreshCommand + 1) % 6;
        drawHome();
    }
}
