#pragma once

#include "seml/types.h"

struct LossInfo {
    pvz_emulator::object::plant::explode_info explode = {0, 0, 0};
    int hp_loss = 0;
};

// One test contains one wave.
struct Test {
    int start_tick;
    std::vector<std::array<LossInfo, 6>> loss_infos;
    std::vector<pvz_emulator::object::plant*> protect_plants;
    std::vector<std::vector<unique_plant>> plants_to_be_shoveled;
    std::vector<Op> ops;
};