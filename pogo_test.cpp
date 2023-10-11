/* 测试收跳跳的最左炮准星.
 * 注意: PvZ Emulator 模拟跳跳与玉米炮互动不准确, 无法测定有炮时的情况.
 *
 * WINDOWS POWERSHELL
 * g++ -O3 -o dest/bin/pogo_test pogo_test.cpp -Ilib -Ilib/lib -Llib/build -lpvzemu -Wfatal-errors;
 * cd dest; ./bin/pogo_test; cd ..
 */

#include "common/io.h"
#include "common/pe.h"
#include "constants/constants.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

const std::string OUTPUT_FILE = "pogo_test";

const int TOTAL_WAVE_NUM = 10240;
const int THREAD_NUM = 32;            // if not sure, use number of CPU cores
const int ZOMBIE_NUM_PER_WAVE = 1000; // must not exceed 1024
const int START_TICK = 1200;
const int END_TICK = 1800;
const scene_type SCENE_TYPE = scene_type::fog;
const int ICE_TIME = 1;                   // actual effect time, <= 0 means no ice
const std::vector<int> COB_COLS = {5, 7}; // col of cob tail, [0..8]

std::mutex mtx;

int upper_cob[END_TICK - START_TICK + 1];
int same_cob[END_TICK - START_TICK + 1];
int lower_cob[END_TICK - START_TICK + 1];

void test(int repeat)
{
    world w(SCENE_TYPE);

    int local_upper_cob[END_TICK - START_TICK + 1];
    int local_same_cob[END_TICK - START_TICK + 1];
    int local_lower_cob[END_TICK - START_TICK + 1];
    for (int i = 0; i < (END_TICK - START_TICK + 1); i++) {
        local_upper_cob[i] = local_same_cob[i] = local_lower_cob[i] = 999.0f;
    }

    for (int r = 0; r < repeat; r++) {
        w.reset();
        w.scene.stop_spawn = true;

        if (ICE_TIME > 0) {
            w.plant_factory.create(plant_type::iceshroom, 0, 0);
            run(w, 100 - ICE_TIME);
        }

        for (const auto& cob_col : COB_COLS) {
            for (int cob_row = 1; cob_row <= w.scene.get_max_row(); cob_row++) {
                w.plant_factory.create(plant_type::cob_cannon, cob_row - 1, cob_col - 1);
            }
        }

        for (int i = 0; i < ZOMBIE_NUM_PER_WAVE; i++) {
            w.zombie_factory.create(zombie_type::pogo);
        }
        run(w, START_TICK);

        for (int tick = START_TICK; tick <= END_TICK; tick++) {
            for (auto& z : w.scene.zombies) {
                assert(z.type == zombie_type::pogo);

                auto upper_cob_x_range
                    = get_cob_hit_x_range(get_hit_box(z), row_to_cob_hit_y(w.scene.type, z.row));
                auto same_cob_x_range = get_cob_hit_x_range(
                    get_hit_box(z), row_to_cob_hit_y(w.scene.type, z.row + 1));
                auto lower_cob_x_range = get_cob_hit_x_range(
                    get_hit_box(z), row_to_cob_hit_y(w.scene.type, z.row + 2));

                local_upper_cob[tick - START_TICK]
                    = std::min(local_upper_cob[tick - START_TICK], upper_cob_x_range.second);
                local_same_cob[tick - START_TICK]
                    = std::min(local_same_cob[tick - START_TICK], same_cob_x_range.second);
                local_lower_cob[tick - START_TICK]
                    = std::min(local_lower_cob[tick - START_TICK], lower_cob_x_range.second);
            }
            run(w, 1);
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        upper_cob[tick - START_TICK]
            = std::min(upper_cob[tick - START_TICK], local_upper_cob[tick - START_TICK]);
        same_cob[tick - START_TICK]
            = std::min(same_cob[tick - START_TICK], local_same_cob[tick - START_TICK]);
        lower_cob[tick - START_TICK]
            = std::min(lower_cob[tick - START_TICK], local_lower_cob[tick - START_TICK]);
    }
}

int min_garg_walk(int tick)
{
    if (ICE_TIME <= 0) {
        return tick;
    } else {
        int res = ICE_TIME - 1;
        int walk = std::max(tick - ICE_TIME - (600 - 2), 0);
        int uniced_walk = std::max(walk - (2000 - 600), 0);
        return static_cast<int>(res + (walk - uniced_walk) / 2.0 + uniced_walk);
    }
}

std::string bool_to_string(bool b) { return b ? "OK" : "ERROR"; }

int main()
{
    auto start = std::chrono::high_resolution_clock::now();
    ::system("chcp 65001 > nul");
    auto file = open_csv(OUTPUT_FILE).first;

    for (int i = 0; i < (END_TICK - START_TICK + 1); ++i) {
        upper_cob[i] = same_cob[i] = lower_cob[i] = 999.0f;
    }

    std::vector<std::thread> threads;
    for (const auto& repeat : assign_repeat(TOTAL_WAVE_NUM, THREAD_NUM)) {
        threads.emplace_back([repeat]() { test(repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "tick,收上行跳跳,收本行跳跳,收下行跳跳,收上行巨人,收本行巨人,收下行巨人,"
         << "全三,三跳两巨,两跳三巨,全两,下跳两巨,"
         << "\n";

    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        auto upper_cob_pogo = upper_cob[tick - START_TICK];
        auto same_cob_pogo = same_cob[tick - START_TICK];
        auto lower_cob_pogo = lower_cob[tick - START_TICK];

        rect garg_hit_box;
        garg_hit_box.x = static_cast<int>(get_garg_x_max(min_garg_walk(tick))) - 17;
        garg_hit_box.y = (is_roof(SCENE_TYPE) ? 40 : 50) - 38;
        garg_hit_box.width = 125;
        garg_hit_box.height = 154;

        auto upper_cob_garg
            = get_cob_hit_x_range(garg_hit_box, row_to_cob_hit_y(SCENE_TYPE, 0)).first;
        auto same_cob_garg
            = get_cob_hit_x_range(garg_hit_box, row_to_cob_hit_y(SCENE_TYPE, 1)).first;
        auto lower_cob_garg
            = get_cob_hit_x_range(garg_hit_box, row_to_cob_hit_y(SCENE_TYPE, 2)).first;

        file << tick << "," << lower_cob_pogo << "," << same_cob_pogo << "," << upper_cob_pogo
             << "," << lower_cob_garg << "," << same_cob_garg << "," << upper_cob_garg << ","
             << bool_to_string(lower_cob_garg <= lower_cob_pogo) << ","
             << bool_to_string(
                    (upper_cob_garg <= lower_cob_pogo) && (same_cob_garg <= lower_cob_pogo))
             << "," << bool_to_string(lower_cob_garg <= same_cob_pogo)
             << "," // assume lower pogo is always easier than same pogo
             << bool_to_string(
                    (upper_cob_garg <= same_cob_pogo) && (same_cob_garg <= same_cob_pogo))
             << ","
             << bool_to_string(
                    (upper_cob_garg <= upper_cob_pogo) && (same_cob_garg <= upper_cob_pogo))
             << ","
             << "\n";
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";
    return 0;
}