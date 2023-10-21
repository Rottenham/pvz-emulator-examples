#pragma once

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

std::vector<int> choose_by_pos(pvz_emulator::world& w, const std::vector<CardPos>& positions,
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

std::vector<int> choose_by_num(pvz_emulator::world& w, const std::vector<CardPos>& positions,
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
