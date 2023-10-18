/* 测试收跳跳的最左炮准星.
 */

#include "common/io.h"
#include "common/pe.h"
#include "constants/constants.h"
#include "world.h"

#include <mutex>

/***** 配置部分开始 *****/

const int TOTAL_WAVE_NUM = 1000;
const int START_TICK = 1200;
const int END_TICK = 1800;
const scene_type SCENE_TYPE = scene_type::fog;
const int ICE_TIME = 1;                        // actual effect time, <= 0 means no ice
const std::vector<int> IDLE_COB_COLS = {6, 8}; // [2..9]
const int HIT_COB_COL = -1;                    // [1..8], only effective in RE/ME

/***** 配置部分结束 *****/

using namespace pvz_emulator;
using namespace pvz_emulator::object;
using namespace pvz_emulator::system;

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
        w.scene.reset();
        w.scene.stop_spawn = true;

        if (ICE_TIME > 0) {
            w.plant_factory.create(plant_type::iceshroom, 0, 0);
            run(w, 100 - ICE_TIME);
        }

        for (const auto& cob_col : IDLE_COB_COLS) {
            for (int cob_row = 1; cob_row <= w.scene.get_max_row(); cob_row++) {
                w.plant_factory.create(plant_type::cob_cannon, cob_row - 1, cob_col - 2);
            }
        }

        for (int i = 0; i < 1000; i++) {
            w.zombie_factory.create(zombie_type::pogo, 1);
        }
        run(w, START_TICK);

        for (int tick = START_TICK; tick <= END_TICK; tick++) {
            for (auto& z : w.scene.zombies) {
                assert(z.type == zombie_type::pogo);

                auto upper_cob_x_range = get_cob_hit_x_range(get_hit_box(z),
                    get_cob_hit_xy(w.scene.type, z.row, 9.0f, HIT_COB_COL, -1).second);
                auto same_cob_x_range = get_cob_hit_x_range(get_hit_box(z),
                    get_cob_hit_xy(w.scene.type, z.row + 1, 9.0f, HIT_COB_COL, -1).second);
                auto lower_cob_x_range = get_cob_hit_x_range(get_hit_box(z),
                    get_cob_hit_xy(w.scene.type, z.row + 2, 9.0f, HIT_COB_COL, -1).second);

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
    auto file = open_csv("pogo_test").first;

    for (int i = 0; i < (END_TICK - START_TICK + 1); ++i) {
        upper_cob[i] = same_cob[i] = lower_cob[i] = 999.0f;
    }

    std::vector<std::thread> threads;
    for (const auto& repeat : assign_repeat(TOTAL_WAVE_NUM, std::thread::hardware_concurrency())) {
        threads.emplace_back([repeat]() { test(repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "tick,收上行跳跳,收本行跳跳,收下行跳跳,收上行巨人,收本行巨人,收下行巨人,"
         << "上跳上巨,上跳本巨,上跳下巨,本跳上巨,本跳本巨,本跳下巨,下跳上巨,下跳本巨,下跳下巨"
         << "\n";

    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        auto upper_cob_pogo = upper_cob[tick - START_TICK];
        auto same_cob_pogo = same_cob[tick - START_TICK];
        auto lower_cob_pogo = lower_cob[tick - START_TICK];

        rect garg_hit_box;
        garg_hit_box.x = static_cast<int>(get_garg_x_max(min_garg_walk(tick))) - 17;
        garg_hit_box.y
            = (is_roof(SCENE_TYPE) ? 40 : 50) + (is_frontyard(SCENE_TYPE) ? 100 : 85) - 38;
        garg_hit_box.width = 125;
        garg_hit_box.height = 154;

        auto upper_cob_garg = get_cob_hit_x_range(
            garg_hit_box, get_cob_hit_xy(SCENE_TYPE, 1, 9.0f, HIT_COB_COL, -1).second)
                                  .first;
        auto same_cob_garg = get_cob_hit_x_range(
            garg_hit_box, get_cob_hit_xy(SCENE_TYPE, 2, 9.0f, HIT_COB_COL, -1).second)
                                 .first;
        auto lower_cob_garg = get_cob_hit_x_range(
            garg_hit_box, get_cob_hit_xy(SCENE_TYPE, 3, 9.0f, HIT_COB_COL, -1).second)
                                  .first;

        file << tick << "," << lower_cob_pogo << "," << same_cob_pogo << "," << upper_cob_pogo
             << "," << lower_cob_garg << "," << same_cob_garg << "," << upper_cob_garg << ",";

        file << bool_to_string(lower_cob_garg <= lower_cob_pogo) << ","; // 上跳上巨
        file << bool_to_string(same_cob_garg <= lower_cob_pogo) << ",";  // 上跳本巨
        file << bool_to_string(upper_cob_garg <= lower_cob_pogo) << ","; // 上跳下巨

        file << bool_to_string(lower_cob_garg <= same_cob_pogo) << ","; // 本跳上巨
        file << bool_to_string(same_cob_garg <= same_cob_pogo) << ",";  // 本跳本巨
        file << bool_to_string(upper_cob_garg <= same_cob_pogo) << ","; // 本跳下巨

        file << bool_to_string(lower_cob_garg <= upper_cob_pogo) << ","; // 下跳上巨
        file << bool_to_string(same_cob_garg <= upper_cob_pogo) << ",";  // 下跳本巨
        file << bool_to_string(upper_cob_garg <= upper_cob_pogo) << ","; // 下跳下巨

        file << "\n";
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";
    return 0;
}