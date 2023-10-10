/* WINDOWS POWERSHELL
 * g++ -O3 -o hello hello.cpp -Ilib -Ilib/lib -Llib/build -lpvzemu -Wfatal-errors; cd dest;
 * ./bin/refresh > refresh.csv; cd ..
 */

#include "common/io.h"
#include "common/pe.h"
#include "smash/lib.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

const std::string CONFIG_FILE = "1.json";
const std::string OUTPUT_FILE = "smash_test";
const std::string OUTPUT_FILE_EXT = ".csv";

const int TOTAL_REPEAT_NUM = 320;
const int THREAD_NUM = 32;            // if not sure, use number of CPU cores
const int GIGA_NUM_PER_REPEAT = 1000; // must not exceed 1024
                                      // is the total of giga from all waves

std::mutex mtx;

RawTable raw_table;

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Info info;
    RawTable local_raw_table;

    for (int r = 0; r < repeat; r++) {
        auto ops = load_config(config, info);

        w.reset();
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

    mtx.lock();
    merge_raw_table(local_raw_table, raw_table);
    mtx.unlock();
}

int main()
{
    ::system("chcp 65001 > nul");

    auto file = open_csv(OUTPUT_FILE);

    auto start = std::chrono::high_resolution_clock::now();

    auto config = read_json("1.json");

    std::vector<std::thread> threads;
    for (auto j = 0; j < THREAD_NUM; j++) {
        threads.emplace_back([config]() { test_one(config, TOTAL_REPEAT_NUM / THREAD_NUM); });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Finished in " << elapsed.count() << "s with " << threads.size() << " threads.\n";

    const auto [table, summary] = generate_table_and_summary(raw_table);

    file << "砸率,";
    for (const auto& wave : summary.waves) {
        file << "w" << wave << ",";
    }
    file << "\n";
    file << "总和,";
    for (const auto& wave: summary.waves) {
        const auto& garg_summary = summary.garg_summary_by_wave.at(wave);
        file << std::fixed << std::setprecision(2)
             << 100.0 * garg_summary.smashed_garg_count / garg_summary.total_garg_count
             << "%,";
    }
    file << "\n";

    for (const auto& protect_position : config.setting.protect_positions) {
        file << protect_position.row << "路,";
        for (const auto& wave : summary.waves) {
            const auto& garg_summary = summary.garg_summary_by_wave.at(wave);

            file << std::fixed << std::setprecision(2)
                 << 100.0 * garg_summary.smashed_garg_count_by_row.at(protect_position.row - 1)
                    / garg_summary.total_garg_count
                 << "%,";
        }
        file << "\n";
    }

    Info info;
    load_config(config, info);

    file << "\n出生波数,砸率,砸炮数,总数,";
    int prev_wave = -1;
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
             << 100.0 * data.smashed_garg_count
                / summary.garg_summary_by_wave.at(data.wave).total_garg_count
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
    return 0;
}