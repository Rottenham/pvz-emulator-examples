/* 测试砸率.
 */

#include "common/io.h"
#include "common/pe.h"
#include "smash/lib.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

std::mutex mtx;

RawTable raw_table;

void validate_config(const Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "必须提供操作" << std::endl;
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

double calc_smash_rate(const Config& config, int smashed_garg_count, int total_garg_count)
{
    int total_garg_rows = is_backyard(config.setting.scene_type) ? 4 : 5;
    return 100.0 * (smashed_garg_count / (total_garg_count / 5.0))
        * (static_cast<double>(config.setting.protect_positions.size()) / total_garg_rows);
}

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Info info;
    RawTable local_raw_table;

    for (int r = 0; r < repeat; r++) {
        auto ops = load_config(config, info);

        w.scene.reset();
        w.scene.stop_spawn = true;
        w.scene.disable_garg_throw_imp = true;

        auto prev_tick = ops.front().tick;
        for (const auto& op : ops) {
            run(w, op.tick - prev_tick);
            op.f(w);
            prev_tick = op.tick;
        }

        update_raw_table(info, local_raw_table);
    }

    std::lock_guard<std::mutex> guard(mtx);
    merge_raw_table(local_raw_table, raw_table);
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();

    ::system("chcp 65001 > nul");

    std::vector<std::string> args(argv, argv + argc);
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "smash_test");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "300"));
    auto thread_num
        = std::stoi(get_cmd_arg(args, "t", std::to_string(std::thread::hardware_concurrency())));

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);
    if (config.setting.giga_total != 1000) {
        total_repeat_num = static_cast<int>(total_repeat_num * 1000.0 / config.setting.giga_total);
    }

    std::vector<std::thread> threads;
    for (const auto& repeat : assign_repeat(total_repeat_num, thread_num)) {
        threads.emplace_back([config, repeat]() { test_one(config, repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    auto [table, summary] = generate_table_and_summary(raw_table);

    file << "每波砸率,";
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
        file << protect_position.row << "路,";
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

    Info info;
    load_config(config, info);

    file << "\n出生波数,每波砸率,砸炮数,总数,";
    auto prev_wave = -1;
    for (const auto& action_info : info.action_infos) {
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

    for (const auto& [op_states, data] : table) {
        file << data.wave << "," << std::fixed << std::setprecision(2)
             << calc_smash_rate(config, data.smashed_garg_count,
                    summary.garg_summary_by_wave.at(data.wave).total_garg_count)
             << "%," << data.smashed_garg_count << "," << data.total_garg_count << ",";
        for (const auto& op_state : op_states) {
            file << op_state_to_string(op_state) << ",";
        }
        for (const auto& protect_position : config.setting.protect_positions) {
            file << data.smashed_garg_count_by_row[protect_position.row - 1] << ",";
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