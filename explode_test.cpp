/* 测试炸率.
 */

#include "common/io.h"
#include "common/pe.h"
#include "seml/explode/lib.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

std::mutex mtx;

void validate_config(const Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "必须提供操作" << std::endl;
        exit(1);
    }

    if (config.waves.size() > 200) {
        std::cerr << "波数不能超过 200" << std::endl;
        exit(1);
    }

    if (config.setting.protect_positions.empty()) {
        std::cerr << "必须提供要保护的位置" << std::endl;
        exit(1);
    }

    std::unordered_set<int> valid_rows;
    if (is_backyard(config.setting.scene_type)) {
        valid_rows = {1, 2, 5, 6};
    } else {
        valid_rows = {1, 2, 3, 4, 5};
    }

    for (const auto& protect_position : config.setting.protect_positions) {
        if (!valid_rows.count(protect_position.row)) {
            std::cerr << "保护位置所在行无效: " << protect_position.row << std::endl;
            exit(1);
        }
    }
}

Table table;

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Table local_table;

    for (int r = 0; r < repeat; r++) {
        std::vector<Test> tests;
        tests.reserve(config.waves.size());

        for (int wave_num = 0; wave_num < config.waves.size(); wave_num++) {
            const auto& wave = config.waves[wave_num];

            w.scene.reset();
            w.scene.stop_spawn = true;

            Test test;
            load_config(config, wave_num, test);

            auto it = test.ops.begin();
            auto prev_tick = it->tick;

            while (it != test.ops.end() && it->tick < wave.start_tick) {
                run(w, it->tick - prev_tick);
                it->f(w);
                prev_tick = it->tick;
                it++;
            }
            run(w, wave.start_tick - prev_tick);

            for (int tick = wave.start_tick; tick <= wave.wave_length - 200; tick++) {
                while (it != test.ops.end() && it->tick == tick) {
                    it->f(w);
                    it++;
                }

                test.loss_infos.push_back({});

                for (const auto& plant : test.plants) {
                    test.loss_infos.back()[plant->row]
                        = {plant->explode, plant->max_hp - plant->hp};
                };
                run(w, 1);
            }
            tests.push_back(std::move(test));
        }

        update_table(local_table, tests);
    }

    std::lock_guard<std::mutex> guard(mtx);
    merge_table(local_table, table);
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();

    ::system("chcp 65001 > nul");

    std::vector<std::string> args(argv, argv + argc);
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "explode_test");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "30000"));

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);

    std::vector<std::thread> threads;
    for (const auto& repeat :
        assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back([config, repeat]() { test_one(config, repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    int max_action_count = 0;
    int min_start_tick = INT_MAX;
    int max_wave_length = 0;
    for (const auto& wave : config.waves) {
        max_action_count = std::max(max_action_count, static_cast<int>(wave.actions.size()));
        min_start_tick = std::min(min_start_tick, wave.start_tick);
        max_wave_length = std::max(max_wave_length, wave.wave_length - 200);
    }

    file << ",";
    for (const auto& wave : config.waves) {
        for (int i = 0; i < wave.ice_times.size(); i++) {
            if (i != 0) {
                file << " ";
            }
            file << wave.ice_times[i];
        }
        if (!wave.ice_times.empty()) {
            file << "冰";
        }
        file << " " << wave.start_tick << "~" << wave.wave_length - 200 << ",";
    }
    file << "\n";

    for (int i = 0; i < max_action_count; i++) {
        if (i == max_action_count - 1) {
            file << "tick";
        }
        file << ",";
        for (const auto& wave : config.waves) {
            if (i < wave.actions.size()) {
                file << desc(wave.actions[i]);
            }
            file << ",";
        }
        file << "\n";
    }

    for (int tick = min_start_tick; tick <= max_wave_length; tick++) {
        file << tick << ",";
        for (int test_num = 0; test_num < config.waves.size(); test_num++) {
            const auto& wave = config.waves[test_num];

            if (tick >= wave.start_tick && tick <= wave.wave_length - 200) {
                file << (sum(table.data[test_num][tick - wave.start_tick].explode) * 300.0
                            + table.data[test_num][tick - wave.start_tick].hp_loss)
                        / table.repeat;
            }
            file << ",";

             if (tick >= wave.start_tick && tick <= wave.wave_length - 200) {
                file << static_cast<double>(table.data[test_num][tick - wave.start_tick].hp_loss)
                        / table.repeat;
            }
            file << ",";
        }
        file << "\n";
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";

    return 0;
}