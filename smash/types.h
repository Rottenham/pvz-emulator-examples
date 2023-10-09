#pragma once

#include <functional>
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
        ss << time << "P ";
        for (const auto& p : positions)
            ss << p.row;
        ss << " " << positions.at(0).col;
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
        ss << "C ";
        for (const auto& p : positions) {
            ss << p.row;
            if (p.type != FodderPos::Type::Normal)
                ss << "'";
        }
        ss << " " << positions.at(0).col;
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
    int spawn_tick;
    int alive_time;
    std::unordered_set<int> hit_by_cob;
    std::unordered_set<int> attempted_smashes;
    std::unordered_set<int> ignored_smashes;
};

struct OpInfo {
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
    std::vector<_SmashInternal::OpInfo> op_infos;
};

struct Op {
    int tick;
    std::function<void(pvz_emulator::world&)> f;
};
