#ifndef _MQTT_TIMER_H
#define _MQTT_TIMER_H

#include "config.h"

#include <driver/gptimer.h>

void mqtt_timer_start();
void mqtt_timer_reset();
uint64_t mqtt_timer_get(int i);
void mqtt_timer_stop();

#endif // _MQTT_TIMER_H
