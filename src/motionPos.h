#pragma once

const float PULLEY_TEETH   = 20.0f;
const float BELT_PITCH_MM  = 2.0f;
const float STEP_ANGLE_DEG = 1.8f;
const float MICROSTEP      = 16.0f;

const float STEPS_PER_REV   = 360.0f / STEP_ANGLE_DEG;
const float DIST_PER_REV_MM = PULLEY_TEETH * BELT_PITCH_MM;
const float STEPS_PER_MM    = (STEPS_PER_REV * MICROSTEP) / DIST_PER_REV_MM;

const float OVERSHOOT_DIST = 0.0f;


//--------------------------------------------
// Motion State
//--------------------------------------------
// Named structs so we can declare extern instances in this header
typedef struct {
    float x, y;              // current XY position (mm)
    float x_target, y_target;
    bool motion_active;
    bool position_reached;
} Gantry_t;

typedef struct {
    long A_pos, B_pos;        // current step counts
    long A_target, B_target;  // target step counts
} Motors_t;

//--------------------------------------------
// Move Verification Types
//--------------------------------------------
typedef enum {
    VERIFY_SUCCESS = 0,
    VERIFY_SOURCE_NOT_EMPTY = 1,
    VERIFY_DEST_NOT_OCCUPIED = 2,
    VERIFY_CORRECTION_FAILED = 3,
    VERIFY_CAPTURE_ZONE_FAIL = 4,
    VERIFY_TIMEOUT = 5
} MoveVerifyResult;

typedef struct {
    int source_row, source_col;
    int dest_row, dest_col;
    int capture_zone_row, capture_zone_col;  // -1 if no capture
    bool is_castling;
    int rook_src_col, rook_dest_col;         // For castling
} MoveVerifyContext;

// Declare global instances (defined in motionPos.cpp)
extern Gantry_t gantry;
extern Motors_t motors;

//--------------------------------------------
// Initialization
//--------------------------------------------
void setupMotion();
void plan_move(int A_from, int B_from, int A_to, int B_to, bool direct);
int home_gantry();
int correct_movement(int fix_x, int fix_y);

// Declare moveToXY so it can be used in motionPos.cpp
bool moveToXY(float x_target_mm, float y_target_mm, float speed_mm_s, float overshoot, bool magnet_on);

//--------------------------------------------
// Move Verification Functions
//--------------------------------------------
bool wait_for_movement_complete(uint32_t timeout_ms);
MoveVerifyResult verify_simple_move(int src_row, int src_col, int dest_row, int dest_col);
MoveVerifyResult verify_capture_move(int src_row, int src_col, int dest_row, int dest_col,
                                      int cap_zone_row, int cap_zone_col);
MoveVerifyResult verify_castling_move(int king_row, int king_src_col, int king_dest_col,
                                       int rook_src_col, int rook_dest_col);

extern volatile bool limit_y_triggered;
extern volatile bool limit_x_triggered;

constexpr float board_pos[12][8][2] = {
    {{0.000f, 20.000f}, {0.000f, 57.000f}, {0.000f, 94.000f}, {0.000f, 131.000f}, {0.000f, 168.000f}, {0.000f, 205.000f}, {0.000f, 242.000f}, {0.000f, 279.000f}},
    {{30.000f, 20.000f}, {30.000f, 57.000f}, {30.000f, 94.000f}, {30.000f, 131.000f}, {30.000f, 168.000f}, {30.000f, 205.000f}, {30.000f, 242.000f}, {30.000f, 279.000f}},
    {{75.090f, 20.000f}, {75.090f, 57.000f}, {75.090f, 94.000f}, {75.090f, 131.000f}, {75.090f, 168.000f}, {75.090f, 205.000f}, {75.090f, 242.000f}, {75.090f, 279.000f}},
    {{112.090f, 20.000f}, {112.090f, 57.000f}, {112.090f, 94.000f}, {112.090f, 131.000f}, {112.090f, 168.000f}, {112.090f, 205.000f}, {112.090f, 242.000f}, {112.090f, 279.000f}},
    {{149.090f, 20.000f}, {149.090f, 57.000f}, {149.090f, 94.000f}, {149.090f, 131.000f}, {149.090f, 168.000f}, {149.090f, 205.000f}, {149.090f, 242.000f}, {149.090f, 279.000f}},
    {{186.090f, 20.000f}, {186.090f, 57.000f}, {186.090f, 94.000f}, {186.090f, 131.000f}, {186.090f, 168.000f}, {186.090f, 205.000f}, {186.090f, 242.000f}, {186.090f, 279.000f}},
    {{223.090f, 20.000f}, {223.090f, 57.000f}, {223.090f, 94.000f}, {223.090f, 131.000f}, {223.090f, 168.000f}, {223.090f, 205.000f}, {223.090f, 242.000f}, {223.090f, 279.000f}},
    {{260.090f, 20.000f}, {260.090f, 57.000f}, {260.090f, 94.000f}, {260.090f, 131.000f}, {260.090f, 168.000f}, {260.090f, 205.000f}, {260.090f, 242.000f}, {260.090f, 279.000f}},
    {{297.090f, 20.000f}, {297.090f, 57.000f}, {297.090f, 94.000f}, {297.090f, 131.000f}, {297.090f, 168.000f}, {297.090f, 205.000f}, {297.090f, 242.000f}, {297.090f, 279.000f}},
    {{334.090f, 20.000f}, {334.090f, 57.000f}, {334.090f, 94.000f}, {334.090f, 131.000f}, {334.090f, 168.000f}, {334.090f, 205.000f}, {334.090f, 242.000f}, {334.090f, 279.000f}},
    {{379.180f, 20.000f}, {379.180f, 57.000f}, {379.180f, 94.000f}, {379.180f, 131.000f}, {379.180f, 168.000f}, {379.180f, 205.000f}, {379.180f, 242.000f}, {379.180f, 279.000f}},
    {{409.180f, 20.000f}, {409.180f, 57.000f}, {409.180f, 94.000f}, {409.180f, 131.000f}, {409.180f, 168.000f}, {409.180f, 205.000f}, {409.180f, 242.000f}, {409.180f, 279.000f}}
};