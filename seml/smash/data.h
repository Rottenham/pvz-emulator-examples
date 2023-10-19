#pragma once

#include <set>

#include "smash_types.h"

namespace _SmashInternal {

enum OpState : char {
    Dead,
    Hit,
    Miss,
    NotBorn,
};

using OpStates = std::vector<OpState>;

struct OpStatesHash {
    size_t operator()(const OpStates& op_states) const
    {
        std::hash<OpState> hasher;
        size_t hash = 0;
        for (const auto& op_state : op_states) {
            hash ^= hasher(op_state) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

struct Data {
    int wave = -1;
    int total_garg_count = 0;
    int smashed_garg_count = 0;
    std::array<int, 6> smashed_garg_count_by_row = {};
};

struct Summary {
    struct GargSummary {
        int total_garg_count = 0;
        int smashed_garg_count = 0;
        std::array<int, 6> smashed_garg_count_by_row = {};
    };

    std::set<int> waves;
    std::unordered_map<int, GargSummary> garg_summary_by_wave;
};

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

using RawTable = std::unordered_map<_SmashInternal::OpStates, _SmashInternal::Data,
    _SmashInternal::OpStatesHash>;
using Table = std::vector<std::pair<_SmashInternal::OpStates, _SmashInternal::Data>>;

void update_raw_table(const Info& info, RawTable& raw_table)
{
    for (const auto& garg_info : info.giga_infos) {
        _SmashInternal::OpStates op_states;
        op_states.reserve(info.action_infos.size());
        for (const auto& action_info : info.action_infos) {
            op_states.push_back(_SmashInternal::categorize(action_info, garg_info));
        }

        assert(op_states.size() == info.action_infos.size());
        assert(
            raw_table[op_states].wave == -1 || raw_table[op_states].wave == garg_info.spawn_wave);

        raw_table[op_states].wave = garg_info.spawn_wave;
        raw_table[op_states].total_garg_count++;
        if (!garg_info.ignored_smashes.empty()) {
            raw_table[op_states].smashed_garg_count++;
            raw_table[op_states].smashed_garg_count_by_row[garg_info.row]++;
        }
    }
}

void merge_raw_table(const RawTable& src, RawTable& dst)
{
    for (const auto& [op_states, data] : src) {
        dst[op_states].wave = data.wave;
        dst[op_states].total_garg_count += data.total_garg_count;
        dst[op_states].smashed_garg_count += data.smashed_garg_count;
        for (int i = 0; i < 6; i++) {
            dst[op_states].smashed_garg_count_by_row[i] += data.smashed_garg_count_by_row[i];
        }
    }
}

std::pair<Table, _SmashInternal::Summary> generate_table_and_summary(const RawTable& raw_table)
{
    Table table(raw_table.begin(), raw_table.end());
    std::sort(table.begin(), table.end(), [](const auto& a, const auto& b) {
        for (size_t i = 0; i < a.first.size(); ++i) {
            if (a.first[i] < b.first[i]) {
                return true;
            } else if (a.first[i] > b.first[i]) {
                return false;
            }
        }
        return false;
    });

    _SmashInternal::Summary summary;
    for (const auto& [op_states, data] : table) {
        summary.waves.insert(data.wave);

        auto& garg_summary = summary.garg_summary_by_wave[data.wave];
        garg_summary.total_garg_count += data.total_garg_count;
        garg_summary.smashed_garg_count += data.smashed_garg_count;
        for (int i = 0; i < 6; i++) {
            garg_summary.smashed_garg_count_by_row[i] += data.smashed_garg_count_by_row[i];
        }
    }

    return {table, summary};
}

std::string op_state_to_string(const _SmashInternal::OpState& op_state)
{
    switch (op_state) {
    case _SmashInternal::OpState::NotBorn:
        return "";
    case _SmashInternal::OpState::Hit:
        return "HIT";
    case _SmashInternal::OpState::Miss:
        return "MISS";
    case _SmashInternal::OpState::Dead:
        return "";
    default:
        assert(false && "unreachable");
        return "";
    }
}