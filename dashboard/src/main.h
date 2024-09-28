#pragma once

#define SERIAL_RX_BUFFER_SIZE 256
#include <Arduino.h>
#include <Bounce2.h>

#include "screen.h"
#include "config.h"
#include "controller.h"

enum Menu {
    MENU_HOME,
    MENU_DRIVE,
    MENU_FIELD_WEAK,
    MENU_CURRENT_FRONT,
    MENU_CURRENT_REAR,
    MENU_SAVING,
    MENU_COUNT, // internal use only
};
