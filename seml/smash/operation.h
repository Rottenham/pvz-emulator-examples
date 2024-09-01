#pragma once

#include <functional>

#include "common/pe.h"
#include "constants/constants.h"
#include "seml/operation.h"
#include "seml/types.h"
#include "types.h"

namespace _smash_internal {

using scene_type = pvz_emulator::object::scene_type;
using plant_type = pvz_emulator::object::plant_type;
using zombie_type = pvz_emulator::object::zombie_type;

std::vector<int> get_giga_rows(const Setting& setting)
{
    std::vector<int> giga_rows;
    giga_rows.reserve(setting.protect_positions.size());
    for (const auto& protect_position : setting.protect_positions) {
        giga_rows.push_back(protect_position.row - 1);
    }
    return giga_rows;
}

int pick_giga_row(std::mt19937& rnd, const std::vector<int>& giga_rows)
{
    if (giga_rows.empty()) {
        return -1;
    } else {
        std::uniform_int_distribution<size_t> distribution(0, giga_rows.size() - 1);
        auto index = distribution(rnd);
        return giga_rows[index];
    }
}

void insert_setup(Test& test, int tick, const std::vector<Setting::ProtectPos>& protect_positions)
{
    auto f = [protect_positions](pvz_emulator::world& w) {
        for (const auto& pos : protect_positions) {
            auto plant_type = pos.is_cob() ? plant_type::cob_cannon : plant_type::umbrella_leaf;
            auto col = pos.is_cob() ? pos.col - 2 : pos.col - 1;

            auto& p = w.plant_factory.create(plant_type, pos.row - 1, col);
            p.ignore_garg_smash = true;
        }
    };
    test.ops.push_back({tick, f});
}

void insert_spawn(Test& test, int tick, int wave, int giga_num, const std::vector<int>& giga_rows)
{
    auto f = [&test, tick, wave, giga_num, giga_rows](pvz_emulator::world& w) {
        // sync current gigas
        for (auto& giga_info : test.giga_infos) {
            auto& zombie = giga_info.zombie;

            if (zombie.is_valid()) {
                giga_info.alive_time = giga_info.zombie.ptr->time_since_spawn;

                giga_info.hit_by_ash.clear();
                giga_info.hit_by_ash.reserve(zombie.ptr->hit_by_ash.size);
                for (int i = 0; i < zombie.ptr->hit_by_ash.size; i++) {
                    giga_info.hit_by_ash.insert(zombie.ptr->hit_by_ash.arr[i]);
                }

                giga_info.attempted_smashes.clear();
                giga_info.attempted_smashes.reserve(zombie.ptr->attempted_smashes.size);
                for (int i = 0; i < zombie.ptr->attempted_smashes.size; i++) {
                    giga_info.attempted_smashes.insert(zombie.ptr->attempted_smashes.arr[i]);
                }

                giga_info.ignored_smashes.clear();
                giga_info.ignored_smashes.reserve(zombie.ptr->ignored_smashes.size);
                for (int i = 0; i < zombie.ptr->ignored_smashes.size; i++) {
                    giga_info.ignored_smashes.insert(zombie.ptr->ignored_smashes.arr[i]);
                }
            }
        }

        w.scene.spawn.wave = wave;

        for (int i = 0; i < giga_num; i++) {
            auto& z = w.zombie_factory.create(
                zombie_type::giga_gargantuar, pick_giga_row(test.rnd, giga_rows));
            test.giga_infos.push_back({{&z, z.uuid}, z.row, wave, tick, 0, {}, {}, {}});
        }
    };
    test.ops.push_back({tick, f});
}

void insert_ice(Test& test, int tick)
{
    auto f = [](pvz_emulator::world& w) {
        w.plant_factory.create(plant_type::iceshroom, 0, 0, plant_type::none, true);
    };
    test.ops.push_back({tick, f});
}

void insert_cob(Test& test, int tick, int wave, const Cob* cob, const scene_type& scene_type)
{
    test.action_infos.push_back({ActionInfo::Type::Ash, wave, tick, cob->desc(), {}});
    auto idx = test.action_infos.size() - 1;
    auto cob_col = cob->cob_col;

    for (const auto& pos : cob->positions) {
        auto f = [&test, idx, pos, cob_col](pvz_emulator::world& w) {
            test.action_infos[idx].plants.push_back(
                {nullptr, launch_cob(w, pos.row, pos.col, cob_col)});
        };

        test.ops.push_back({tick - get_cob_fly_time(scene_type, pos.row, pos.col, cob_col), f});
    }
}

void insert_fixed_card(Test& test, int tick, int wave, const FixedCard* fixed_card)
{
    auto plant_type = fixed_card->plant_type;

    ActionInfo::Type action_info_type;
    if (plant_type == plant_type::jalapeno || plant_type == plant_type::cherry_bomb
        || plant_type == plant_type::squash) {
        action_info_type = ActionInfo::Type::Ash;
    } else {
        action_info_type = ActionInfo::Type::Fodder;
    }

    test.action_infos.push_back({action_info_type, wave, tick, fixed_card->desc(), {}});

    auto idx = test.action_infos.size() - 1;
    auto pos = fixed_card->position;

    auto f = [&test, idx, plant_type, pos](pvz_emulator::world& w) {
        auto& p = w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
        test.action_infos[idx].plants.push_back({&p, p.uuid});
    };
    test.ops.push_back({get_fixed_card_op_tick(fixed_card, tick), f});

    if (fixed_card->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& plant : test.action_infos[idx].plants) {
                if (plant.is_valid()) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        test.ops.push_back({tick - fixed_card->time + fixed_card->shovel_time, f});
    }
}

void insert_smart_card(Test& test, int tick, int wave, const SmartCard* smart_card)
{
    auto plant_type = smart_card->plant_type;

    test.action_infos.push_back({ActionInfo::Type::Ash, wave, tick, smart_card->desc(), {}});

    auto idx = test.action_infos.size() - 1;
    auto positions = smart_card->positions;
    int max_card_zombie_row_diff = get_smart_card_max_card_zombie_row_diff(smart_card);

    auto f = [&test, idx, plant_type, positions, max_card_zombie_row_diff](pvz_emulator::world& w) {
        auto chosen = choose_by_num(w, positions, 1, {},
            {zombie_type::giga_gargantuar, zombie_type::gargantuar}, max_card_zombie_row_diff);
        assert(chosen.size() == 1);

        auto pos = positions[chosen[0]];
        auto& p = w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
        test.action_infos[idx].plants.push_back({&p, p.uuid});
    };
    test.ops.push_back({get_smart_card_op_tick(smart_card, tick), f});
}

void insert_fixed_fodder(Test& test, int tick, int wave, const FixedFodder* fodder)
{
    test.action_infos.push_back({ActionInfo::Type::Fodder, wave, tick, fodder->desc(), {}});
    auto idx = test.action_infos.size() - 1;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;

    auto f = [&test, idx, fodders, positions](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        for (size_t i = 0; i < fodders.size(); i++) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            test.action_infos[idx].plants.push_back({&p, p.uuid});
        }
    };
    test.ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& plant : test.action_infos[idx].plants) {
                if (plant.is_valid()) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        test.ops.push_back({tick - fodder->time + fodder->shovel_time, f});
    }
}

void insert_smart_fodder(Test& test, int tick, int wave, const SmartFodder* fodder)
{
    test.action_infos.push_back({ActionInfo::Type::Fodder, wave, tick, fodder->desc(), {}});
    auto idx = test.action_infos.size() - 1;
    auto symbol = fodder->symbol;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;
    auto choose = fodder->choose;
    auto waves = fodder->waves;

    auto f = [&test, idx, symbol, fodders, positions, choose, waves](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        std::vector<size_t> chosen;
        if (symbol == "C") {
            chosen.reserve(positions.size());
            for (size_t i = 0; i < positions.size(); i++) {
                chosen.push_back(i);
            }
        } else if (symbol == "C_POS") {
            chosen = choose_by_giga_pos(w, positions, choose, waves);
        } else if (symbol == "C_NUM") {
            chosen = choose_by_num(w, positions, choose, waves,
                {zombie_type::ladder, zombie_type::jack_in_the_box}, 0);
        } else {
            assert(false && "unreachable");
        }

        for (auto i : chosen) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            test.action_infos[idx].plants.push_back({&p, p.uuid});
        }
    };
    test.ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& plant : test.action_infos[idx].plants) {
                if (plant.is_valid()) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        test.ops.push_back({tick - fodder->time + fodder->shovel_time, f});
    }
}

} // namespace _smash_internal

void load_config(const Config& config, Test& test)
{
    using namespace pvz_emulator::object;
    using namespace _smash_internal;

    test = {};
    test.protect_positions.reserve(config.setting.protect_positions.size());
    test.giga_infos.reserve(config.waves.size() * 5);
    test.rnd.seed(std::random_device {}());

    int base_tick = 0;
    auto giga_rows = get_giga_rows(config.setting);

    insert_setup(test, base_tick, config.setting.protect_positions);
    for (size_t i = 0; i < config.waves.size(); i++) {
        const int wave_num = static_cast<int>(i + 1);
        const auto& wave = config.waves[i];

        insert_spawn(test, base_tick, wave_num, 5, giga_rows);

        for (const auto& ice_time : wave.ice_times) {
            insert_ice(test, base_tick + ice_time - 99);
        }

        for (const auto& action : wave.actions) {
            if (auto a = dynamic_cast<const Cob*>(action.get())) {
                insert_cob(test, base_tick + a->time, wave_num, a, config.setting.scene_type);
            } else if (auto a = dynamic_cast<const FixedCard*>(action.get())) {
                insert_fixed_card(test, base_tick + a->time, wave_num, a);
            } else if (auto a = dynamic_cast<const SmartCard*>(action.get())) {
                insert_smart_card(test, base_tick + a->time, wave_num, a);
            } else if (auto a = dynamic_cast<const FixedFodder*>(action.get())) {
                insert_fixed_fodder(test, base_tick + a->time, wave_num, a);
            } else if (auto a = dynamic_cast<const SmartFodder*>(action.get())) {
                insert_smart_fodder(test, base_tick + a->time, wave_num, a);
            } else {
                assert(false && "unreachable");
            }
        }

        base_tick += wave.wave_length;
    }

    test.ops.erase(std::remove_if(test.ops.begin(), test.ops.end(),
                       [base_tick](const Op& op) { return op.tick > base_tick; }),
        test.ops.end());
    std::stable_sort(
        test.ops.begin(), test.ops.end(), [](const Op& a, const Op& b) { return a.tick < b.tick; });

    insert_spawn(test, base_tick + 1, static_cast<int>(config.waves.size()) + 1, 0,
        {}); // make sure giga test is synced at the end
}