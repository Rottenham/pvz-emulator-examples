#pragma once

#include <functional>
#include <vector>

#include "seml/types.h"
#include "world.h"

struct Op {
    int tick;
    std::function<void(pvz_emulator::world&)> f;
};

struct LossInfo {
    pvz_emulator::object::plant::explode_info explode = {0, 0, 0};
    int hp_loss = 0;
};

struct WaveInfo {
    int start_tick;
    std::vector<std::array<LossInfo, 6>> loss_infos;
};

struct Test {
    std::vector<WaveInfo> wave_infos;
    std::vector<pvz_emulator::object::plant*> plants;
    std::vector<std::vector<unique_plant>> plants_to_be_shoveled;
    std::vector<Op> ops;
};