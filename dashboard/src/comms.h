#ifndef COMMS_H
#define COMMS_H

#include "controller.h"

extern void recv(Controller &controller);
extern void set(Controller &controller, const char *param, const int16_t value);
extern void get(Controller &controller, const char *param, int16_t *value_p);

#endif
