#pragma once

#include <functional>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <vector>

#include "seml/reader/types.h"
#include "seml/types.h"
#include "world.h"

struct Op {
    int tick;
    std::function<void(pvz_emulator::world&)> f;
};

struct GigaInfo {
    unique_zombie zombie;
    unsigned int row; // [0, 5]
    int spawn_wave;
    int spawn_tick;
    int alive_time;
    std::unordered_set<int> hit_by_ash;
    std::unordered_set<int> attempted_smashes;
    std::unordered_set<int> ignored_smashes;
};

struct ActionInfo {
    enum class Type { Ash, Fodder } type;
    int wave;
    int tick;
    std::string desc = "";

    std::vector<unique_plant> plants;
};

struct Info {
    std::vector<Setting::ProtectPos> protect_positions;
    std::vector<GigaInfo> giga_infos;
    std::vector<ActionInfo> action_infos;
    std::vector<unique_plant> cards_to_be_shoveled;
    std::mt19937 gen;
};
