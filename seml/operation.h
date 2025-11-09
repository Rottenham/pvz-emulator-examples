#pragma once

#include <optional>

#include "constants/constants.h"
#include "reader/types.h"
#include "world.h"

pvz_emulator::object::plant& plant_fodder(
    pvz_emulator::world& w, const Fodder& fodder, const CardPos& pos)
{
    auto plant_type = pvz_emulator::object::plant_type::umbrella_leaf;
    if (fodder == Fodder::Puff) {
        plant_type = pvz_emulator::object::plant_type::sunshroom;
    } else if (fodder == Fodder::Pot) {
        plant_type = pvz_emulator::object::plant_type::flower_pot;
    }
    return w.plant_factory.create(plant_type, pos.row - 1, pos.col - 1);
}

std::vector<size_t> choose_by_giga_pos(pvz_emulator::world& w,
    const std::vector<CardPos>& positions, int choose, const std::unordered_set<int>& waves)
{
    const int GIGA_X_MAX = 1000;
    std::array<int, 6> giga_min_x;
    std::fill(giga_min_x.begin(), giga_min_x.end(), GIGA_X_MAX);
    for (const auto& z : w.scene.zombies) {
        if (!z.is_dead && z.is_not_dying
            && z.type == pvz_emulator::object::zombie_type::giga_gargantuar
            && (waves.empty() || waves.count(z.spawn_wave)) && (z.int_x < giga_min_x[z.row])) {
            giga_min_x[z.row] = z.int_x;
        }
    }

    std::unordered_set<size_t> indices;
    indices.reserve(positions.size());
    for (size_t i = 0; i < positions.size(); i++) {
        indices.insert(i);
    }

    std::vector<size_t> res;
    res.reserve(choose);
    for (int i = 0; i < choose; i++) {
        int min_x = GIGA_X_MAX;
        std::optional<size_t> best_index;
        for (const auto& index : indices) {
            if (giga_min_x[positions[index].row - 1] < min_x) {
                min_x = giga_min_x[positions[index].row - 1];
                best_index = index;
            }
        }
        if (!best_index.has_value()) {
            break;
        } else {
            indices.erase(*best_index);
            res.push_back(*best_index);
        }
    }
    return res;
}

std::vector<size_t> choose_by_num(pvz_emulator::world& w,
    const std::vector<CardPos>& card_positions, int choose, const std::unordered_set<int>& waves,
    const std::unordered_set<pvz_emulator::object::zombie_type>& target_zombies,
    int max_card_zombie_row_diff)
{
    std::array<int, 6> target_zombie_count = {};
    for (const auto& z : w.scene.zombies) {
        if (!z.is_dead && z.is_not_dying && target_zombies.count(z.type)
            && (waves.empty() || waves.count(z.spawn_wave))) {
            target_zombie_count[z.row]++;
        }
    }

    std::unordered_set<size_t> indices;
    indices.reserve(card_positions.size());
    for (size_t i = 0; i < card_positions.size(); i++) {
        indices.insert(i);
    }

    std::vector<size_t> res;
    res.reserve(choose);
    for (int i = 0; i < choose; i++) {
        int max_target_zombie_sum = -1;
        std::optional<size_t> best_index;

        for (const auto& index : indices) {
            auto card_row = card_positions[index].row - 1;
            int target_zombie_sum = 0;
            for (int zombie_row = card_row - max_card_zombie_row_diff;
                zombie_row <= card_row + max_card_zombie_row_diff; zombie_row++) {
                if (zombie_row >= 0 && zombie_row < 6) {
                    target_zombie_sum += target_zombie_count[zombie_row];
                }
            }

            if (target_zombie_sum > max_target_zombie_sum) {
                max_target_zombie_sum = target_zombie_sum;
                best_index = index;
            }
        }
        if (!best_index.has_value()) {
            break;
        } else {
            res.push_back(*best_index);
            indices.erase(*best_index);
        }
    }
    return res;
}

int get_fixed_card_op_tick(const FixedCard* fixed_card, int tick)
{
    auto plant_type = fixed_card->plant_type;

    if (plant_type == pvz_emulator::object::plant_type::jalapeno
        || plant_type == pvz_emulator::object::plant_type::cherry_bomb) {
        return tick - 99;
    } else if (plant_type == pvz_emulator::object::plant_type::squash) {
        return tick - 181;
    } else if (plant_type == pvz_emulator::object::plant_type::garlic) {
        return tick;
    } else {
        assert(false && "unreachable");
        return 0;
    }
}

int get_smart_card_op_tick(const SmartCard* smart_card, int tick)
{
    auto plant_type = smart_card->plant_type;

    if (plant_type == pvz_emulator::object::plant_type::jalapeno
        || plant_type == pvz_emulator::object::plant_type::cherry_bomb) {
        return tick - 99;
    } else if (plant_type == pvz_emulator::object::plant_type::squash) {
        return tick - 181;
    } else {
        assert(false && "unreachable");
        return 0;
    }
}

int get_smart_card_max_card_zombie_row_diff(const SmartCard* smart_card)
{
    auto plant_type = smart_card->plant_type;

    if (plant_type == pvz_emulator::object::plant_type::cherry_bomb) {
        return 1;
    } else if (plant_type == pvz_emulator::object::plant_type::jalapeno
        || plant_type == pvz_emulator::object::plant_type::squash) {
        return 0;
    } else {
        assert(false && "unreachable");
        return 0;
    }
}

int get_cob_fly_time(
    const pvz_emulator::object::scene_type& scene_type, int row, double col, int cob_col = -1)
{
    if (is_backyard(scene_type) && (row == 3 || row == 4)) {
        return 378;
    } else if (is_roof(scene_type)) {
        assert(cob_col != -1);
        return get_roof_cob_fly_time(col, cob_col);
    } else {
        return 373;
    }
}
