
#include "world.h"

#include <chrono>
#include <iomanip>
#include <iostream>

void dump(pvz_emulator::world& w)
{
    std::string str;
    w.to_json(str);
    std::cout << str << std::endl;
}

void run(pvz_emulator::world& w, int ticks)
{
    for (int i = 0; i < ticks; i++)
        w.update();
}

// TODO: implement for roof and moon_night
void launch_cob(pvz_emulator::world& w, int row, float col)
{
    using namespace pvz_emulator::object;
    assert(w.scene.type != scene_type::roof && w.scene.type != scene_type::moon_night);

    auto& p = w.plant_factory.create(plant_type::cob_cannon, 1, 1);

    p.status = plant_status::cob_cannon_launch;
    p.countdown.launch = 206;
    p.set_reanim(plant_reanim_name::anim_shooting, reanim_type::once, 12);

    p.cannon.x = static_cast<int>(col * 80.0 - 47.0);
    p.cannon.y = 120 + (row - 1) * (w.scene.type == scene_type::day || w.scene.type == scene_type::night ? 100 : 85);
}
