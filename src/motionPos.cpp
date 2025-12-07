#include "motionPos.h"
#include "move_queue.h"
#include <cmath>
#include <algorithm>
#include "PinDefs.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "step_timer.h"
#include "reed.hpp"

// Define the actual global instances declared as extern in the header
Gantry_t gantry{};
Motors_t motors{};

// Implementation of setupMotion moved from header to avoid multiple-definition
void setupMotion() {
    gantry.motion_active = false;
    gantry.position_reached = true;
    gantry.x = 0;
    gantry.y = 0;
    motors.A_pos = 0;
    motors.B_pos = 0;
}


inline float half_dx_between(int a1, int a2) {
    a1 = std::clamp(a1, 0, 11);
    a2 = std::clamp(a2, 0, 11);
    return fabs(board_pos[a2][0][0] - board_pos[a1][0][0]) / 2.0f;
}

inline float half_dy_between(int b1, int b2) {
    b1 = std::clamp(b1, 0, 7);
    b2 = std::clamp(b2, 0, 7);
    return fabs(board_pos[0][b2][1] - board_pos[0][b1][1]) / 2.0f;
}

void plan_move(int A_from, int B_from, int A_to, int B_to, bool direct) {
    MoveCommand mc;
    float fromX = board_pos[A_from][B_from][0];
    float fromY = board_pos[A_from][B_from][1];
    float toX   = board_pos[A_to][B_to][0];
    float toY   = board_pos[A_to][B_to][1];

    // ---- Step 0: Move to "from" square (magnet off)
    mc = {fromX, fromY, 200.0f, 0.0f, false};
    move_queue_push(&mc);

    // ---- Step 1: If direct, go straight there
    if (direct) {
        mc = {toX, toY, 40.0f, OVERSHOOT_DIST, true};
        move_queue_push(&mc);
    } else {
        mc.magnet = true;
        mc.speed = 40.0f;
        mc.overshoot = OVERSHOOT_DIST;

        //---------------------------------------------------------
        // Step 1: Move halfway out of source square into corridor
        //---------------------------------------------------------
        float x1 = fromX;
        float y1 = fromY;

        if (A_to > A_from)
            x1 += half_dx_between(A_from, A_from + 1);
        else if (A_to < A_from)
            x1 -= half_dx_between(A_from, A_from - 1);
        else
            x1 -= half_dx_between(A_from, std::max(0, A_from - 1)); // default left

        if (B_to > B_from)
            y1 += half_dy_between(B_from, B_from + 1);
        else if (B_to < B_from)
            y1 -= half_dy_between(B_from, B_from - 1);
        else
            y1 += half_dy_between(B_from, std::min(7, B_from + 1)); // default up

        mc.x = x1;
        mc.y = y1;
        move_queue_push(&mc);

        //---------------------------------------------------------
        // Step 2a: Move horizontally in corridor (constant Y)
        //---------------------------------------------------------
        float x2 = x1;
        float y2 = y1;
        if (A_to != A_from) {
            if (A_to > A_from)
                x2 = toX - half_dx_between(A_to - 1, A_to);
            else
                x2 = toX + half_dx_between(A_to + 1, A_to);
        }
        mc.x = x2;
        mc.y = y2;
        move_queue_push(&mc);

        //---------------------------------------------------------
        // Step 2b: Move vertically in corridor (constant X)
        //---------------------------------------------------------
        float x3 = x2;
        float y3 = y2;
        if (B_to != B_from) {
            if (B_to > B_from)
                y3 = toY - half_dy_between(B_to - 1, B_to);
            else
                y3 = toY + half_dy_between(B_to + 1, B_to);
        }
        mc.x = x3;
        mc.y = y3;
        move_queue_push(&mc);

        //---------------------------------------------------------
        // Step 3: Move to destination center
        //---------------------------------------------------------
        mc.x = toX;
        mc.y = toY;
        mc.overshoot = OVERSHOOT_DIST;
        mc.speed = 10.0f;
        move_queue_push(&mc);
    }

    // ---- Step 4: Release magnet
    mc = {toX, toY, 0.0f, 0.0f, false};
    move_queue_push(&mc);
}

// move piece around the space untill detected in position
int correct_movement(int fix_x, int fix_y){

    ESP_LOGI("CORRECTOR", "current switch state: %d", switches->isPopulated(fix_x, fix_y));

    float correction_offset = 5.0f; //mm
    float fix_x_mm = board_pos[fix_x][fix_y][0];
    float fix_y_mm = board_pos[fix_x][fix_y][1];

    moveToXY(fix_x_mm, fix_y_mm, 200.0f, 0.0f, false);

    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    gpio_set_level(MAGNET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(MAGNET_PIN, 0);

    //check if detected
    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 1
    moveToXY(fix_x_mm + correction_offset, fix_y_mm + correction_offset, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 2
    moveToXY(fix_x_mm + correction_offset, fix_y_mm, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 3
    moveToXY(fix_x_mm + correction_offset, fix_y_mm - correction_offset, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 4
    moveToXY(fix_x_mm, fix_y_mm - correction_offset, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 5
    moveToXY(fix_x_mm - correction_offset, fix_y_mm - correction_offset, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 6
    moveToXY(fix_x_mm - correction_offset, fix_y_mm, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 7
    moveToXY(fix_x_mm - correction_offset, fix_y_mm + correction_offset, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // offset 8
    moveToXY(fix_x_mm, fix_y_mm + correction_offset, 20.0f, 0.0f, true);
    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    gpio_set_level(MAGNET_PIN, 0);

    if (switches->isPopulated(fix_x, fix_y)){
        ESP_LOGI("CORRECT", "Piece detected at (%d, %d)", fix_x, fix_y);
        return 1;
    }

    // failed to correct
    ESP_LOGI("CORRECT", "Failed to detect piece at (%d, %d) after correction attempts", fix_x, fix_y);
    return -1;

}

int home_gantry() {
    const float homing_speed = 20.0f; // mm/s
    const float backoff_dist = 20.0f; // mm

    // Temporarily disable interrupts on limit switches to check their state
    gpio_intr_disable(LIMIT_Y_PIN);
    gpio_intr_disable(LIMIT_X_PIN);

    // If a limit switch is already pressed, back off a bit
    if (gpio_get_level(LIMIT_Y_PIN) == 0) {
        ESP_LOGI("HOME", "Y limit switch is already pressed. Backing off.");
        // move +20mm in Y
        moveToXY(gantry.x, gantry.y + backoff_dist, homing_speed, 0.0f, false);
        while(gantry.position_reached == false) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

    }

    // back off X every time
    moveToXY(gantry.x + backoff_dist, gantry.y, homing_speed, 0.0f, false);
    while(gantry.position_reached == false) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (gpio_get_level(LIMIT_Y_PIN) == 0 || gpio_get_level(LIMIT_X_PIN) == 0) {
        ESP_LOGI("HOME", "Failed to back off from limit switches. Homing aborted.");
        // Re-enable interrupts before exiting
        gpio_intr_enable(LIMIT_Y_PIN);
        gpio_intr_enable(LIMIT_X_PIN);
        return -1; // Indicate failure
    }

    // Re-enable interrupts
    gpio_intr_enable(LIMIT_Y_PIN);
    gpio_intr_enable(LIMIT_X_PIN);

    // --- Home Y axis ---
    ESP_LOGI("HOME", "Homing Y axis...");
    limit_y_triggered = false;
    
    // Start moving
    moveToXY(gantry.x, -2000, homing_speed, 0.0f, false); // Move towards negative A and B
    int homeStartTime = esp_log_timestamp();
    while(!limit_y_triggered) {
        if (esp_log_timestamp() - homeStartTime > 40000) { // 40 second timeout
            ESP_LOGI("HOME", "Homing Y axis timed out. Aborting.");
            esp_timer_stop(step_timer);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI("HOME", "Y axis homed.");
    vTaskDelay(pdMS_TO_TICKS(100)); // Settle

    // --- Home X axis ---
    ESP_LOGI("HOME", "Homing X axis...");
    limit_x_triggered = false;

    // Start moving
    moveToXY(-2000, gantry.y, homing_speed, 0.0f, false); // Move towards negative A and B
    
    while(!limit_x_triggered) {
        if (esp_log_timestamp() - homeStartTime > 40000) { // 40 second timeout
            ESP_LOGI("HOME", "Homing X axis timed out. Aborting.");
            esp_timer_stop(step_timer);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI("HOME", "X axis homed.");
    vTaskDelay(pdMS_TO_TICKS(100)); // Settle

    // --- Set zero position ---
    ESP_LOGI("HOME", "Homing complete. Setting zero position.");
    gantry.x = 0;
    gantry.y = 0;
    motors.A_pos = 0;
    motors.B_pos = 0;
    gantry.position_reached = true;
    gantry.motion_active = false;   
    
    gpio_intr_disable(LIMIT_Y_PIN);
    gpio_intr_disable(LIMIT_X_PIN);

    return 1;
}

void yeet_piece(int A_from, int B_from) {
    float fromX = board_pos[A_from][B_from][0];
    float fromY = board_pos[A_from][B_from][1];

    MoveCommand mc;
    mc = {fromX, fromY, 200.0f, 0.0f, false};
    move_queue_push(&mc);

    while(!gantry.position_reached) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    //determine if pos is closer to left or right edge
    float board_center_x = 409.180f / 2.0f;
    if (fromX < board_center_x) {
        //yeet left
        mc = {408.0f, fromY, 250.0f, 0.0f, true};
    } else {
        //yeet right
        mc = { 0.0f, fromY, 250.0f, 0.0f, true};
    }
    move_queue_push(&mc);
    while (!gantry.position_reached) {

        if (gantry.x > 400 || gantry.x < 10) {
            //reached yeet position, stop magnet
            gpio_set_level(MAGNET_PIN, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(MAGNET_PIN, 0);
}

//--------------------------------------------
// Move Verification Functions
//--------------------------------------------

bool wait_for_movement_complete(uint32_t timeout_ms) {
    uint32_t start_time = esp_log_timestamp();

    while (!move_queue_is_empty() || !gantry.position_reached) {
        if ((esp_log_timestamp() - start_time) > timeout_ms) {
            ESP_LOGE("VERIFY", "Timeout waiting for movement to complete");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Additional settle time for reed switch debouncing
    vTaskDelay(pdMS_TO_TICKS(200));
    return true;
}

MoveVerifyResult verify_simple_move(int src_row, int src_col, int dest_row, int dest_col) {
    // Step 1: Verify source is empty (piece was picked up)
    if (switches->isPopulated(src_row, src_col)) {
        ESP_LOGW("VERIFY", "Source (%d,%d) still occupied - piece not picked up", src_row, src_col);
        return VERIFY_SOURCE_NOT_EMPTY;
    }
    ESP_LOGI("VERIFY", "Source (%d,%d) confirmed empty", src_row, src_col);

    // Step 2: Verify destination is occupied (piece was placed)
    if (!switches->isPopulated(dest_row, dest_col)) {
        ESP_LOGW("VERIFY", "Destination (%d,%d) not occupied - attempting correction", dest_row, dest_col);

        // Use existing correction logic
        int correction_result = correct_movement(dest_row, dest_col);
        if (correction_result != 1) {
            ESP_LOGE("VERIFY", "Correction failed for destination (%d,%d)", dest_row, dest_col);
            return VERIFY_CORRECTION_FAILED;
        }
    }
    ESP_LOGI("VERIFY", "Destination (%d,%d) confirmed occupied", dest_row, dest_col);

    return VERIFY_SUCCESS;
}

MoveVerifyResult verify_capture_move(int src_row, int src_col, int dest_row, int dest_col,
                                      int cap_zone_row, int cap_zone_col) {
    // First verify the capture zone has the captured piece
    if (!switches->isPopulated(cap_zone_row, cap_zone_col)) {
        ESP_LOGW("VERIFY", "Capture zone (%d,%d) not occupied", cap_zone_row, cap_zone_col);

        int correction_result = correct_movement(cap_zone_row, cap_zone_col);
        if (correction_result != 1) {
            ESP_LOGE("VERIFY", "Correction failed for capture zone (%d,%d)", cap_zone_row, cap_zone_col);
            return VERIFY_CAPTURE_ZONE_FAIL;
        }
    }
    ESP_LOGI("VERIFY", "Capture zone (%d,%d) confirmed occupied", cap_zone_row, cap_zone_col);

    // Then verify the main move
    return verify_simple_move(src_row, src_col, dest_row, dest_col);
}

MoveVerifyResult verify_castling_move(int king_row, int king_src_col, int king_dest_col,
                                       int rook_src_col, int rook_dest_col) {
    // Verify king moved correctly
    MoveVerifyResult king_result = verify_simple_move(king_row, king_src_col, king_row, king_dest_col);
    if (king_result != VERIFY_SUCCESS) {
        ESP_LOGE("VERIFY", "King verification failed during castling");
        return king_result;
    }

    // Verify rook moved correctly
    MoveVerifyResult rook_result = verify_simple_move(king_row, rook_src_col, king_row, rook_dest_col);
    if (rook_result != VERIFY_SUCCESS) {
        ESP_LOGE("VERIFY", "Rook verification failed during castling");
        return rook_result;
    }

    return VERIFY_SUCCESS;
}
