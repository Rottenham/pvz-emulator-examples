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
        std::ostringstream os;
        os << time;
        if (shovel_time != -1) {
            os << "~" << shovel_time;
        }
        os << symbol;
        return os.str();
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
        std::ostringstream os;
        os << time;
        if (shovel_time != -1) {
            os << "~" << shovel_time;
        }
        os << symbol;
        if (positions.size() != choose) {
            os << " ";
            for (const auto& position : positions) {
                os << position.row;
            }
            os << ">" << choose;
        }
        return os.str();
    }
};

using Action = std::variant<Cob, Jalapeno, FixedFodder, SmartFodder>;

std::string desc(const Action& action)
{
    if (auto a = std::get_if<Cob>(&action)) {
        return a->desc();
    } else if (auto a = std::get_if<Jalapeno>(&action)) {
        return a->desc();
    } else if (auto a = std::get_if<FixedFodder>(&action)) {
        return a->desc();
    } else if (auto a = std::get_if<SmartFodder>(&action)) {
        return a->desc();
    } else {
        assert(false && "unreachable");
    }
}

struct Wave {
    std::vector<int> ice_times;
    int wave_length;
    std::vector<Action> actions;
    int start_tick = -1;
};

using Round = std::vector<Wave>;

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
    std::vector<Round> rounds;
};

bool is_cob(const Setting::ProtectPos& protect_pos)
{
    return protect_pos.type == Setting::ProtectPos::Type::Cob;
}