#include <Arduino.h>
#include <Bounce2.h>

#include "screen.h"
#include "config.h"
#include "comms.h"
#include "controller.h"

// https://github.com/EFeru/hoverboard-firmware-hack-FOC/wiki/Debug-Serial
// Parameters (read-write)
#define P_CTRL_MOD      "CTRL_MOD"      // 1=Voltage, 2=Speed, 3=Torque
#define P_CTRL_TYP      "CTRL_TYP"      // 0=Commutation 1=Sinusodial 2=FOC
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

enum Menu {
    MENU_HOME,
    MENU_DRIVE,
    MENU_FIELD_WEAK,
    MENU_CURRENT_FRONT,
    MENU_CURRENT_REAR,
    MENU_SAVING,
    MENU_COUNT, // internal use only
};
