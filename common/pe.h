#pragma once

#include "world.h"

[[nodiscard]] bool is_roof(const pvz_emulator::object::scene_type& scene_type)
{
    using namespace pvz_emulator::object;
    return scene_type == scene_type::roof || scene_type == scene_type::moon_night;
}

[[nodiscard]] bool is_frontyard(const pvz_emulator::object::scene_type& scene_type)
{
    using namespace pvz_emulator::object;
    return scene_type == scene_type::day || scene_type == scene_type::night;
}

[[nodiscard]] bool is_backyard(const pvz_emulator::object::scene_type& scene_type)
{
    using namespace pvz_emulator::object;
    return scene_type == scene_type::pool || scene_type == scene_type::fog;
}

[[nodiscard]] std::string dump(pvz_emulator::world& w)
{
    std::string str;
    w.to_json(str);
    return str + "\n";
}

[[nodiscard]] std::string dump(
    pvz_emulator::object::scene& scene, pvz_emulator::object::zombie& zombie)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    zombie.to_json(scene, writer);
    return std::string(sb.GetString()) + "\n";
}

[[nodiscard]] std::string dump(
    pvz_emulator::object::scene& scene, pvz_emulator::object::plant& plant)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    plant.to_json(scene, writer);
    return std::string(sb.GetString()) + "\n";
}

void run(pvz_emulator::world& w, int ticks)
{
    assert(ticks >= 0);
    for (int i = 0; i < ticks; i++)
        w.update();
}

void run(pvz_emulator::world& w, int& curr_tick, int target_tick)
{
    run(w, target_tick - curr_tick);
    curr_tick = target_tick;
}

// row: [1, 6]
[[nodiscard]] std::pair<int, int> get_cob_hit_xy(const pvz_emulator::object::scene_type& scene_type,
    int row, double col, int cob_col, int cob_row = -1)
{
    using namespace pvz_emulator::object;
    if (!is_roof(scene_type)) {
        int x = static_cast<int>(std::round(col * 80.0));
        if (x >= 7) {
            x -= 7;
        } else {
            x -= 6;
        }
        return {x - 40, 120 + (row - 1) * (is_frontyard(scene_type) ? 100 : 85)};
    } else {
        int x = static_cast<int>(std::round(col * 80.0));
        int y = 209 + (row - 1) * 85;

        int step1;
        if (x <= 206) {
            step1 = 0;
        } else if (x >= 527) {
            step1 = 5;
        } else {
            step1 = (x - 127) / 80;
        }
        y -= step1 * 20;

        int left_edge, right_edge, step2_shift;
        assert((cob_col != -1) && "must provide cob col");
        if (cob_col == 1) {
            left_edge = 87;
            right_edge = 524;
            step2_shift = 0;
        } else if (cob_col >= 7) {
            left_edge = 510;
            right_edge = 523;
            step2_shift = 5;
        } else {
            left_edge = 80 * cob_col - 13;
            right_edge = 524;
            step2_shift = 5;
        }

        int step2;
        if (x <= left_edge) {
            step2 = 0;
        } else if (x >= right_edge) {
            step2 = (right_edge - left_edge + 3) / 4 - step2_shift;
        } else {
            step2 = (x - left_edge + 3) / 4 - step2_shift;
        }
        y -= step2;

        if (x == left_edge && cob_col >= 2 && cob_col <= 6) {
            assert((cob_row != -1) && "must provide cob row");
            if (cob_row >= 3 && cob_row <= 5) {
                y += 5;
            }
            if (cob_row == 3 && cob_col == 6) {
                y -= 5;
            }
        }

        if (x >= 7) {
            x -= 7;
        } else {
            x -= 6;
        }

        if (y < 0) {
            y = 0;
        }

        return {x - 40, y};
    }
}

// row: [1, 6]
// col: [0.0, 10.0]
// cob_col: [1, 8] (required for RE/ME)
int launch_cob(
    pvz_emulator::world& w, unsigned int row, double col, int cob_col = -1, int cob_row = -1)
{
    using namespace pvz_emulator::object;
    assert(row >= 1 && row <= w.scene.get_max_row());
    assert(col >= 0.0 && col <= 10.0);
    if (is_roof(w.scene.type)) {
        assert((cob_col >= 1 && cob_col <= 8));
    } else {
        assert(cob_col == -1);
        cob_col = 1;
    }

    if (cob_row == -1) {
        cob_row = is_backyard(w.scene.type) ? 3 : 6;
    } else {
        assert(cob_row >= 1 && cob_row <= static_cast<int>(w.scene.get_max_row()));
    }

    auto& p = w.plant_factory.create(plant_type::cob_cannon, cob_row - 1, cob_col - 1);

    p.status = plant_status::cob_cannon_launch;
    p.countdown.launch = 206;
    p.set_reanim(plant_reanim_name::anim_shooting, reanim_type::once, 12);

    p.cannon.x = static_cast<int>(std::round(col * 80.0)) - 47;
    p.cannon.y = 120 + (row - 1) * (is_frontyard(w.scene.type) ? 100 : 85);

    return p.uuid;
}

[[nodiscard]] int distance_from_point_to_range(const std::pair<int, int>& range, int point)
{
    assert(range.first <= range.second);
    if (point < range.first) {
        return range.first - point;
    } else if (point > range.second) {
        return point - range.second;
    } else {
        return 0;
    }
}

[[nodiscard]] pvz_emulator::object::rect get_hit_box(const pvz_emulator::object::zombie& zombie)
{
    pvz_emulator::object::rect rect;
    zombie.get_hit_box(rect);
    return rect;
}

// returns a range (min_x, max_x)
// min_x : min cob hit x when cob is to the left of zombie
// max_x : max cob hit x when cob is to the right of zombie
[[nodiscard]] std::pair<int, int> get_cob_hit_x_range(
    const pvz_emulator::object::rect& zombie_hit_box, int cob_y)
{
    int y_dist = distance_from_point_to_range(
        {zombie_hit_box.y, zombie_hit_box.y + zombie_hit_box.height}, cob_y);

    if (y_dist > 115) {
        return {9999, -9999};
    } else {
        int cob_dist = static_cast<int>(sqrt(115 * 115 - y_dist * y_dist));
        return {zombie_hit_box.x - cob_dist + 7,
            zombie_hit_box.x + zombie_hit_box.width + cob_dist + 7};
    }
}
