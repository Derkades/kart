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

#define CHANGED_DRIVE_FRONT       1
#define CHANGED_DRIVE_REAR        2
#define CHANGED_CURRENT_FRONT     4
#define CHANGED_CURRENT_REAR      8
#define CHANGED_ANGLE_FRONT      16
#define CHANGED_ANGLE_REAR       32
#define CHANGED_WEAK_FRONT       64
#define CHANGED_WEAK_REAR       128
#define CHANGED_WEAK_ENA_FRONT  256
#define CHANGED_WEAK_ENA_REAR   512
