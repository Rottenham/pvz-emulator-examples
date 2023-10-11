/* 测试意外刷新概率.
 *
 * WINDOWS POWERSHELL
 * g++ -O3 -o dest/bin/refresh_test refresh_test.cpp -Ilib -Ilib/lib -Llib/build -lpvzemu
 * -Wfatal-errors; cd dest; ./bin/refresh_test; cd ..
 */

#include "common/io.h"
#include "common/pe.h"
#include "refresh/simulate_summon.h"
#include "world.h"

using namespace pvz_emulator;
using namespace pvz_emulator::object;

const std::string OUTPUT_FILE = "refresh_test";
const int COB_TIME = 225;
const int TOTAL_ROUND_NUM = 1000;
const int WAVE_PER_ROUND = 20;
const bool ASSUME_ACTIVATE = true;

double hp_ratio[TOTAL_ROUND_NUM + 1][WAVE_PER_ROUND + 1];
double accident_rate[TOTAL_ROUND_NUM + 1][WAVE_PER_ROUND + 1];

void test_one_round(int round, world& w)
{
    auto type_list = random_type_list(w.scene, {GARGANTUAR, GIGA_GARGANTUAR});

    for (int wave = 1; wave <= WAVE_PER_ROUND; wave++) {
        auto spawn_list = simulate_wave(type_list);

        w.reset();
        w.scene.stop_spawn = true;

        launch_cob(w, 2, 9);
        launch_cob(w, 5, 9);

        run(w, 373 - COB_TIME);

        w.scene.spawn.wave = 1;
        for (const auto& z : spawn_list)
            w.zombie_factory.create(static_cast<zombie_type>(z));

        w.scene.spawn.wave
            = 2; // as if we are already at the next wave, so get_current_hp() works correctly
        auto init_hp = w.spawn.get_current_hp();
        run(w, 401);
        auto curr_hp = w.spawn.get_current_hp();

        hp_ratio[round][wave] = 1.0 * curr_hp / init_hp;
        double refresh_prob = (0.65 - std::clamp(hp_ratio[round][wave], 0.5, 0.65)) / 0.15;
        accident_rate[round][wave] = ASSUME_ACTIVATE ? 1 - refresh_prob : refresh_prob;
    }
}

int main(void)
{
    auto start = std::chrono::high_resolution_clock::now();
    ::system("chcp 65001 > nul");
    auto file = open_csv(OUTPUT_FILE).first;
    init_rnd();

    std::atomic<int> i = 0;
    std::vector<std::thread> threads;
    for (size_t j = 0; j < std::thread::hardware_concurrency(); j++) {
        threads.emplace_back([&]() {
            world w(scene_type::fog);
            for (auto round = i.fetch_add(1); round <= TOTAL_ROUND_NUM; round = i.fetch_add(1)) {
                test_one_round(round, w);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "index,wave,hp,accident_rate\n";
    double hp_ratio_sum = 0.0, accident_rate_sum = 0.0;
    for (int round = 1; round <= TOTAL_ROUND_NUM; round++) {
        for (int wave = 1; wave <= WAVE_PER_ROUND; wave++) {
            hp_ratio_sum += hp_ratio[round][wave];
            accident_rate_sum += accident_rate[round][wave];
        }
    }
    file << ",," << std::fixed << std::setprecision(3)
         << hp_ratio_sum / (TOTAL_ROUND_NUM * WAVE_PER_ROUND) << ","
         << 100.0 * accident_rate_sum / (TOTAL_ROUND_NUM * WAVE_PER_ROUND) << "%,\n";

    for (int round = 1; round <= TOTAL_ROUND_NUM; round++) {
        for (int wave = 1; wave <= WAVE_PER_ROUND; wave++) {
            file << round << "," << wave << "," << hp_ratio[round][wave] << ","
                 << accident_rate[round][wave] << ",\n";
        }
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";
    return 0;
}