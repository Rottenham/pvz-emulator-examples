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

// TODO: implement for roof and moon_night
// row: [1, 6]
[[nodiscard]] float row_to_cob_hit_y(const pvz_emulator::object::scene_type& scene_type, int row)
{
    using namespace pvz_emulator::object;
    assert(!is_roof(scene_type) && "unimplemented");

    return 120 + (row - 1) * (is_frontyard(scene_type) ? 100 : 85);
}

// row: [1, 6]
// col: [0.0, 10.0]
int launch_cob(pvz_emulator::world& w, unsigned int row, float col)
{
    using namespace pvz_emulator::object;
    assert(row >= 1 && row <= w.scene.get_max_row());
    assert(col >= 0.0 && col <= 10.0);

    auto& p = w.plant_factory.create(plant_type::cob_cannon, 1, 1);

    p.status = plant_status::cob_cannon_launch;
    p.countdown.launch = 206;
    p.set_reanim(plant_reanim_name::anim_shooting, reanim_type::once, 12);

    p.cannon.x = static_cast<int>(std::round(col * 80.0)) - 47;
    p.cannon.y = row_to_cob_hit_y(w.scene.type, row);

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
        return {-1, 999};
    } else {
        int cob_dist = static_cast<int>(sqrt(115 * 115 - y_dist * y_dist));
        return {zombie_hit_box.x - cob_dist + 7,
            zombie_hit_box.x + zombie_hit_box.width + cob_dist + 7};
    }
}
