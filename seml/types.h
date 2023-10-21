#pragma once

#include "world.h"

struct unique_plant {
    pvz_emulator::object::plant* ptr;
    int uuid;

    bool is_valid() const { return ptr && ptr->uuid == uuid; }
};

struct unique_zombie {
    pvz_emulator::object::zombie* ptr;
    int uuid;

    bool is_valid() const { return ptr && ptr->uuid == uuid; }
};