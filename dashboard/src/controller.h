#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "params.h"

class Controller {
    public:
        Controller(const char *name, uint32_t rx, uint32_t tx) : serial(rx, tx), name{name} {}

        const char *name;
        HardwareSerial serial;
        uint32_t lastWorking;
        int16_t feedbackSPD_AVG; // average motor speed RPM
        int16_t feedbackBATV; // battery voltage * 100
        int16_t feedbackDC_CURR; // total DC link current A * 100

        // Values must match default values in hoverboard controllers
        param pCtrlTyp = {false, P_CTRL_TYP, CTRL_TYP_SIN}; // COM, SIN, FOC
        param pCtrlMod = {false, P_CTRL_MOD, CTRL_MOD_VLT}; // VLT, SPD, TRQ
        param pIMotMax = {false, P_I_MOT_MAX, 15}; // Maximum motor current
        param pFiWeakEna = {false, P_FI_WEAK_ENA, 0}; // Field weakening / phase advance enabled
        param pFiWeakMax = {false, P_FI_WEAK_MAX, 2}; // Maximum field weakening current
        param pPhaAdvMax = {false, P_PHA_ADV_MAX, 30}; // Maximum phase advance angle
        param *params[5] = {&pCtrlTyp, &pIMotMax, &pFiWeakEna, &pFiWeakMax, &pPhaAdvMax};

        bool working();
        void recv();
        void set(const char *param, const int16_t value);
        void get(const char *param);

    private:
        void processResponse(char *name, int16_t value);
        void sendCommand(char *command);
};

#endif
