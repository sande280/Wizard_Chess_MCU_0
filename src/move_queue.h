#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    float x;
    float y;
    float speed;
    bool magnet;
} MoveCommand;


void move_queue_init(void);


bool move_queue_push(const MoveCommand* cmd);


bool move_queue_pop(MoveCommand* out);


bool move_queue_is_empty(void);
