


#ifndef BOARD_STATES_HH
#define BOARD_STATES_HH

namespace Student {

const int NUM_BOARD_STATES = 18;

const bool BOARD_STATES[NUM_BOARD_STATES][8][8] = {

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, true, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, false, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {false, false, true, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, false, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, false, false, true, true, true, true},
        {false, false, true, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, false, false, true, true, true, true},
        {false, false, true, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {false, true, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, false, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, false, true, true, true, true, true, true},
        {true, true, false, false, true, true, true, true},
        {true, false, true, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {false, true, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, false, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, false, true, true, true, true, true, true},
        {true, true, false, false, true, true, true, true},
        {true, false, true, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {true, true, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, false, true, true, true, true, true, true},
        {true, true, false, false, true, true, true, true},
        {false, false, true, false, false, false, false, false},
        {false, false, false, true, false, false, false, false},
        {true, true, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, false, true, true, true, true, true, true},
        {true, true, false, false, true, true, true, true},
        {false, false, true, false, false, false, false, false},
        {true, false, false, true, false, false, false, false},
        {false, true, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    },

    {
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {false, false, false, false, false, false, false, false},
        {true, true, true, true, true, true, true, true},
        {true, true, true, true, true, true, true, true}
    }
};

}

#endif
