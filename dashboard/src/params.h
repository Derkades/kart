#ifndef PARAMS_H
#define PARAMS_H

#include <Arduino.h>

// https://github.com/EFeru/hoverboard-firmware-hack-FOC/wiki/Debug-Serial
// Parameters (read-write)
#define P_CTRL_TYP      "CTRL_TYP"      // 0=Commutation 1=Sinusodial 2=FOC
#define P_CTRL_MOD      "CTRL_MOD"      // 1=Voltage, 2=Speed, 3=Torque
#define P_I_MOT_MAX     "I_MOT_MAX"     // Max motor current A
#define P_FI_WEAK_ENA   "FI_WEAK_ENA"   // Field weakening / phase advance enabled
#define P_FI_WEAK_MAX   "FI_WEAK_MAX"
#define P_PHA_ADV_MAX   "PHA_ADV_MAX"
// Variables (read-only)
#define V_DC_CURR       "DC_CURR"       // dc current, centiamps
#define V_SPD_AVG       "SPD_AVG"       // rpm
#define V_BATV          "BATV"          // battery voltage, centivolts

#define CTRL_MOD_VLT 1
#define CTRL_MOD_SPD 2
#define CTRL_MOD_TRQ 3

#define CTRL_TYP_COM 0
#define CTRL_TYP_SIN 1
#define CTRL_TYP_FOC 2

struct param {
    bool dirty;
    const char *name;
    int16_t value;
};

inline void param_set(param &p, int16_t value) {
    p.dirty = true;
    p.value = value;
}

inline int16_t param_get(const param &p) {
    return p.value;
}

#endif
