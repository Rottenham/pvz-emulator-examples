/* 测试砸率.
 */

#include "common/pe.h"
#include "common/test.h"
#include "seml/smash/lib.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

std::mutex mtx;

TestInfo test_info;

void validate_config(const Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "请提供操作." << std::endl;
        exit(1);
    }

    if (config.waves.size() > 200) {
        std::cerr << "波数超过 200: " << config.waves.size() << std::endl;
        exit(1);
    }

    if (config.setting.protect_positions.empty()) {
        std::cerr << "请提供保护位置." << std::endl;
        exit(1);
    }

    std::unordered_set<int> protect_rows;
    for (const auto& protect_position : config.setting.protect_positions) {
        if (protect_rows.count(protect_position.row)) {
            std::cerr << "保护位置行重复: " << protect_position.row << std::endl;
            exit(1);
        }
        protect_rows.insert(protect_position.row);
    }
}

double calc_smash_rate(const Config& config, int smashed_garg_count, int total_garg_count)
{
    int total_garg_rows = is_backyard(config.setting.scene_type) ? 4 : 5;
    return 100.0 * (smashed_garg_count / (total_garg_count / 5.0))
        * (static_cast<double>(config.setting.protect_positions.size()) / total_garg_rows);
}

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Test test;
    TestInfo local_test_info;

    for (int r = 0; r < repeat; r++) {
        load_config(config, test);

        w.scene.reset();
        w.scene.stop_spawn = true;
        w.scene.disable_garg_throw_imp = true;

        auto prev_tick = test.ops.front().tick;
        for (const auto& op : test.ops) {
            run(w, op.tick - prev_tick);
            op.f(w);
            prev_tick = op.tick;
        }

        local_test_info.update(test);
    }

    std::lock_guard<std::mutex> guard(mtx);
    test_info.merge(local_test_info);
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();

    ::system("chcp 65001 > nul");

    auto args = parse_cmd_line();
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "smash_test");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "10000"));

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);

    std::vector<std::thread> threads;
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back(
            [config, repeat]() { test_one(config, repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    auto [table, summary] = test_info.make_table_and_summary();

    file << "单波砸率,";
    for (const auto& wave : summary.waves) {
        file << "w" << wave << ",";
    }
    file << "\n";
    file << "总和,";
    for (const auto& wave : summary.waves) {
        const auto& garg_summary = summary.garg_summary_by_wave.at(wave);
        file << std::fixed << std::setprecision(2)
             << calc_smash_rate(
                    config, garg_summary.smashed_garg_count, garg_summary.total_garg_count)
             << "%,";
    }
    file << "\n";

    for (const auto& protect_position : config.setting.protect_positions) {
        file << protect_position.row << "路" << protect_position.col;
        if (protect_position.is_cob()) {
            file << "炮,";
        } else {
            file << "普通,";
        }
        for (const auto& wave : summary.waves) {
            const auto& garg_summary = summary.garg_summary_by_wave.at(wave);

            file << std::fixed << std::setprecision(2)
                 << calc_smash_rate(config,
                        garg_summary.smashed_garg_count_by_row.at(protect_position.row - 1),
                        garg_summary.total_garg_count)
                 << "%,";
        }
        file << "\n";
    }

    Test test;
    load_config(config, test);

    file << "\n出生波数,单波砸率,砸炮数,总数,";
    auto prev_wave = -1;
    for (const auto& action_info : test.action_infos) {
        if (action_info.wave != prev_wave) {
            file << "[w" << action_info.wave << "] ";
            prev_wave = action_info.wave;
        }
        file << action_info.desc << ",";
    }
    for (const auto& protect_position : config.setting.protect_positions) {
        file << protect_position.row << "路,";
    }
    file << "\n";

    for (const auto& [os, data] : table) {
        file << os.wave << "," << std::fixed << std::setprecision(2)
             << calc_smash_rate(config, data.smashed_garg_count,
                    summary.garg_summary_by_wave.at(os.wave).total_garg_count)
             << "%," << data.smashed_garg_count << "," << data.total_garg_count << ",";
        for (const auto& op_state : os.op_states) {
            file << op_state_to_string(op_state) << ",";
        }
        for (const auto& protect_position : config.setting.protect_positions) {
            file << data.smashed_garg_count_by_row[protect_position.row - 1] << ",";
        }
        file << "\n";
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程." << std::endl;

    return 0;
}