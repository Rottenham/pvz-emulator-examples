#pragma once

#include "common/pe.h"
#include "constants/constants.h"
#include "seml/operation.h"
#include "types.h"

namespace _explode_internal {

using scene_type = pvz_emulator::object::scene_type;
using plant_type = pvz_emulator::object::plant_type;
using zombie_type = pvz_emulator::object::zombie_type;

const int PLANT_INIT_HP = 2147483647 / 2;

void insert_setup(Test& test, int tick, const std::vector<Setting::ProtectPos>& protect_positions)
{
    auto f = [&test, protect_positions](pvz_emulator::world& w) {
        // plant umbrella leafs to ignore catapults
        for (int row = 0; row < static_cast<int>(w.scene.get_max_row()); row++) {
            w.plant_factory.create(plant_type::umbrella_leaf, row, 0);
        }

        for (const auto& pos : protect_positions) {
            auto plant_type = pos.is_cob() ? plant_type::cob_cannon : plant_type::umbrella_leaf;

            auto& p = w.plant_factory.create(
                plant_type, pos.row - 1, pos.is_cob() ? pos.col - 2 : pos.col - 1);
            p.ignore_jack_explode = true;
            p.hp = p.max_hp = PLANT_INIT_HP;
            test.protect_plants.push_back(&p);
        }
    };
    test.ops.push_back({tick, f});
}

void insert_spawn(Test& test, int tick)
{
    auto f = [tick](pvz_emulator::world& w) {
        for (const auto& type : {zombie_type::jack_in_the_box, zombie_type::ladder,
                 zombie_type::football, zombie_type::catapult}) {
            for (int r = 0; r < 5; r++) {
                w.zombie_factory.create(type);
            }
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

void insert_cob(Test& test, int tick, const Cob* cob, const scene_type& scene_type)
{
    auto cob_col = cob->cob_col;

    for (const auto& pos : cob->positions) {
        auto f = [&test, pos, cob_col](
                     pvz_emulator::world& w) { launch_cob(w, pos.row, pos.col, cob_col); };
        test.ops.push_back({tick - get_cob_fly_time(scene_type, pos.row, pos.col, cob_col), f});
    }
}

void insert_fixed_card(Test& test, int tick, const FixedCard* fixed_card)
{
    test.plants_to_be_shoveled.push_back({});
    auto idx = test.plants_to_be_shoveled.size() - 1;
    auto plant_type = fixed_card->plant_type;
    auto pos = fixed_card->position;

    auto f = [&test, idx, plant_type, pos](pvz_emulator::world& w) {
        auto& p = w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
        test.plants_to_be_shoveled[idx].push_back({&p, p.uuid});
    };
    test.ops.push_back({get_fixed_card_op_tick(fixed_card, tick), f});

    if (fixed_card->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (auto plant : test.plants_to_be_shoveled[idx]) {
                if (plant.is_valid()) {
                    w.plant_factory.destroy(*plant.ptr);
                }
            }
        };
        test.ops.push_back({tick - fixed_card->time + fixed_card->shovel_time, f});
    }
}

void insert_smart_card(Test& test, int tick, const SmartCard* smart_card)
{
    auto plant_type = smart_card->plant_type;
    auto positions = smart_card->positions;
    int max_card_zombie_row_diff = get_smart_card_max_card_zombie_row_diff(smart_card);

    auto f = [&test, plant_type, positions, max_card_zombie_row_diff](pvz_emulator::world& w) {
        auto chosen = choose_by_num(w, positions, 1, {},
            {zombie_type::giga_gargantuar, zombie_type::gargantuar}, max_card_zombie_row_diff);
        assert(chosen.size() == 1);

        auto pos = positions[chosen[0]];
        w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
    };
    test.ops.push_back({get_smart_card_op_tick(smart_card, tick), f});
}

void insert_fixed_fodder(Test& test, int tick, const FixedFodder* fodder)
{
    test.plants_to_be_shoveled.push_back({});
    auto idx = test.plants_to_be_shoveled.size() - 1;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;

    auto f = [&test, idx, fodders, positions](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        for (size_t i = 0; i < fodders.size(); i++) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            test.plants_to_be_shoveled[idx].push_back({&p, p.uuid});
        }
    };
    test.ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& fodder : test.plants_to_be_shoveled[idx]) {
                if (fodder.is_valid()) {
                    w.plant_factory.destroy(*fodder.ptr);
                }
            }
        };
        test.ops.push_back({tick - fodder->time + fodder->shovel_time, f});
    }
}

void insert_smart_fodder(Test& test, int tick, const SmartFodder* fodder)
{
    test.plants_to_be_shoveled.push_back({});
    auto idx = test.plants_to_be_shoveled.size() - 1;
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
            test.plants_to_be_shoveled[idx].push_back({&p, p.uuid});
        }
    };
    test.ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& fodder : test.plants_to_be_shoveled[idx]) {
                if (fodder.is_valid()) {
                    w.plant_factory.destroy(*fodder.ptr);
                }
            }
        };
        test.ops.push_back({tick - fodder->time + fodder->shovel_time, f});
    }
}

} // namespace _explode_internal

void load_wave(const Setting& setting, const Wave& wave, Test& test)
{
    using namespace _explode_internal;

    test = {};
    test.start_tick = wave.start_tick;
    test.loss_infos.reserve(wave.wave_length - wave.start_tick + 1);
    test.protect_plants.reserve(setting.protect_positions.size());

    int base_tick = 0;
    insert_setup(test, base_tick, setting.protect_positions);
    insert_spawn(test, base_tick);

    for (const auto& ice_time : wave.ice_times) {
        insert_ice(test, base_tick + ice_time - 99);
    }

    for (const auto& action : wave.actions) {
        if (auto a = dynamic_cast<const Cob*>(action.get())) {
            insert_cob(test, base_tick + a->time, a, setting.scene_type);
        } else if (auto a = dynamic_cast<const FixedCard*>(action.get())) {
            insert_fixed_card(test, base_tick + a->time, a);
        } else if (auto a = dynamic_cast<const SmartCard*>(action.get())) {
            insert_smart_card(test, base_tick + a->time, a);
        } else if (auto a = dynamic_cast<const FixedFodder*>(action.get())) {
            insert_fixed_fodder(test, base_tick + a->time, a);
        } else if (auto a = dynamic_cast<const SmartFodder*>(action.get())) {
            insert_smart_fodder(test, base_tick + a->time, a);
        } else {
            assert(false && "unreachable");
        }
    }

    std::stable_sort(
        test.ops.begin(), test.ops.end(), [](const Op& a, const Op& b) { return a.tick < b.tick; });
}
