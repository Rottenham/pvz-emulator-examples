/* 测试各个时刻各类僵尸的最小/最大x坐标.
 *
 * WINDOWS POWERSHELL
 * g++ -O3 -o dest/bin/pos_test pos_test.cpp -Ilib -Ilib/lib -Llib/build -lpvzemu -Wfatal-errors; cd dest; ./bin/pos_test > pos_test.csv; cd ..
 */

#include "common.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

const int TOTAL_WAVE_NUM = 10240;
const int THREAD_NUM = 32;            // if not sure, use number of CPU cores
const int ZOMBIE_NUM_PER_WAVE = 1000; // must not exceed 1024
const int START_TICK = 0;
const int END_TICK = 3000;
const std::vector<zombie_type> ZOMBIE_TYPES = {zombie_type::gargantuar};
const bool OUTPUT_AS_INT = true; // if true, output float * 32768, which is guaranteed to be an integer when >= 256

std::mutex mtx;

float min_x[33][END_TICK - START_TICK + 1];
float max_x[33][END_TICK - START_TICK + 1];

void test_one(const zombie_type& type, world& w, int wave_num_per_thread)
{
    float local_min_x[END_TICK - START_TICK + 1];
    float local_max_x[END_TICK - START_TICK + 1];
    for (int i = 0; i < (END_TICK - START_TICK + 1); ++i) {
        local_min_x[i] = 999.0f;
        local_max_x[i] = 0.0f;
    }

    for (int r = 0; r < wave_num_per_thread; r++) {
        w.reset();
        w.scene.stop_spawn = true;

        for (int i = 0; i < ZOMBIE_NUM_PER_WAVE; i++) {
            w.zombie_factory.create(type);
        }
        run(w, START_TICK);

        for (int tick = START_TICK; tick <= END_TICK; tick++) {
            for (const auto& z : w.scene.zombies) {
                local_min_x[tick - START_TICK] = std::min(local_min_x[tick - START_TICK], z.x);
                local_max_x[tick - START_TICK] = std::max(local_max_x[tick - START_TICK], z.x);
            }
            run(w, 1);
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        int idx = static_cast<int>(type);
        min_x[idx][tick - START_TICK] = std::min(min_x[idx][tick - START_TICK], local_min_x[tick - START_TICK]);
        max_x[idx][tick - START_TICK] = std::max(max_x[idx][tick - START_TICK], local_max_x[tick - START_TICK]);
    }
}

int main()
{
    static_assert(TOTAL_WAVE_NUM % THREAD_NUM == 0);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 33; ++i) {
        for (int j = 0; j < (END_TICK - START_TICK + 1); ++j) {
            min_x[i][j] = 999.0f;
            max_x[i][j] = 0.0f;
        }
    }

    std::vector<std::thread> threads;
    for (auto j = 0; j < THREAD_NUM; j++) {
        threads.emplace_back([&]() {
            world w(scene_type::fog);
            for (const auto& zombie_type : ZOMBIE_TYPES)
                test_one(zombie_type, w, TOTAL_WAVE_NUM / THREAD_NUM);
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Finished in " << elapsed.count() << "s with " << threads.size() << " threads.\n";

    std::cout << "tick,";
    for (const auto& zombie_type : ZOMBIE_TYPES) {
        std::cout << zombie::type_to_string(zombie_type) << "_min,"
                  << zombie::type_to_string(zombie_type) << "_max,";
    }
    std::cout << "\n";

    if (!OUTPUT_AS_INT)
        std::cout << std::fixed << std::setprecision(3);

    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        std::cout << tick << ",";
        for (const auto& zombie_type : ZOMBIE_TYPES) {
            int idx = static_cast<int>(zombie_type);

            if (OUTPUT_AS_INT) {
                std::cout << static_cast<int>(32768.0 * min_x[idx][tick - START_TICK]) << ","
                          << static_cast<int>(32768.0 * max_x[idx][tick - START_TICK]) << ",";
            } else {
                std::cout << min_x[idx][tick - START_TICK] << ","
                          << max_x[idx][tick - START_TICK] << ",";
            }
        }
        std::cout << "\n";
    }
    return 0;
}