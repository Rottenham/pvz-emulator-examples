#pragma once

#include "types.h"

pvz_emulator::object::plant::explode_info operator+(
    const pvz_emulator::object::plant::explode_info& lhs,
    const pvz_emulator::object::plant::explode_info& rhs)
{
    return {lhs.from_upper + rhs.from_upper, lhs.from_same + rhs.from_same,
        lhs.from_lower + rhs.from_lower};
}

pvz_emulator::object::plant::explode_info& operator+=(
    pvz_emulator::object::plant::explode_info& lhs,
    const pvz_emulator::object::plant::explode_info& rhs)
{
    lhs.from_upper += rhs.from_upper;
    lhs.from_same += rhs.from_same;
    lhs.from_lower += rhs.from_lower;
    return lhs;
}

struct MergedWaveInfo {
    int start_tick;
    std::vector<LossInfo> merged_loss_info;
};

using MergedRoundInfo = std::vector<MergedWaveInfo>;

struct Table {
    std::vector<MergedRoundInfo> merged_round_infos;
    int repeat = 0;
};

void update_wave(MergedWaveInfo& merged_wave_info, const WaveInfo& wave_info,
    const std::vector<pvz_emulator::object::plant*>& plants)
{
    if (merged_wave_info.merged_loss_info.empty()) {
        merged_wave_info.merged_loss_info.resize(wave_info.loss_infos.size());
        merged_wave_info.start_tick = wave_info.start_tick;
    }
    assert(merged_wave_info.merged_loss_info.size() == wave_info.loss_infos.size());
    assert(merged_wave_info.start_tick == wave_info.start_tick);

    for (int tick = 0; tick < wave_info.loss_infos.size(); tick++) {
        const auto& loss_info = wave_info.loss_infos.at(tick);

        for (const auto& plant : plants) {
            merged_wave_info.merged_loss_info[tick].explode += loss_info[plant->row].explode;
            merged_wave_info.merged_loss_info[tick].hp_loss += loss_info[plant->row].hp_loss;
        }
    }
}

void update_round(MergedRoundInfo& merged_round_info, const Test& test)
{
    if (merged_round_info.empty()) {
        merged_round_info.resize(test.wave_infos.size());
    }
    assert(merged_round_info.size() == test.wave_infos.size());

    for (int wave_num = 0; wave_num < test.wave_infos.size(); wave_num++) {
        update_wave(merged_round_info[wave_num], test.wave_infos.at(wave_num), test.plants);
    }
}

void update_table(Table& table, const std::vector<Test>& tests)
{
    if (table.merged_round_infos.empty()) {
        table.merged_round_infos.resize(tests.size());
    }
    assert(table.merged_round_infos.size() == tests.size());

    for (int round_num = 0; round_num < tests.size(); round_num++) {
        update_round(table.merged_round_infos[round_num], tests.at(round_num));
    }

    table.repeat++;
}

void merge_table(const Table& src, Table& dst)
{
    if (dst.merged_round_infos.empty()) {
        dst = src;
        return;
    }

    assert(src.merged_round_infos.size() == dst.merged_round_infos.size());

    for (int round_num = 0; round_num < src.merged_round_infos.size(); round_num++) {
        const auto& src_merged_round_info = src.merged_round_infos.at(round_num);
        auto& dst_merged_round_info = dst.merged_round_infos.at(round_num);

        assert(src_merged_round_info.size() == dst_merged_round_info.size());

        for (int wave_num = 0; wave_num < src_merged_round_info.size(); wave_num++) {
            const auto& src_wave_info = src_merged_round_info.at(wave_num);
            auto& dst_wave_info = dst_merged_round_info.at(wave_num);

            assert(src_wave_info.merged_loss_info.size() == dst_wave_info.merged_loss_info.size());
            assert(src_wave_info.start_tick == dst_wave_info.start_tick);

            for (int tick = 0; tick < src_wave_info.merged_loss_info.size(); tick++) {
                dst_wave_info.merged_loss_info[tick].explode
                    += src_wave_info.merged_loss_info[tick].explode;
                dst_wave_info.merged_loss_info[tick].hp_loss
                    += src_wave_info.merged_loss_info[tick].hp_loss;
            }
        }
    }

    dst.repeat += src.repeat;
}