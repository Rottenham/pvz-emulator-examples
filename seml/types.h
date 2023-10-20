#pragma once

#include "world.h"

struct unique_plant {
    pvz_emulator::object::plant* ptr;
    int uuid;
};

struct unique_zombie {
    pvz_emulator::object::zombie* ptr;
    int uuid;
};