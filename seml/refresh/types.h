#pragma once

#include <functional>
#include <unordered_set>

#include "seml/types.h"
#include "world.h"

using ZombieTypes = std::unordered_set<pvz_emulator::object::zombie_type>;
using AccidentRates = std::unordered_map<ZombieTypes, std::vector<float>>;

struct Test {
    AccidentRates accident_rates;
    std::vector<Op> ops;
};

using Tests = std::vector<Test>;