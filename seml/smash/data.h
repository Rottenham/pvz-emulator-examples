#pragma once

#include <set>

#include "types.h"

enum OpState : char {
    Dead,
    Hit,
    Miss,
    NotBorn,
};

struct OpStates {
    std::vector<OpState> op_states;
    int wave;
};

namespace std {

template <> struct hash<OpStates> {
    size_t operator()(const OpStates& os) const
    {
        size_t hash = 0;
        for (const auto& state : os.op_states) {
            hash ^= std::hash<int>()(state) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        hash ^= std::hash<int>()(os.wave) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

}

bool operator==(const OpStates& lhs, const OpStates& rhs)
{
    return lhs.op_states == rhs.op_states && lhs.wave == rhs.wave;
}

namespace _SmashInternal {

OpState categorize(const ActionInfo& action_info, const GigaInfo& garg_info)
{
    if (action_info.tick < garg_info.spawn_tick) {
        return OpState::NotBorn;
    } else if (action_info.tick > garg_info.spawn_tick + garg_info.alive_time) {
        return OpState::Dead;
    } else if (action_info.type == ActionInfo::Type::Ash
        || action_info.type == ActionInfo::Type::Fodder) {

        auto effective_uuids = action_info.type == ActionInfo::Type::Ash
            ? garg_info.hit_by_ash
            : garg_info.attempted_smashes;

        for (const auto& plant : action_info.plants) {
            if (effective_uuids.count(plant.uuid)) {
                return OpState::Hit;
                continue;
            }
        }
        return OpState::Miss;
    } else {
        assert(false && "unreachable");
    }
}

} // namespace _SmashInternal

struct Data {
    int total_garg_count = 0;
    int smashed_garg_count = 0;
    std::array<int, 6> smashed_garg_count_by_row = {};
};

using RawTable = std::unordered_map<OpStates, Data>;
using Table = std::vector<std::pair<OpStates, Data>>;

struct Summary {
    std::set<int> waves;
    std::unordered_map<int, Data> garg_summary_by_wave;
};

void update_raw_table(const Info& info, RawTable& raw_table)
{
    for (const auto& garg_info : info.giga_infos) {
        OpStates os;
        os.wave = garg_info.spawn_wave;
        os.op_states.reserve(info.action_infos.size());
        for (const auto& action_info : info.action_infos) {
            os.op_states.push_back(_SmashInternal::categorize(action_info, garg_info));
        }

        assert(os.op_states.size() == info.action_infos.size());

        raw_table[os].total_garg_count++;
        if (!garg_info.ignored_smashes.empty()) {
            raw_table[os].smashed_garg_count++;
            raw_table[os].smashed_garg_count_by_row[garg_info.row]++;
        }
    }
}

void merge_raw_table(const RawTable& src, RawTable& dst)
{
    for (const auto& [op_states, data] : src) {
        dst[op_states].total_garg_count += data.total_garg_count;
        dst[op_states].smashed_garg_count += data.smashed_garg_count;
        for (int i = 0; i < 6; i++) {
            dst[op_states].smashed_garg_count_by_row[i] += data.smashed_garg_count_by_row[i];
        }
    }
}

std::pair<Table, Summary> generate_table_and_summary(const RawTable& raw_table)
{
    Table table(raw_table.begin(), raw_table.end());
    std::sort(table.begin(), table.end(),
        [](const std::pair<OpStates, Data>& a, const std::pair<OpStates, Data>& b) {
            if (a.first.wave < b.first.wave) {
                return true;
            } else if (a.first.wave > b.first.wave) {
                return false;
            }

            for (int i = 0; i < a.first.op_states.size(); ++i) {
                if (a.first.op_states[i] < b.first.op_states[i]) {
                    return true;
                } else if (a.first.op_states[i] > b.first.op_states[i]) {
                    return false;
                }
            }
            return false;
        });

    Summary summary;
    for (const auto& [os, data] : table) {
        summary.waves.insert(os.wave);

        auto& garg_summary = summary.garg_summary_by_wave[os.wave];
        garg_summary.total_garg_count += data.total_garg_count;
        garg_summary.smashed_garg_count += data.smashed_garg_count;
        for (int i = 0; i < 6; i++) {
            garg_summary.smashed_garg_count_by_row[i] += data.smashed_garg_count_by_row[i];
        }
    }

    return {table, summary};
}

std::string op_state_to_string(const OpState& op_state)
{
    switch (op_state) {
    case OpState::NotBorn:
        return "";
    case OpState::Hit:
        return "HIT";
    case OpState::Miss:
        return "MISS";
    case OpState::Dead:
        return "";
    default:
        assert(false && "unreachable");
        return "";
    }
}
