#pragma once

#include <functional>

#include "common/pe.h"
#include "reader.h"

namespace _SmashInternal {

std::vector<int> get_giga_rows(const Config& config)
{
    std::vector<int> giga_rows;
    giga_rows.reserve(config.setting.protect_positions.size());
    for (const auto& protect_position : config.setting.protect_positions) {
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
            auto& p = w.plant_factory.create((pos.type == Setting::ProtectPos::Type::Cob)
                    ? pvz_emulator::object::plant_type::cob_cannon
                    : pvz_emulator::object::plant_type::wallnut,
                pos.row - 1, pos.col - 1);
            p.ignore_garg_smash = true;
        }
    };
    ops.push_back({tick, f});
}

void insert_spawn(std::vector<Op>& ops, Info& info, int tick, int wave, int giga_num,
    const std::vector<int>& giga_rows)
{
    auto f = [&info, wave, giga_num, tick, giga_rows](pvz_emulator::world& w) {
        // sync current gigas
        for (auto& giga_info : info.giga_infos) {
            auto& zombie = giga_info.zombie;

            if (zombie.ptr && zombie.ptr->uuid == zombie.uuid) {
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

void insert_cob(
    std::vector<Op>& ops, Info& info, int tick, int wave, const Cob* cob, bool is_backyard)
{
    info.action_infos.push_back(
        {_SmashInternal::ActionInfo::Type::Ash, wave, tick, cob->desc(), {}});
    auto idx = info.action_infos.size() - 1;

    for (const auto& pos : cob->positions) {
        auto f = [&info, idx, pos](pvz_emulator::world& w) {
            info.action_infos[idx].plants.push_back({nullptr, launch_cob(w, pos.row, pos.col)});
        };

        int cob_fly_time = (is_backyard && (pos.row == 3 || pos.row == 4)) ? 378 : 373;
        ops.push_back({tick - cob_fly_time, f});
    }
}

pvz_emulator::object::plant& plant(pvz_emulator::world& w, const Fodder& card, const FodderPos& pos)
{
    auto plant_type = pvz_emulator::object::plant_type::wallnut;
    if (card == Fodder::Puff) {
        plant_type = pvz_emulator::object::plant_type::sunshroom;
    } else if (card == Fodder::Pot) {
        plant_type = pvz_emulator::object::plant_type::flower_pot;
    }
    return w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
}

void insert_fixed_fodder(
    std::vector<Op>& ops, Info& info, int tick, int wave, const FixedFodder* fodder)
{
    info.action_infos.push_back(
        {_SmashInternal::ActionInfo::Type::Fodder, wave, tick, fodder->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto cards = fodder->fodders;
    auto positions = fodder->positions;

    auto f = [&info, idx, cards, positions](pvz_emulator::world& w) {
        assert(cards.size() == positions.size());

        for (int i = 0; i < cards.size(); i++) {
            auto& p = plant(w, cards[i], positions[i]);
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

std::vector<int> choose_by_pos(pvz_emulator::world& w, const std::vector<FodderPos>& positions,
    int choose, const std::unordered_set<int>& waves)
{
    const int GIGA_X_MAX = 1000;
    std::array<int, 6> giga_min_x;
    std::fill(giga_min_x.begin(), giga_min_x.end(), GIGA_X_MAX);
    for (const auto& z : w.scene.zombies) {
        if (!z.is_dead && z.is_not_dying
            && z.type == pvz_emulator::object::zombie_type::giga_gargantuar
            && (waves.empty() || waves.count(z.spawn_wave))
            && (giga_min_x[z.row] == 0 || z.int_x < giga_min_x[z.row])) {
            giga_min_x[z.row] = z.int_x;
        }
    }

    std::unordered_set<int> indices;
    indices.reserve(positions.size());
    for (int i = 0; i < positions.size(); i++) {
        indices.insert(i);
    }

    std::vector<int> res;
    res.reserve(choose);
    for (int i = 0; i < choose; i++) {
        int min_x = GIGA_X_MAX, best_index = -1;
        for (const auto& index : indices) {
            if (giga_min_x[positions[index].row - 1] < min_x) {
                min_x = giga_min_x[positions[index].row - 1];
                best_index = index;
            }
        }
        if (best_index == -1) {
            break;
        } else {
            indices.erase(best_index);
            res.push_back(best_index);
        }
    }
    return res;
}

std::vector<int> choose_by_num(pvz_emulator::world& w, const std::vector<FodderPos>& positions,
    int choose, const std::unordered_set<int>& waves)
{
    std::array<int, 6> ladder_jack_count = {};
    for (const auto& z : w.scene.zombies) {
        if (!z.is_dead && z.is_not_dying
            && (z.type == pvz_emulator::object::zombie_type::ladder
                || z.type == pvz_emulator::object::zombie_type::jack_in_the_box)
            && (waves.empty() || waves.count(z.spawn_wave))) {
            ladder_jack_count[z.row]++;
        }
    }

    std::unordered_set<int> indices;
    indices.reserve(positions.size());
    for (int i = 0; i < positions.size(); i++) {
        indices.insert(i);
    }

    std::vector<int> res;
    res.reserve(choose);
    for (int i = 0; i < choose; i++) {
        int max_count = -1, best_index = -1;
        for (const auto& index : indices) {
            if (ladder_jack_count[positions[index].row - 1] > max_count) {
                max_count = ladder_jack_count[positions[index].row - 1];
                best_index = index;
            }
        }
        if (best_index == -1) {
            break;
        } else {
            indices.erase(best_index);
            res.push_back(best_index);
        }
    }
    return res;
}

void insert_smart_fodder(
    std::vector<Op>& ops, Info& info, int tick, int wave, const SmartFodder* fodder)
{
    info.action_infos.push_back(
        {_SmashInternal::ActionInfo::Type::Fodder, wave, tick, fodder->desc(), {}});
    auto idx = info.action_infos.size() - 1;
    auto symbol = fodder->symbol;
    auto cards = fodder->fodders;
    auto positions = fodder->positions;
    auto choose = fodder->choose;
    auto waves = fodder->waves;

    auto f = [&info, idx, symbol, cards, positions, choose, waves](pvz_emulator::world& w) {
        assert(cards.size() == positions.size());

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
            auto& p = plant(w, cards[i], positions[i]);
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

std::vector<Op> load_config(const Config& config, Info& info)
{
    using namespace pvz_emulator::object;
    using namespace _SmashInternal;

    info = {};
    info.protect_positions.reserve(config.setting.protect_positions.size());
    info.giga_infos.reserve(config.setting.giga_total);
    info.action_infos.reserve(config.setting.action_count);
    info.gen.seed(static_cast<unsigned>(std::random_device {}()));

    std::vector<Op> ops;
    ops.reserve(config.setting.op_count);

    int base_tick = 0;
    int latest_effect_time = 0;
    auto giga_rows = get_giga_rows(config);

    insert_setup(ops, base_tick, config.setting.protect_positions);
    for (int i = 0; i < config.waves.size(); i++) {
        const int wave_num = i + 1;
        const auto& wave = config.waves[i];

        insert_spawn(ops, info, base_tick, wave_num,
            std::max(config.setting.giga_total / static_cast<int>(config.waves.size()), 1),
            giga_rows);

        for (const auto& ice_time : wave.ice_times) {
            insert_ice(ops, base_tick + ice_time - 100);
        }

        for (const auto& action : wave.actions) {
            int effect_time = base_tick;
            if (auto a = std::get_if<Cob>(&action)) {
                insert_cob(ops, info, base_tick + a->time, wave_num, a,
                    is_backyard(config.setting.scene_type));
                effect_time += a->time + 3;
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
        static_cast<int>(config.waves.size()) + 1, 0,
        {}); // make sure giga info is synced at the end

    return ops;
}