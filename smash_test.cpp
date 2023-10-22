/* 测试砸率.
 */

#include "common/io.h"
#include "common/pe.h"
#include "seml/smash/lib.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

std::mutex mtx;

RawTable raw_table;

void validate_config(const Config& config)
{
    if (config.rounds.empty()) {
        std::cerr << "Must provide at least one round." << std::endl;
        exit(1);
    }

    if (config.rounds.size() > 1) {
        std::cout << "Warning: " << config.rounds.size()
                  << " rounds were provided, but smash test will ignore all except the first round."
                  << std::endl;
    }

    if (config.rounds[0].empty()) {
        std::cerr << "Must provide at least one wave." << std::endl;
        exit(1);
    }

    if (config.rounds[0].size() > 200) {
        std::cerr << "Total number of waves must not exceed 200: " << config.rounds.size()
                  << std::endl;
        exit(1);
    }

    if (config.setting.protect_positions.empty()) {
        std::cerr << "Must provide at least one protect position." << std::endl;
        exit(1);
    }

    std::unordered_set<int> protect_rows;
    for (const auto& protect_position : config.setting.protect_positions) {
        if (protect_rows.count(protect_position.row)) {
            std::cerr << "Cannot provide multiple protect positions on the same row: "
                      << protect_position.row << std::endl;
            exit(1);
        }
        protect_rows.insert(protect_position.row);
    }
}

bool contains_smart_fodder(const Round& round)
{
    for (const auto& wave : round) {
        for (const auto& action : wave.actions) {
            if (std::holds_alternative<SmartFodder>(action)) {
                return true;
            }
        }
    }
    return false;
}

int get_giga_total(const Round& round)
{
    if (contains_smart_fodder(round)) {
        return 5 * static_cast<int>(round.size());
    } else {
        return 1000;
    }
}

double calc_smash_rate(const Config& config, int smashed_garg_count, int total_garg_count)
{
    int total_garg_rows = is_backyard(config.setting.scene_type) ? 4 : 5;
    return 100.0 * (smashed_garg_count / (total_garg_count / 5.0))
        * (static_cast<double>(config.setting.protect_positions.size()) / total_garg_rows);
}

void test_one(const Config& config, int repeat, int giga_total)
{
    world w(config.setting.scene_type);
    Info info;
    RawTable local_raw_table;

    for (int r = 0; r < repeat; r++) {
        auto ops = load_round(config.setting, config.rounds[0], info, giga_total);

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

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);
    auto giga_total = get_giga_total(config.rounds[0]);
    if (giga_total != 1000) {
        total_repeat_num = static_cast<int>(total_repeat_num * 1000.0 / giga_total);
    }

    std::vector<std::thread> threads;
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back(
            [config, repeat, giga_total]() { test_one(config, repeat, giga_total); });
    }
    for (auto& t : threads) {
        t.join();
    }

    auto [table, summary] = generate_table_and_summary(raw_table);

    file << "Smash rate per wave,";
    for (const auto& wave : summary.waves) {
        file << "w" << wave << ",";
    }
    file << "\n";
    file << "Total,";
    for (const auto& wave : summary.waves) {
        const auto& garg_summary = summary.garg_summary_by_wave.at(wave);
        file << std::fixed << std::setprecision(2)
             << calc_smash_rate(
                    config, garg_summary.smashed_garg_count, garg_summary.total_garg_count)
             << "%,";
    }
    file << "\n";

    for (const auto& protect_position : config.setting.protect_positions) {
        file << "Row " << protect_position.row << " Col " << protect_position.col << " ";
        if (is_cob(protect_position)) {
            file << "Cob,";
        } else {
            file << "Normal,";
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

    Info info;
    load_round(config.setting, config.rounds[0], info, 1000);

    file << "\nWave,Smash rate per wave,Smash count,Total count,";
    auto prev_wave = -1;
    for (const auto& action_info : info.action_infos) {
        if (action_info.wave != prev_wave) {
            file << "[w" << action_info.wave << "] ";
            prev_wave = action_info.wave;
        }
        file << action_info.desc << ",";
    }
    for (const auto& protect_position : config.setting.protect_positions) {
        file << "Row " << protect_position.row << ",";
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

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Output file has been saved to " << full_output_file << ".\n"
              << "Finished in " << std::fixed << std::setprecision(2) << elapsed.count()
              << "s with " << threads.size() << " threads.";

    return 0;
}