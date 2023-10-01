// g++ -O3 -o test test.cpp -I.. -I../lib -L../build -lpvzemu -Wfatal-errors; ./test.exe > test_output.txt

#include "simulate_summon.h"
#include <iostream>

using namespace pvz_emulator::object;

int main()
{
    init_rnd(0);

    scene s(scene_type::day);
    auto type_list = random_type_list(s, {GARGANTUAR}, {GIGA_GARGANTUAR});
    auto spawn_list = simulate_summon(s, type_list);

    for (int i = 1; i <= 20; i++) {
        for (int j = 0; j < 50; j++)
            std::cout << zombie::type_to_string(static_cast<zombie_type>(spawn_list[(i - 1) * 50 + j])) << " ";
        std::cout << std::endl;
    }
}