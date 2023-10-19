#pragma once

#include <functional>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <vector>

#include "seml/reader_types.h"
#include "world.h"

struct Op {
    int tick;
    std::function<void(pvz_emulator::world&)> f;
};

struct GigaInfo {
    struct {
        pvz_emulator::object::zombie* ptr;
        int uuid;
    } zombie;
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

    struct plant {
        pvz_emulator::object::plant* ptr;
        int uuid;
    };
    std::vector<plant> plants;
};

struct Info {
    std::vector<Setting::ProtectPos> protect_positions;
    std::vector<GigaInfo> giga_infos;
    std::vector<ActionInfo> action_infos;
    std::mt19937 gen;
};
