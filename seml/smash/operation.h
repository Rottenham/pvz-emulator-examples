#pragma once

#include <functional>

#include "common/pe.h"
#include "constants/constants.h"
#include "seml/operation.h"
#include "seml/types.h"
#include "types.h"

namespace _SmashInternal {

std::vector<int> get_giga_rows(const Setting& setting)
{
    std::vector<int> giga_rows;
    giga_rows.reserve(setting.protect_positions.size());
    for (const auto& protect_position : setting.protect_positions) {
        giga_rows.push_back(protect_position.row - 1);
    }
    return giga_rows;
}

int pick_giga_row(const std::vector<int>& giga_rows, std::mt19937& gen)
{
    if (giga_rows.empty()) {
        return -1;
    } else {
        std::uniform_int_distribution<size_t> distribution(0, giga_rows.size() - 1);
        auto index = distribution(gen);
        return giga_rows[index];
    }
}

void insert_setup(
    std::vector<Op>& ops, int tick, const std::vector<Setting::ProtectPos>& protect_positions)
{
    auto f = [protect_positions](pvz_emulator::world& w) {
        for (const auto& pos : protect_positions) {
            auto plant_type = is_cob(pos) ? pvz_emulator::object::plant_type::cob_cannon
                                          : pvz_emulator::object::plant_type::umbrella_leaf;
            auto col = is_cob(pos) ? pos.col - 2 : pos.col - 1;

            auto& p = w.plant_factory.create(plant_type, pos.row - 1, col);
            p.ignore_garg_smash = true;
        }
    };
    ops.push_back({tick, f});
}

void insert_spawn(std::vector<Op>& ops, Info& info, int tick, int wave, int giga_num,
    const std::vector<int>& giga_rows)
{
    auto f = [&info, tick, wave, giga_num, giga_rows](pvz_emulator::world& w) {
        // sync current gigas
        for (auto& giga_info : info.giga_infos) {
            auto& zombie = giga_info.zombie;

            if (zombie.is_valid()) {
                giga_info.alive_time = giga_info.zombie.ptr->time_since_spawn;

                giga_info.hit_by_ash.clear();
                for (int i = 0; i < zombie.ptr->hit_by_ash.size; i++) {
                    giga_info.hit_by_ash.insert(zombie.ptr->hit_by_ash.arr[i]);
                }

                giga_info.attempted_smashes.clear();
                for (int i = 0; i < zombie.ptr->attempted_smashes.size; i++) {
                    giga_info.attempted_smashes.insert(zombie.ptr->attempted_smashes.arr[i]);
                }

                giga_info.ignored_smashes.clear();
                for (int i = 0; i < zombie.ptr->ignored_smashes.size; i++) {
                    giga_info.ignored_smashes.insert(zombie.ptr->ignored_smashes.arr[i]);
                }
            }
        }

        w.scene.spawn.wave = wave;

        for (int i = 0; i < giga_num; i++) {
            auto& z = w.zombie_factory.create(pvz_emulator::object::zombie_type::giga_gargantuar,
                pick_giga_row(giga_rows, info.gen));
            info.giga_infos.push_back({{&z, z.uuid}, z.row, wave, tick, 0, {}, {}, {}});
        }
    };
    ops.push_back({tick, f});
}

void insert_ice(std::vector<Op>& ops, int tick)
{
    auto f = [](pvz_emulator::world& w) {
        w.plant_factory.create(pvz_emulator::object::plant_type::iceshroom, 0, 0);
    };
    ops.push_back({tick, f});
}

void insert_cob(std::vector<Op>& ops, Info& info, int tick, int wave, const Cob* cob,
    const pvz_emulator::object::scene_type& scene_type)
{
    info.action_infos.push_back({ActionInfo::Type::Ash, wave, tick, cob->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto cob_col = cob->cob_col;

    for (const auto& pos : cob->positions) {
        auto f = [&info, idx, pos, cob_col](pvz_emulator::world& w) {
            info.action_infos[idx].plants.push_back(
                {nullptr, launch_cob(w, pos.row, pos.col, cob_col)});
        };

        ops.push_back({tick - get_cob_fly_time(scene_type, pos.row, pos.col, cob_col), f});
    }
}

void insert_jalapeno(std::vector<Op>& ops, Info& info, int tick, int wave, const Jalapeno* jalapeno)
{
    info.action_infos.push_back({ActionInfo::Type::Ash, wave, tick, jalapeno->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto pos = jalapeno->position;

    auto f = [&info, idx, pos](pvz_emulator::world& w) {
        auto& p = w.plant_factory.create(
            pvz_emulator::object::plant_type::jalapeno, pos.row - 1, pos.col - 1);
        info.action_infos[idx].plants.push_back({&p, p.uuid});
    };
    ops.push_back({tick - 100, f});
}

void insert_fixed_fodder(
    std::vector<Op>& ops, Info& info, int tick, int wave, const FixedFodder* fodder)
{
    info.action_infos.push_back({ActionInfo::Type::Fodder, wave, tick, fodder->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;

    auto f = [&info, idx, fodders, positions](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        for (size_t i = 0; i < fodders.size(); i++) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            info.action_infos[idx].plants.push_back({&p, p.uuid});
        }
    };
    ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&info, idx](pvz_emulator::world& w) {
            for (const auto& plant : info.action_infos[idx].plants) {
                if (plant.is_valid()) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        ops.push_back({tick + fodder->shovel_time - fodder->time, f});
    }
}

void insert_smart_fodder(
    std::vector<Op>& ops, Info& info, int tick, int wave, const SmartFodder* fodder)
{
    info.action_infos.push_back({ActionInfo::Type::Fodder, wave, tick, fodder->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto symbol = fodder->symbol;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;
    auto choose = fodder->choose;
    auto waves = fodder->waves;

    auto f = [&info, idx, symbol, fodders, positions, choose, waves](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        std::vector<size_t> chosen;
        if (symbol == "C") {
            chosen.reserve(positions.size());
            for (size_t i = 0; i < positions.size(); i++) {
                chosen.push_back(i);
            }
        } else if (symbol == "C_POS") {
            chosen = choose_by_pos(w, positions, choose, waves);
        } else if (symbol == "C_NUM") {
            chosen = choose_by_num(w, positions, choose, waves);
        } else {
            assert(false && "unreachable");
        }

        for (auto i : chosen) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            info.action_infos[idx].plants.push_back({&p, p.uuid});
        }
    };
    ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&info, idx](pvz_emulator::world& w) {
            for (const auto& plant : info.action_infos[idx].plants) {
                if (plant.is_valid()) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        ops.push_back({tick + fodder->shovel_time - fodder->time, f});
    }
}

} // namespace _SmashInternal

std::vector<Op> load_round(const Setting& setting, const Round& round, Info& info, int giga_total)
{
    using namespace pvz_emulator::object;
    using namespace _SmashInternal;

    info = {};
    info.protect_positions.reserve(setting.protect_positions.size());
    info.giga_infos.reserve(giga_total);
    info.gen.seed(std::random_device {}());

    std::vector<Op> ops;

    int base_tick = 0;
    int latest_effect_time = 0;
    auto giga_rows = get_giga_rows(setting);

    insert_setup(ops, base_tick, setting.protect_positions);
    for (size_t i = 0; i < round.size(); i++) {
        const int wave_num = static_cast<int>(i + 1);
        const auto& wave = round[i];

        insert_spawn(
            ops, info, base_tick, wave_num, giga_total / static_cast<int>(round.size()), giga_rows);

        for (const auto& ice_time : wave.ice_times) {
            insert_ice(ops, base_tick + ice_time - 100);
        }

        for (const auto& action : wave.actions) {
            int effect_time = base_tick;
            if (auto a = std::get_if<Cob>(&action)) {
                insert_cob(ops, info, base_tick + a->time, wave_num, a, setting.scene_type);
                effect_time += a->time + 3;
            } else if (auto a = std::get_if<Jalapeno>(&action)) {
                insert_jalapeno(ops, info, base_tick + a->time, wave_num, a);
                effect_time += a->time;
            } else if (auto a = std::get_if<FixedFodder>(&action)) {
                insert_fixed_fodder(ops, info, base_tick + a->time, wave_num, a);
                effect_time += std::max(a->time, a->shovel_time);
            } else if (auto a = std::get_if<SmartFodder>(&action)) {
                insert_smart_fodder(ops, info, base_tick + a->time, wave_num, a);
                effect_time += std::max(a->time, a->shovel_time);
            } else {
                assert(false && "unreachable");
            }
            latest_effect_time = std::max(latest_effect_time, effect_time);
        }

        base_tick += wave.wave_length;
    }

    std::stable_sort(
        ops.begin(), ops.end(), [](const Op& a, const Op& b) { return a.tick < b.tick; });
    insert_spawn(ops, info, std::max(base_tick + 1, latest_effect_time + 1),
        static_cast<int>(round.size()) + 1, 0, {}); // make sure giga info is synced at the end

    return ops;
}