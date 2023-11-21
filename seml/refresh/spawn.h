#pragma once

#include <optional>

#include "types.h"

using namespace pvz_emulator::object;

const std::unordered_map<zombie_type, std::array<int, 2>> WEIGHT = {
    {zombie_type::zombie, {400, 400}},
    {zombie_type::conehead, {1000, 1000}},
    {zombie_type::pole_vaulting, {2000, 2000}},
    {zombie_type::buckethead, {3000, 3000}},
    {zombie_type::newspaper, {1000, 1000}},
    {zombie_type::screendoor, {3500, 3500}},
    {zombie_type::football, {2000, 2000}},
    {zombie_type::dancing, {1000, 1000}},
    {zombie_type::snorkel, {2000, 2000}},
    {zombie_type::zomboni, {2000, 2000}},
    {zombie_type::dolphin_rider, {1500, 1500}},
    {zombie_type::jack_in_the_box, {1000, 1000}},
    {zombie_type::balloon, {2000, 2000}},
    {zombie_type::digger, {1000, 1000}},
    {zombie_type::pogo, {1000, 1000}},
    {zombie_type::yeti, {1, 1}},
    {zombie_type::bungee, {0, 1000}},
    {zombie_type::ladder, {1000, 1000}},
    {zombie_type::catapult, {1500, 1500}},
    {zombie_type::gargantuar, {1500, 1500}},
    {zombie_type::giga_gargantuar, {1000, 6000}},
};

const std::unordered_map<scene_type, std::vector<zombie_type>> BANNED_TYPES = {
    {scene_type::day, {zombie_type::snorkel, zombie_type::dolphin_rider}},
    {scene_type::night, {zombie_type::snorkel, zombie_type::zomboni, zombie_type::dolphin_rider}},
    {scene_type::pool, {}}, {scene_type::fog, {}},
    {scene_type::roof,
        {zombie_type::dancing, zombie_type::snorkel, zombie_type::dolphin_rider,
            zombie_type::digger}},
    {scene_type::moon_night,
        {zombie_type::dancing, zombie_type::snorkel, zombie_type::dolphin_rider,
            zombie_type::digger}}};

ZombieTypes get_spawn_types(std::mt19937& rng, const pvz_emulator::object::scene_type& scene_type,
    const std::vector<zombie_type>& required = {}, const std::vector<zombie_type>& banned = {})
{
    zombie_type selected, other;
    if (rng() % 5) {
        selected = zombie_type::conehead;
        other = zombie_type::newspaper;
    } else {
        selected = zombie_type::newspaper;
        other = zombie_type::conehead;
    }

    ZombieTypes dummies = {zombie_type::none, zombie_type::flag};
    auto get_candidates = [&]() {
        ZombieTypes candidates = {zombie_type::pole_vaulting, zombie_type::buckethead,
            zombie_type::screendoor, zombie_type::football, zombie_type::dancing,
            zombie_type::snorkel, zombie_type::zomboni, zombie_type::dolphin_rider,
            zombie_type::jack_in_the_box, zombie_type::balloon, zombie_type::digger,
            zombie_type::pogo, zombie_type::pogo, zombie_type::ladder, zombie_type::catapult,
            zombie_type::gargantuar, zombie_type::giga_gargantuar, other};
        for (const auto& type : dummies) {
            candidates.insert(type);
        }
        for (const auto& type : BANNED_TYPES.at(scene_type)) {
            candidates.erase(type);
        }
        for (const auto& types : {required, banned}) {
            for (const auto& type : types) {
                assert(candidates.count(type));
                candidates.erase(type);
            }
        }
        return std::vector<zombie_type>(candidates.begin(), candidates.end());
    };

    auto candidates = get_candidates();
    ZombieTypes res = {zombie_type::zombie, zombie_type::yeti, selected};
    for (const auto& type : required) {
        res.insert(type);
    }
    for (size_t i = 0; i < 9 - required.size(); i++) {
        int idx = rng() % static_cast<int>(candidates.size());
        if (!dummies.count(candidates[idx])) {
            res.insert(candidates[idx]);
        }
        candidates[idx] = candidates[candidates.size() - 1];
        candidates.pop_back();
    }
    return res;
}

std::array<zombie_type, 50> get_spawn_list(std::mt19937& rng, ZombieTypes spawn_types,
    bool huge, int giga_limit = 50, std::optional<int> giga_count = std::nullopt)
{
    if (giga_count.has_value()) {
        spawn_types.erase(zombie_type::giga_gargantuar);
    }

    std::vector<int> weights;
    for (const auto& type : spawn_types) {
        weights.push_back(WEIGHT.at(type).at(huge));
    }
    std::discrete_distribution<> dist(weights.begin(), weights.end());

    std::array<zombie_type, 50> result;
    int cur = 0;
    if (huge) {
        result[cur++] = zombie_type::flag;
        for (int i = 0; i < 8; i++)
            result[cur++] = zombie_type::zombie;
    }
    if (giga_count.has_value()) {
        for (int i = 0; i < *giga_count; i++) {
            result[cur++] = zombie_type::giga_gargantuar;
        }
    }

    std::vector<zombie_type> spawn_types_vec(spawn_types.begin(), spawn_types.end());
    while (cur < 50) {
        auto type = spawn_types_vec[dist(rng)];
        while (!huge && giga_limit <= 0 && type == zombie_type::giga_gargantuar) {
            type = spawn_types_vec[dist(rng)];
        }
        result[cur++] = type;
        if (type == zombie_type::giga_gargantuar) {
            giga_limit--;
        }
    }
    return result;
}
