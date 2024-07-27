#ifndef COMMS_H
#define COMMS_H

#include "controller.h"

extern bool set(Controller &controller, const char *param, const int16_t value);
extern bool get(Controller &controller, const char *param, int16_t *value_p);
extern void getBlocking(Controller &controller, const char *param, int16_t *value_p);

#endif
