#pragma once

#include <functional>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <vector>

#include "world.h"

namespace _SmashInternal {

struct CobPos {
    int row;
    float col;
};

enum class Fodder {
    Normal,
    Puff,
    Pot,
};

struct CardPos {
    int row;
    int col;
};

struct Cob {
    std::string symbol;
    int time;
    std::vector<CobPos> positions;

    std::string desc() const { return std::to_string(time) + symbol; }
};

struct Jalapeno {
    std::string symbol;
    int time;
    CardPos position;

    std::string desc() const { return std::to_string(time) + symbol; }
};

struct FixedFodder {
    std::string symbol;
    int time;
    int shovel_time = -1;
    std::vector<Fodder> fodders;
    std::vector<CardPos> positions;

    std::string desc() const
    {
        std::stringstream ss;
        ss << time;
        if (shovel_time != -1) {
            ss << "~" << shovel_time;
        }
        ss << symbol;
        return ss.str();
    }
};

struct SmartFodder {
    std::string symbol;
    int time;
    int shovel_time = -1;
    std::vector<Fodder> fodders;
    std::vector<CardPos> positions;
    int choose;
    std::unordered_set<int> waves;

    std::string desc() const
    {
        std::stringstream ss;
        ss << time;
        if (shovel_time != -1) {
            ss << "~" << shovel_time;
        }
        ss << symbol << " ";
        for (const auto& position : positions) {
            ss << position.row;
        }
        ss << ">" << choose;
        return ss.str();
    }
};

using Action = std::variant<Cob, Jalapeno, FixedFodder, SmartFodder>;

struct Wave {
    std::vector<int> ice_times;
    int wave_length;
    std::vector<Action> actions;
};

struct Setting {
    struct ProtectPos {
        enum class Type { Cob, Normal } type;
        int row;
        int col;
    };

    pvz_emulator::object::scene_type scene_type = pvz_emulator::object::scene_type::fog;
    std::vector<ProtectPos> protect_positions;
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

} // namespace _SmashInternal

struct Config {
    _SmashInternal::Setting setting;
    std::vector<_SmashInternal::Wave> waves;
};

struct Info {
    std::vector<_SmashInternal::Setting::ProtectPos> protect_positions;
    std::vector<_SmashInternal::GigaInfo> giga_infos;
    std::vector<_SmashInternal::ActionInfo> action_infos;
    std::mt19937 gen;
};

struct Op {
    int tick;
    std::function<void(pvz_emulator::world&)> f;
};
