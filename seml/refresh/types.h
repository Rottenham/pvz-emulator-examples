#pragma once

#include <functional>
#include <unordered_set>

#include "seml/types.h"
#include "world.h"

using ZombieTypes = std::unordered_set<pvz_emulator::object::zombie_type>;
using ZombieList = std::array<pvz_emulator::object::zombie_type, 50>;
using AccidentRates = std::unordered_map<ZombieTypes, std::vector<float>>;

struct Test {
    int init_hp;
    AccidentRates accident_rates;
    std::vector<std::vector<unique_plant>> plants_to_be_shoveled;
    std::vector<Op> ops;
};

using Tests = std::vector<Test>;