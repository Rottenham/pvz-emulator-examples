/* 测试收跳跳的最左炮准星.
 */

#include "common/pe.h"
#include "common/test.h"
#include "constants/constants.h"
#include "seml/reader/lib.h"
#include "world.h"

#include <mutex>
#include <optional>

using namespace pvz_emulator;
using namespace pvz_emulator::object;
using namespace pvz_emulator::system;

std::mutex mtx;

std::vector<std::vector<std::pair<int, int>>> cob_ranges;
std::optional<int> ice_time;
int hit_cob_col = -1;

bool is_range_overlap(std::pair<int, int> range1, std::pair<int, int> range2)
{
    return (range1.first <= range2.second && range1.second >= range2.first)
        || (range2.first <= range1.second && range2.second >= range1.first);
}

std::pair<int, int> get_col_range(const Setting::ProtectPos& protect_position)
{
    if (protect_position.is_cob()) {
        return {protect_position.col - 1, protect_position.col};
    } else {
        return {protect_position.col, protect_position.col};
    }
}

void get_hit_cob_col(const Wave& wave)
{
    for (const auto& action : wave.actions) {
        if (auto a = std::get_if<Cob>(&action)) {
            hit_cob_col = a->cob_col;
            return;
        }
    }
    std::cerr << "屋顶场合, 请提供炮尾所在列." << std::endl;
    exit(1);
}

void validate_config(Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "请提供操作." << std::endl;
        exit(1);
    }

    if (config.waves.size() > 1) {
        std::cout << "警告: 共有 " << config.waves.size() << " 波操作, 但跳跳测试只考虑第 1 波."
                  << std::endl;
    }

    auto& wave = config.waves[0];

    if (is_roof(config.setting.scene_type)) {
        get_hit_cob_col(wave);
    }

    if (wave.start_tick == -1) {
        wave.start_tick = wave.wave_length - 200;
    }

    if (!wave.ice_times.empty()) {
        ice_time = wave.ice_times.front();
    }

    std::vector<Setting::ProtectPos> prev_protect_positions;
    for (const auto& protect_position : config.setting.protect_positions) {
        for (const auto& prev_position : prev_protect_positions) {
            if (is_range_overlap(get_col_range(protect_position), get_col_range(prev_position))) {
                std::cerr << "列重复: " << prev_position.col << ", " << protect_position.col
                          << std::endl;
                exit(1);
            }
        }
    }
    cob_ranges.resize(
        3, std::vector<std::pair<int, int>>(wave.wave_length - wave.start_tick + 1, {-9999, 9999}));
}

int count = 0;
void test(const Config& config, int repeat)
{
    const auto& wave = config.waves[0];
    world w(config.setting.scene_type);
    std::vector<std::vector<std::pair<int, int>>> local_cob_ranges(
        3, std::vector<std::pair<int, int>>(wave.wave_length - wave.start_tick + 1, {-9999, 9999}));

    int local_count = 0;
    for (int r = 0; r < repeat; r++) {
        w.scene.reset();
        w.scene.stop_spawn = true;

        for (int tick = -100; tick <= wave.wave_length; tick++, run(w, 1)) {
            if (ice_time.has_value() && tick == *ice_time - 99) {
                w.plant_factory.create(plant_type::iceshroom, 0, 0, plant_type::none, true);
            }

            if (tick == 0) {
                for (const auto& protect_position : config.setting.protect_positions) {
                    if (protect_position.is_cob()) {
                        w.plant_factory.create(plant_type::cob_cannon, 1, protect_position.col - 2);
                    } else {
                        w.plant_factory.create(
                            plant_type::umbrella_leaf, 1, protect_position.col - 1);
                    }
                }
                for (int i = 0; i < 1000; i++) {
                    w.zombie_factory.create(zombie_type::pogo, 1);
                }
            }

            if (tick >= wave.start_tick) {
                for (auto& z : w.scene.zombies) {
                    for (int diff = -1; diff <= 1; diff++) {
                        auto new_cob_range = get_cob_hit_x_range(get_hit_box(z),
                            get_cob_hit_xy(w.scene.type, (z.row + 1) + diff, 9.0f, hit_cob_col)
                                .second);
                        auto& old_cob_range = local_cob_ranges[diff + 1][tick - wave.start_tick];

                        old_cob_range.first = std::max(old_cob_range.first, new_cob_range.first);
                        old_cob_range.second = std::min(old_cob_range.second, new_cob_range.second);
                        if (tick == 1669 && diff == -1 && new_cob_range.second < 678) {
                            local_count++;
                        }
                    }
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    count += local_count;
    for (int tick = wave.start_tick; tick <= wave.wave_length; tick++) {
        for (int i = 0; i < 3; i++) {
            auto& local_cob_range = local_cob_ranges[i][tick - wave.start_tick];
            auto& global_cob_range = cob_ranges[i][tick - wave.start_tick];
            global_cob_range.first = std::max(global_cob_range.first, local_cob_range.first);
            global_cob_range.second = std::min(global_cob_range.second, local_cob_range.second);
        }
    }
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();
    ::system("chcp 65001 > nul");

    std::vector<std::string> args(argv, argv + argc);
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "pogo_test2");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "1000"));

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);

    std::vector<std::thread> threads;
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back([config, repeat]() { test(config, repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << count << "\n";
    file << "时刻,收上行跳跳左,右,收本行跳跳左,右,收下行跳跳左,右,"
         << "\n";
    const auto& wave = config.waves[0];
    for (int tick = wave.start_tick; tick <= wave.wave_length; tick++) {
        file << tick << ",";
        for (int i = 2; i >= 0; i--) {
            auto& cob_range = cob_ranges[i][tick - wave.start_tick];
            if (std::abs(cob_range.first) == 9999)
                file << "ERR";
            else
                file << cob_range.first;
            file << ",";
            if (std::abs(cob_range.second) == 9999)
                file << "ERR";
            else
                file << cob_range.second;
            file << ",";
        }
        file << "\n";
    }
    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程." << std::endl;
    return 0;
}