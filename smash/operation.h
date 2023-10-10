#pragma once

#include <functional>

#include "common/pe.h"
#include "reader.h"

namespace _SmashInternal {

void insert_setup(
    std::vector<Op>& ops, int tick, const std::vector<Setting::ProtectPos>& protect_positions)
{
    auto f = [protect_positions](pvz_emulator::world& w) {
        for (const auto& pos : protect_positions) {
            auto& p = w.plant_factory.create((pos.type == Setting::ProtectPos::Type::Cob)
                    ? pvz_emulator::object::plant_type::cob_cannon
                    : pvz_emulator::object::plant_type::wallnut,
                pos.row - 1, pos.col - 1);
            p.ignore_garg_smash = true;
        }
    };
    ops.push_back({tick, f});
}

void insert_spawn(std::vector<Op>& ops, Info& info, int tick, int wave, int garg_num)
{
    auto f = [&info, wave, garg_num, tick](pvz_emulator::world& w) {
        // sync current gargs
        for (auto& garg_info : info.garg_infos) {
            auto& zombie = garg_info.zombie;

            if (zombie.ptr && zombie.ptr->uuid == zombie.uuid) {
                garg_info.alive_time = garg_info.zombie.ptr->time_since_spawn;

                garg_info.hit_by_cob.clear();
                for (int i = 0; i < zombie.ptr->hit_by_cob.size; i++) {
                    garg_info.hit_by_cob.insert(zombie.ptr->hit_by_cob.arr[i]);
                }

                garg_info.attempted_smashes.clear();
                for (int i = 0; i < zombie.ptr->attempted_smashes.size; i++) {
                    garg_info.attempted_smashes.insert(zombie.ptr->attempted_smashes.arr[i]);
                }

                garg_info.ignored_smashes.clear();
                for (int i = 0; i < zombie.ptr->ignored_smashes.size; i++) {
                    garg_info.ignored_smashes.insert(zombie.ptr->ignored_smashes.arr[i]);
                }
            }
        }

        w.scene.spawn.wave = wave;

        for (int i = 0; i < garg_num; i++) {
            auto& z = w.zombie_factory.create(pvz_emulator::object::zombie_type::giga_gargantuar);
            info.garg_infos.push_back({{&z, z.uuid}, z.row, wave, tick, 0, {}, {}, {}});
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

void insert_cob(
    std::vector<Op>& ops, Info& info, int tick, int wave, const Cob* cob, bool is_backyard)
{
    info.action_infos.push_back(
        {_SmashInternal::ActionInfo::Type::Cob, wave, tick, cob->desc(), {}});
    auto idx = info.action_infos.size() - 1;

    for (const auto& pos : cob->positions) {
        auto f = [&info, idx, pos](pvz_emulator::world& w) {
            info.action_infos[idx].plants.push_back({nullptr, launch_cob(w, pos.row, pos.col)});
        };

        int cob_fly_time = (is_backyard && (pos.row == 3 || pos.row == 4)) ? 378 : 373;
        ops.push_back({tick - cob_fly_time, f});
    }
}

void insert_fixed_fodder(
    std::vector<Op>& ops, Info& info, int tick, int wave, const FixedFodder* fodder)
{
    info.action_infos.push_back(
        {_SmashInternal::ActionInfo::Type::Card, wave, tick, fodder->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto positions = fodder->positions;

    auto f = [&info, idx, positions](pvz_emulator::world& w) {
        for (const auto& pos : positions) {
            auto plant_type = (pos.type == FodderPos::Type::Normal)
                ? pvz_emulator::object::plant_type::wallnut
                : pvz_emulator::object::plant_type::sunshroom;
            auto& p = w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
            info.action_infos[idx].plants.push_back({&p, p.uuid});
        }
    };
    ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&info, idx](pvz_emulator::world& w) {
            for (const auto& plant : info.action_infos[idx].plants) {
                if (plant.ptr && plant.ptr->uuid == plant.uuid) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        ops.push_back({tick + fodder->shovel_time - fodder->time, f});
    }
}

} // namespace _SmashInternal

std::vector<Op> load_config(const Config& config, Info& info, int garg_total = 1000)
{
    using namespace pvz_emulator::object;
    using namespace _SmashInternal;

    info = {};
    info.protect_positions.reserve(config.setting.protect_positions.size());
    info.garg_infos.reserve(garg_total);

    std::vector<Op> ops;

    int garg_num_per_wave = std::max(garg_total / config.waves.size(), 1ull);

    insert_setup(ops, 0, config.setting.protect_positions);

    int base_tick = 0;
    for (int i = 0; i < config.waves.size(); i++) {
        const int wave_num = i + 1;
        const auto& wave = config.waves[i];

        insert_spawn(ops, info, base_tick, wave_num, garg_num_per_wave);

        for (const auto& ice_time : wave.ice_times) {
            insert_ice(ops, base_tick + ice_time - 100);
        }

        for (const auto& action : wave.actions) {
            if (auto a = std::get_if<Cob>(&action)) {
                insert_cob(ops, info, base_tick + a->time, wave_num, a,
                    is_backyard(config.setting.scene_type));
            } else if (auto a = std::get_if<FixedFodder>(&action)) {
                insert_fixed_fodder(ops, info, base_tick + a->time, wave_num, a);
            } else {
                assert(false && "unimplemented");
            }
        }

        base_tick += wave.wave_length;
    }

    std::stable_sort(
        ops.begin(), ops.end(), [](const Op& a, const Op& b) { return a.tick < b.tick; });
    insert_spawn(ops, info, ops.back().tick + 1, config.waves.size() + 1, 0);

    return ops;
}