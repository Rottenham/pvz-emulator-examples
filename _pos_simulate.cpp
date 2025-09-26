/* 通过模拟随机数的方式，测试各个时刻各类僵尸的最小/最大x坐标.
   已有对小丑不爆的处理。
 */

#include "common/pe.h"
#include "common/test.h"
#include "world.h"

#include <algorithm>
#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

/***** 配置部分开始 *****/

const int START_TICK = 0;
const int END_TICK = 5000;
const int ICE_TIME = -1;
const zombie_type ZOMBIE_TYPE = zombie_type::jack_in_the_box;
const zombie_dance_cheat dance_cheat = zombie_dance_cheat::none;
const bool OUTPUT_AS_INT
    = true; // if true, output float * 32768, which is guaranteed to be an integer when >= 256
const bool HUGE_WAVE = false;

/***** 配置部分结束 *****/

const std::unordered_map<zombie_type, std::pair<int, int>> ZOMBIE_POS_RANGES = {
    {zombie_type::flag, {800, 800}},
    {zombie_type::pole_vaulting, {870, 879}},
    {zombie_type::zomboni, {800, 809}},
    {zombie_type::catapult, {825, 834}},
    {zombie_type::gargantuar, {845, 854}},
    {zombie_type::giga_gargantuar, {845, 854}},
};

std::pair<int, int> get_pos_range(const zombie_type& type)
{
    auto it = ZOMBIE_POS_RANGES.find(type);
    if (it != ZOMBIE_POS_RANGES.end()) {
        return it->second;
    } else {
        if (HUGE_WAVE) {
            return {780 + 40, 819 + 40};
        } else {
            return {780, 819};
        }
    }
}

int get_enter_home_thres(const zombie_type& type)
{
    int threshold;
    switch (type) {
    case zombie_type::gargantuar:
    case zombie_type::giga_gargantuar:
    case zombie_type::pole_vaulting:
        threshold = -150;
        break;
    case zombie_type::catapult:
    case zombie_type::football:
    case zombie_type::zomboni:
        threshold = -175;
        break;
    case zombie_type::backup_dancer:
    case zombie_type::dancing:
    case zombie_type::snorkel:
        threshold = -130;
        break;
    default:
        threshold = -100;
        break;
    }
    return threshold;
}

std::pair<float, float> get_dx_range(const zombie_type& type)
{
    if (type == zombie_type::digger || type == zombie_type::pole_vaulting
        || type == zombie_type::football || type == zombie_type::snorkel
        || type == zombie_type::jack_in_the_box) {
        return {0.66f, 0.68f};
    }
    if (type == zombie_type::ladder) {
        return {0.79f, 0.81f};
    }
    if (type == zombie_type::dolphin_rider) {
        return {0.89f, 0.91f};
    }
    return {0.23f, 0.37f};
}

struct XAndDx {
    float x;
    float dx;

    bool operator<(const XAndDx& other) const { return x < other.x; }
    bool operator>(const XAndDx& other) const { return x > other.x; }
    bool operator<(const float& other) const { return x < other; }
    bool operator>(const float& other) const { return x > other; }
};

std::mutex mtx;
const float DEFAULT_MIN = 999.0f;
const float DEFAULT_MAX = -999.0f;

XAndDx min_x[33][END_TICK - START_TICK + 1];
XAndDx max_x[33][END_TICK - START_TICK + 1];

std::vector<float> dx_list;

void test_one(const zombie_type& type, world& w, size_t dx_idx_start, size_t dx_idx_end)
{
    if (!(dx_idx_start < dx_idx_end && dx_idx_end <= dx_list.size())) {
        std::cout << "ERROR: " << dx_idx_start << " " << dx_idx_end << " " << dx_list.size();
        assert(false);
    }
    auto pos_range = get_pos_range(type);
    auto enter_home_thres = get_enter_home_thres(type);

    XAndDx local_min_x[END_TICK - START_TICK + 1];
    XAndDx local_max_x[END_TICK - START_TICK + 1];
    for (size_t i = 0; i < END_TICK - START_TICK + 1; i++) {
        local_min_x[i] = {DEFAULT_MIN, 0.0f};
        local_max_x[i] = {DEFAULT_MAX, 0.0f};
    }

    for (size_t i = dx_idx_start; i < dx_idx_end; i++) {
        auto dx = dx_list[i];

        for (int pos : {pos_range.first, pos_range.second}) {
            w.scene.reset();
            w.scene.stop_spawn = true;
            w.scene.ignore_game_over = true;
            w.scene.lock_dx = true;
            w.scene.lock_dx_val = dx;
            w.scene.is_zombie_dance = dance_cheat == zombie_dance_cheat::slow;
            if (ICE_TIME > 0) {
                w.plant_factory.create(plant_type::iceshroom, 0, 0);
                run(w, 100 - ICE_TIME);
            }

            auto& z = w.zombie_factory.create(type);
            z.x = static_cast<float>(pos);
            z.int_x = pos;
            if (z.type == zombie_type::jack_in_the_box) {
                z.countdown.action = END_TICK + 1;
            } else if ((z.type == zombie_type::zombie || z.type == zombie_type::conehead
                           || z.type == zombie_type::buckethead)) {
                z.dance_cheat = dance_cheat;
            }
            run(w, START_TICK);

            for (int tick = START_TICK; tick <= END_TICK; tick++) {
                if (local_min_x[tick - START_TICK] > z.x) {
                    local_min_x[tick - START_TICK] = {z.x, dx};
                }
                if (local_max_x[tick - START_TICK] < z.x) {
                    local_max_x[tick - START_TICK] = {z.x, dx};
                }
                if (static_cast<int>(z.x) < enter_home_thres) {
                    break;
                }
                run(w, 1);
            }
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        int idx = static_cast<int>(type);
        auto min = local_min_x[tick - START_TICK], max = local_max_x[tick - START_TICK];
        if (min < min_x[idx][tick - START_TICK]) {
            min_x[idx][tick - START_TICK] = min;
        }
        if (max > max_x[idx][tick - START_TICK]) {
            max_x[idx][tick - START_TICK] = max;
        }
    }
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();
    ::system("chcp 65001 > nul");
    std::string name = zombie::type_to_string(ZOMBIE_TYPE);
    auto file = open_csv("pos_sim_" + name).first;
    file << std::fixed;

    for (size_t i = 0; i < 33; ++i) {
        for (size_t j = 0; j < END_TICK - START_TICK + 1; j++) {
            min_x[i][j] = {DEFAULT_MIN, 0.0f};
            max_x[i][j] = {DEFAULT_MAX, 0.0f};
        }
    }

    auto dx_range = get_dx_range(ZOMBIE_TYPE);
    for (float dx = dx_range.first; dx <= dx_range.second;
         dx = std::nextafter(dx, dx_range.second + 1.0f)) {
        dx_list.push_back(dx);
    }

    std::vector<std::thread> threads;
    size_t idx = 0;
    for (int workload :
        assign_repeat(static_cast<int>(dx_list.size()), std::thread::hardware_concurrency())) {
        threads.emplace_back([idx, workload]() {
            world w(scene_type::fog);
            test_one(ZOMBIE_TYPE, w, idx, idx + static_cast<size_t>(workload));
        });
        idx += workload;
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "tick,";
    file << zombie::type_to_string(ZOMBIE_TYPE) << "_min,dx," <<
    zombie::type_to_string(ZOMBIE_TYPE)
         << "_max,dx,";
    file << "dx=" << dx_range.first << "~" << dx_range.second << ". " << dx_list.size()
         << " values in total.\n";

    bool stop_min[33] = {};
    bool stop_max[33] = {};
    for (int tick = START_TICK; tick <= END_TICK; tick++) {
        file << tick << ",";
        int idx = static_cast<int>(ZOMBIE_TYPE);
        auto enter_home_thres = get_enter_home_thres(ZOMBIE_TYPE);
        auto min = min_x[idx][tick - START_TICK];
        auto max = max_x[idx][tick - START_TICK];

        if (min.x != DEFAULT_MIN && !stop_min[idx]) {
            if (OUTPUT_AS_INT) {
                file << static_cast<int>(32768.0 * min.x);
            } else {
                file << std::setprecision(3) << min.x;
            }
            file << "," << std::setprecision(7) << min.dx << ",";
            stop_min[idx] = static_cast<int>(min.x) <= enter_home_thres;
        } else {
            file << ",,";
        }
        if (max.x != DEFAULT_MAX && !stop_max[idx]) {
            if (OUTPUT_AS_INT) {
                file << static_cast<int>(32768.0 * max.x);
            } else {
                file << std::setprecision(3) << max.x;
            }
            file << "," << std::setprecision(7) << max.dx << ",";
            stop_max[idx] = static_cast<int>(max.x) <= enter_home_thres;
        } else {
            file << ",,";
        }
        file << "\n";
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";

    return 0;
}