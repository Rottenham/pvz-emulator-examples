#pragma once

#include "types.h"

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
    int total_garg_count = 0;
    int smashed_garg_count = 0;
};

using RawTable = std::unordered_map<OpStates, Data, OpStatesHash>;
using Table = std::vector<std::pair<OpStates, Data>>;

OpState categorize(const _SmashInternal::OpInfo& op_info, const _SmashInternal::GargInfo& garg_info)
{
    if (op_info.tick < garg_info.spawn_tick) {
        return OpState::NotBorn;
    } else if (op_info.tick > garg_info.spawn_tick + garg_info.alive_time) {
        return OpState::Dead;
    } else if (op_info.type == _SmashInternal::OpInfo::Type::Cob
        || op_info.type == _SmashInternal::OpInfo::Type::Card) {

        auto effective_uuids = op_info.type == _SmashInternal::OpInfo::Type::Cob
            ? garg_info.hit_by_cob
            : garg_info.attempted_smashes;

        for (const auto& plant : op_info.plants) {
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

void update_raw_table(const Info& info, RawTable& raw_table)
{
    for (const auto& garg_info : info.garg_infos) {
        OpStates op_states;
        for (const auto& op_info : info.op_infos) {
            op_states.push_back(categorize(op_info, garg_info));
        }

        assert(op_states.size() == info.op_infos.size());

        raw_table[op_states].total_garg_count++;
        if (!garg_info.ignored_smashes.empty()) {
            raw_table[op_states].smashed_garg_count++;
        }
    }
}

Table generate_table(const RawTable& raw_table)
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

    return table;
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
        return "X";
    default:
        assert(false && "unreachable");
        return "";
    }
}