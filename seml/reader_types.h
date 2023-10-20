#pragma once

#include <functional>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <vector>

#include "world.h"

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
    int cob_col = -1;

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
    int start_tick = -1;
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

struct Config {
    Setting setting;
    std::vector<Wave> waves;
};

bool is_cob(const Setting::ProtectPos& protect_pos)
{
    return protect_pos.type == Setting::ProtectPos::Type::Cob;
}