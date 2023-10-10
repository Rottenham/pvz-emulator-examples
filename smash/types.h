#pragma once

#include <functional>
#include <sstream>
#include <unordered_set>
#include <variant>
#include <vector>

#include "world.h"

namespace _SmashInternal {

struct Cob {
    struct CobPos {
        int row;
        float col;
    };

    int time;
    std::vector<CobPos> positions;

    std::string desc() const
    {
        std::stringstream ss;
        ss << time << "P";
        return ss.str();
    }
};

struct FodderPos {
    enum class Type { Normal, Puff } type;
    int row;
    int col;
};

struct FixedFodder {
    int time;
    int shovel_time = -1;
    std::vector<FodderPos> positions;

    std::string desc() const
    {
        std::stringstream ss;
        ss << time;
        if (shovel_time != -1) {
            ss << "~" << shovel_time;
        }
        ss << "C";
        return ss.str();
    }
};

struct SmartFodder {
    int time;
    int shovel_time = -1;
    std::vector<FodderPos> positions;
    int choose;
    std::vector<int> waves;
};

using Action = std::variant<Cob, FixedFodder, SmartFodder>;

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

struct GargInfo {
    struct {
        pvz_emulator::object::zombie* ptr;
        int uuid;
    } zombie;
    unsigned int row; // [0, 5]
    int spawn_wave;
    int spawn_tick;
    int alive_time;
    std::unordered_set<int> hit_by_cob;
    std::unordered_set<int> attempted_smashes;
    std::unordered_set<int> ignored_smashes;
};

struct ActionInfo {
    enum class Type { Cob, Card } type;
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
    std::vector<_SmashInternal::GargInfo> garg_infos;
    std::vector<_SmashInternal::ActionInfo> action_infos;
};

struct Op {
    int tick;
    std::function<void(pvz_emulator::world&)> f;
};
