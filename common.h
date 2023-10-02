
#include "world.h"

#include <chrono>
#include <iomanip>
#include <iostream>

bool is_roof(const pvz_emulator::object::scene_type& scene_type)
{
    using namespace pvz_emulator::object;
    return scene_type == scene_type::roof || scene_type == scene_type::moon_night;
}

bool is_frontyard(const pvz_emulator::object::scene_type& scene_type)
{
    using namespace pvz_emulator::object;
    return scene_type == scene_type::day || scene_type == scene_type::night;
}

void dump(pvz_emulator::world& w)
{
    std::string str;
    w.to_json(str);
    std::cout << str << std::endl;
}

void dump(pvz_emulator::object::scene& scene, pvz_emulator::object::zombie& zombie)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    zombie.to_json(scene, writer);
    std::cout << sb.GetString() << std::endl;
}

void run(pvz_emulator::world& w, int ticks)
{
    for (int i = 0; i < ticks; i++)
        w.update();
}

// TODO: implement for roof and moon_night
// row: [1, 6]
float row_to_cob_hit_y(const pvz_emulator::object::scene_type& scene_type, int row)
{
    using namespace pvz_emulator::object;
    assert(!is_roof(scene_type) && "unimplemented");

    return 120 + (row - 1) * (is_frontyard(scene_type) ? 100 : 85);
}

// row: [1, 6]
// col: [0.0, 10.0]
void launch_cob(pvz_emulator::world& w, int row, float col)
{
    using namespace pvz_emulator::object;
    assert(row >= 1 && row <= w.scene.get_max_row());
    assert(col >= 0.0 && col <= 10.0);

    auto& p = w.plant_factory.create(plant_type::cob_cannon, 1, 1);

    p.status = plant_status::cob_cannon_launch;
    p.countdown.launch = 206;
    p.set_reanim(plant_reanim_name::anim_shooting, reanim_type::once, 12);

    p.cannon.x = static_cast<int>(col * 80.0 - 47.0);
    p.cannon.y = row_to_cob_hit_y(w.scene.type, row);
}

int distance_from_point_to_range(const std::pair<int, int>& range, int point)
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

pvz_emulator::object::rect get_hit_box(const pvz_emulator::object::zombie& zombie)
{
    pvz_emulator::object::rect rect;
    zombie.get_hit_box(rect);
    return rect;
}

// returns a range (min_x, max_x)
// min_x : min cob explode x when cob is to the left of zombie
// max_x : max cob explode x when cob is to the right of zombie
std::pair<int, int> get_cob_hit_x_range(const pvz_emulator::object::rect& zombie_hit_box, int cob_y)
{
    int y_dist = distance_from_point_to_range({zombie_hit_box.y, zombie_hit_box.y + zombie_hit_box.height}, cob_y);
    cob_y > zombie_hit_box.x;

    if (y_dist > 115) {
        return {-1, 999};
    } else {
        int cob_dist = static_cast<int>(sqrt(115 * 115 - y_dist * y_dist));
        return {zombie_hit_box.x - cob_dist + 7, zombie_hit_box.x + zombie_hit_box.width + cob_dist + 7};
    }
}