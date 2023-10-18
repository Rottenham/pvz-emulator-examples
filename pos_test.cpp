/* 测试各个时刻各类僵尸的最小/最大x坐标.
 */

#include "common/io.h"
#include "common/pe.h"
#include "world.h"

#include <mutex>

/***** 配置部分开始 *****/

const int TOTAL_WAVE_NUM = 10000;
const int START_TICK = 0;
const int END_TICK = 3000;
const std::vector<zombie_type> ZOMBIE_TYPES = {zombie_type::gargantuar};
const bool OUTPUT_AS_INT
    = true; // if true, output float * 32768, which is guaranteed to be an integer when >= 256

/***** 配置部分结束 *****/

using namespace pvz_emulator;
using namespace pvz_emulator::object;

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

        for (int i = 0; i < 1000; i++) {
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
        min_x[idx][tick - START_TICK]
            = std::min(min_x[idx][tick - START_TICK], local_min_x[tick - START_TICK]);
        max_x[idx][tick - START_TICK]
            = std::max(max_x[idx][tick - START_TICK], local_max_x[tick - START_TICK]);
    }
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();
    ::system("chcp 65001 > nul");
    auto file = open_csv("pos_test").first;

    for (int i = 0; i < 33; ++i) {
        for (int j = 0; j < (END_TICK - START_TICK + 1); ++j) {
            min_x[i][j] = 999.0f;
            max_x[i][j] = 0.0f;
        }
    }

    std::vector<std::thread> threads;
    for (const auto& repeat : assign_repeat(TOTAL_WAVE_NUM, std::thread::hardware_concurrency())) {
        threads.emplace_back([repeat]() {
            world w(scene_type::fog);
            for (const auto& zombie_type : ZOMBIE_TYPES) {
                test_one(zombie_type, w, repeat);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "tick,";
    for (const auto& zombie_type : ZOMBIE_TYPES) {
        file << zombie::type_to_string(zombie_type) << "_min,"
             << zombie::type_to_string(zombie_type) << "_max,";
    }
    file << "\n";

    if (!OUTPUT_AS_INT)
        file << std::fixed << std::setprecision(3);

    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        file << tick << ",";
        for (const auto& zombie_type : ZOMBIE_TYPES) {
            int idx = static_cast<int>(zombie_type);

            if (OUTPUT_AS_INT) {
                file << static_cast<int>(32768.0 * min_x[idx][tick - START_TICK]) << ","
                     << static_cast<int>(32768.0 * max_x[idx][tick - START_TICK]) << ",";
            } else {
                file << min_x[idx][tick - START_TICK] << "," << max_x[idx][tick - START_TICK]
                     << ",";
            }
        }
        file << "\n";
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";

    return 0;
}