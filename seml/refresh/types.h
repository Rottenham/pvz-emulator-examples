#pragma once

#include <functional>
#include <unordered_set>

#include "seml/types.h"
#include "world.h"

using ZombieTypes = std::unordered_set<pvz_emulator::object::zombie_type>;
using ZombieList = std::array<pvz_emulator::object::zombie_type, 50>;

struct ZombieTypesHash {
    size_t operator()(const ZombieTypes& zombie_types) const noexcept
    {
        size_t hash = 0;
        for (const auto& zombie_type : zombie_types) {
            hash ^= std::hash<pvz_emulator::object::zombie_type>()(zombie_type) + 0x9e3779b9
                + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

struct Test {
    int init_hp;
    std::unordered_map<ZombieTypes, std::vector<float>, ZombieTypesHash> accident_rates;
    std::vector<std::vector<unique_plant>> plants_to_be_shoveled;
    std::vector<Op> ops;
};

using Tests = std::vector<Test>;

