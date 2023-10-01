/*
g++ -O3 -o test test.cpp -I../lib -I../lib/lib -L../lib/build -lpvzemu -Wfatal-errors;
./test.exe;
rm ./test.exe
*/ 

#include "simulate_summon.h"
#include <iostream>

using namespace pvz_emulator::object;

int main()
{
    init_rnd(0);

    scene s(scene_type::day);
    for (int i = 0; i < 10; i++) {
        auto type_list = random_type_list(s, {GARGANTUAR}, {GIGA_GARGANTUAR});

        assert(std::find(type_list.begin(), type_list.end(), GARGANTUAR) != type_list.end());
        assert(std::find(type_list.begin(), type_list.end(), GIGA_GARGANTUAR) == type_list.end());
    }

    std::cout << "OK -- All tests passed.\n";
}