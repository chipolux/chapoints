#ifndef STEPPER_H
#define STEPPER_H

#include <inttypes.h>
#include <stdbool.h>

void stepper_setup_gpio();
void stepper_setup_timer();
void stepper_teardown_timer();

bool stepper_enqueue(const uint16_t count, const int8_t direction, const bool unlock_at_end);

#endif // STEPPER_H
