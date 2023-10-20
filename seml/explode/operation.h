#pragma once

#include <functional>

#include "common/pe.h"
#include "constants/constants.h"
#include "seml/operation.h"
#include "seml/reader/types.h"
#include "types.h"

namespace _ExplodeInternal {

const int PLANT_INIT_HP = 2147483647 / 2;

void insert_setup(Test& test, int tick, const std::vector<Setting::ProtectPos>& protect_positions)
{
    auto f = [&test, protect_positions](pvz_emulator::world& w) {
        // plant umbrella leafs to ignore catapults
        for (int row = 0; row < w.scene.get_max_row(); row++) {
            w.plant_factory.create(pvz_emulator::object::plant_type::umbrella_leaf, row, 0);
        }

        for (const auto& pos : protect_positions) {
            auto plant_type = is_cob(pos) ? pvz_emulator::object::plant_type::cob_cannon
                                          : pvz_emulator::object::plant_type::umbrella_leaf;
            auto col = is_cob(pos) ? pos.col - 2 : pos.col - 1;

            auto& p = w.plant_factory.create(plant_type, pos.row - 1, col);
            p.ignore_jack_explode = true;
            p.hp = p.max_hp = PLANT_INIT_HP;
            test.plants.push_back(&p);
        }
    };
    test.ops.push_back({tick, f});
}

void insert_spawn(Test& test, int tick)
{
    using namespace pvz_emulator::object;
    auto f = [tick](pvz_emulator::world& w) {
        for (const auto& zombie_type : {zombie_type::jack_in_the_box, zombie_type::ladder,
                 zombie_type::football, zombie_type::catapult}) {
            for (int i = 0; i < 5; i++) {
                w.zombie_factory.create(zombie_type);
            }
        }
    };
    test.ops.push_back({tick, f});
}

void insert_ice(Test& test, int tick)
{
    auto f = [](pvz_emulator::world& w) {
        w.plant_factory.create(pvz_emulator::object::plant_type::iceshroom, 0, 0);
    };
    test.ops.push_back({tick, f});
}

void insert_cob(
    Test& test, int tick, const Cob* cob, const pvz_emulator::object::scene_type& scene_type)
{
    auto cob_col = cob->cob_col;

    for (const auto& pos : cob->positions) {
        auto f = [&test, pos, cob_col](
                     pvz_emulator::world& w) { launch_cob(w, pos.row, pos.col, cob_col); };

        int cob_fly_time;
        if (is_backyard(scene_type) && (pos.row == 3 || pos.row == 4)) {
            cob_fly_time = 378;
        } else if (is_roof(scene_type)) {
            cob_fly_time = get_roof_cob_fly_time(pos.col, cob_col);
        } else {
            cob_fly_time = 373;
        }

        test.ops.push_back({tick - cob_fly_time, f});
    }
}

void insert_jalapeno(Test& test, int tick, const Jalapeno* jalapeno)
{
    auto pos = jalapeno->position;

    auto f = [&test, pos](pvz_emulator::world& w) {
        w.plant_factory.create(
            pvz_emulator::object::plant_type::jalapeno, pos.row - 1, pos.col - 1);
    };
    test.ops.push_back({tick - 100, f});
}

void insert_fixed_fodder(Test& test, int tick, const FixedFodder* fodder)
{
    test.fodders.push_back({});
    auto idx = test.fodders.size() - 1;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;

    auto f = [&test, idx, fodders, positions](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        for (int i = 0; i < fodders.size(); i++) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            test.fodders[idx].push_back({&p, p.uuid});
        }
    };
    test.ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& fodder : test.fodders[idx]) {
                if (fodder.ptr && fodder.ptr->uuid == fodder.uuid) {
                    w.plant_factory.destroy(*fodder.ptr);
                }
            }
        };
        test.ops.push_back({tick + fodder->shovel_time - fodder->time, f});
    }
}

void insert_smart_fodder(Test& test, int tick, const SmartFodder* fodder)
{
    test.fodders.push_back({});
    auto idx = test.fodders.size() - 1;
    auto symbol = fodder->symbol;
    auto fodders = fodder->fodders;
    auto positions = fodder->positions;
    auto choose = fodder->choose;
    auto waves = fodder->waves;

    auto f = [&test, idx, symbol, fodders, positions, choose, waves](pvz_emulator::world& w) {
        assert(fodders.size() == positions.size());

        std::vector<int> chosen;
        if (symbol == "C") {
            chosen.reserve(positions.size());
            for (int i = 0; i < positions.size(); i++) {
                chosen.push_back(i);
            }
        } else if (symbol == "C_POS") {
            chosen = choose_by_pos(w, positions, choose, waves);
        } else if (symbol == "C_NUM") {
            chosen = choose_by_num(w, positions, choose, waves);
        } else {
            assert(false && "unreachable");
        }

        for (int i : chosen) {
            auto& p = plant_fodder(w, fodders[i], positions[i]);
            test.fodders[idx].push_back({&p, p.uuid});
        }
    };
    test.ops.push_back({tick, f});

    if (fodder->shovel_time != -1) {
        auto f = [&test, idx](pvz_emulator::world& w) {
            for (const auto& fodder : test.fodders[idx]) {
                if (fodder.ptr && fodder.ptr->uuid == fodder.uuid) {
                    w.plant_factory.destroy(*fodder.ptr);
                }
            }
        };
        test.ops.push_back({tick + fodder->shovel_time - fodder->time, f});
    }
}

} // namespace _ExplodeInternal

void load_config(const Config& config, int wave_num, Test& test)
{
    using namespace pvz_emulator::object;
    using namespace _ExplodeInternal;

    test = {};

    const auto& wave = config.waves[wave_num];

    test.loss_infos.reserve(wave.wave_length - wave.start_tick + 1);
    test.plants.reserve(config.setting.protect_positions.size());

    insert_setup(test, 0, config.setting.protect_positions);
    insert_spawn(test, 0);

    for (const auto& ice_time : wave.ice_times) {
        insert_ice(test, ice_time - 100);
    }

    for (const auto& action : wave.actions) {
        if (auto a = std::get_if<Cob>(&action)) {
            insert_cob(test, a->time, a, config.setting.scene_type);
        } else if (auto a = std::get_if<Jalapeno>(&action)) {
            insert_jalapeno(test, a->time, a);
        } else if (auto a = std::get_if<FixedFodder>(&action)) {
            insert_fixed_fodder(test, a->time, a);
        } else if (auto a = std::get_if<SmartFodder>(&action)) {
            insert_smart_fodder(test, a->time, a);
        } else {
            assert(false && "unreachable");
        }
    }
    std::stable_sort(
        test.ops.begin(), test.ops.end(), [](const Op& a, const Op& b) { return a.tick < b.tick; });
}
