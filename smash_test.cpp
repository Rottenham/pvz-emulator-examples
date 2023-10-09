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

const int TOTAL_REPEAT_NUM = 32;
const int THREAD_NUM = 32;            // if not sure, use number of CPU cores
const int GIGA_NUM_PER_REPEAT = 1000; // must not exceed 1024
                                      // is the total of giga from all waves

std::mutex mtx;

RawTable raw_table;

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Info info;

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

        mtx.lock();
        update_raw_table(info, raw_table);
        mtx.unlock();
    }
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

    Info info;
    load_config(config, info);
    file << "smashed,total,";
    int prev_wave = -1;
    for (const auto& op_info : info.op_infos) {
        if (op_info.wave != prev_wave) {
            file << "w" << op_info.wave << " ";
            prev_wave = op_info.wave;
        }
        file << op_info.desc << ",";
    }
    file << "\n";

    for (const auto& row : generate_table(raw_table)) {
        file << row.second.smashed_garg_count << "," << row.second.total_garg_count << ",";
        for (const auto& op_state : row.first) {
            file << op_state_to_string(op_state) << ",";
        }
        file << "\n";
    }

    file.close();
    return 0;
}