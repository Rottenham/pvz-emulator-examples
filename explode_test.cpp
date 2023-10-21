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

void validate_config(Config& config)
{
    if (config.rounds.empty()) {
        std::cerr << "Must provide at least one round." << std::endl;
        exit(1);
    }

    for (auto& round : config.rounds) {
        if (round.empty()) {
            std::cerr << "Must provide at least one wave." << std::endl;
            exit(1);
        }
        if (round.size() > 200) {
            std::cerr << "Total number of waves must not exceed 200." << std::endl;
            exit(1);
        }
        for (auto& wave : round) {
            if (wave.start_tick == -1) {
                wave.start_tick = wave.wave_length - 200;
            }
        }
    }

    if (config.setting.protect_positions.empty()) {
        std::cerr << "Must provide protect positions." << std::endl;
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
            std::cerr << "Invalid row for protect position: " << protect_position.row << std::endl;
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
        tests.reserve(config.rounds.size());

        for (int round_num = 0; round_num < config.rounds.size(); round_num++) {
            const auto& round = config.rounds[round_num];

            w.scene.reset();
            w.scene.stop_spawn = true;

            Test test;
            load_config(config, round_num, test);

            auto it = test.ops.begin();
            int curr_tick = it->tick;
            int base_tick = 0;
            for (int wave_num = 0; wave_num < round.size(); wave_num++) {
                const auto& wave = round[wave_num];

                while (it != test.ops.end() && it->tick < base_tick + wave.start_tick) {
                    run(w, curr_tick, it->tick);
                    it->f(w);
                    it++;
                }
                run(w, curr_tick, base_tick + wave.start_tick);

                while (curr_tick <= base_tick + wave.wave_length - 200) {
                    while (it != test.ops.end() && it->tick == curr_tick) {
                        it->f(w);
                        it++;
                    }

                    test.wave_infos[wave_num].loss_infos.push_back({});

                    for (const auto& plant : test.plants) {
                        test.wave_infos[wave_num].loss_infos.back()[plant->row]
                            = {plant->explode, plant->max_hp - plant->hp};
                    };
                    run(w, curr_tick, curr_tick + 1);
                }
                base_tick += wave.wave_length;
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
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "10000"));

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

    int max_headaer_count = 0;
    std::vector<std::vector<std::string>> headers(config.rounds.size());
    int max_wave = 0;
    for (int round_num = 0; round_num < config.rounds.size(); round_num++) {
        const auto& round = config.rounds[round_num];

        for (int wave_num = 0; wave_num < round.size(); wave_num++) {
            const auto& wave = round[wave_num];

            std::string prefix = "[w" + std::to_string(wave_num + 1) + "] ";
            for (const auto& action : wave.actions) {
                headers[round_num].push_back(prefix + desc(action));
                prefix = "";
            }
            if (!prefix.empty()) {
                headers[round_num].push_back(prefix);
            }
        }
        max_headaer_count
            = std::max(max_headaer_count, static_cast<int>(headers[round_num].size()));
        max_wave = std::max(max_wave, static_cast<int>(round.size()));
    }

    std::vector<std::pair<int, int>> tick_range(max_wave, {INT_MAX, 0});
    for (const auto& round : config.rounds) {
        for (int wave_num = 0; wave_num < round.size(); wave_num++) {
            const auto& wave = round[wave_num];
            tick_range[wave_num].first = std::min(tick_range[wave_num].first, wave.start_tick);
            tick_range[wave_num].second = std::max(tick_range[wave_num].second, wave.wave_length);
        }
    }

    file << ",,炮伤";
    for (int i = 0; i < headers.size(); i++) {
        file << ",";
    }
    file << ",来自爆炸的炮伤";
    for (int i = 0; i < headers.size(); i++) {
        file << ",";
    }
    file << ",来自啃食的炮伤";
    for (int i = 0; i < headers.size(); i++) {
        file << ",";
    }
    file << "\n";

    for (int i = 0; i < max_headaer_count; i++) {
        if (i == max_headaer_count - 1) {
            file << "波数,时刻,";
        } else {
            file << ",,";
        }

        for (int r = 0; r < 3; r++) {
            for (const auto& header : headers) {
                if (i < header.size()) {
                    file << header[i];
                }
                file << ",";
            }
            file << ",";
        }

        file << "\n";
    }

    auto to_string = [](const std::vector<double>& loss_list) -> std::string {
        double min = -1;
        int min_idx = -1;

        auto valid_count = std::count_if(
            loss_list.begin(), loss_list.end(), [](const auto& loss) { return loss >= 0; });

        if (valid_count > 1) {
            for (int i = 0; i < loss_list.size(); i++) {
                if (loss_list[i] >= 0 && (min < 0 || loss_list[i] <= min)) {
                    min = loss_list[i];
                    min_idx = i;
                }
            }
        }
        
        std::ostringstream os;
        for (int i = 0; i < loss_list.size(); i++) {
            if (loss_list[i] >= 0) {
                if (i == min_idx) {
                    os << "[" << loss_list[i] << "]";
                } else {
                    os << loss_list[i];
                }
            }
            os << ",";
        }
        return os.str();
    };

    for (int wave_num = 0; wave_num < max_wave; wave_num++) {
        for (int tick = tick_range[wave_num].first; tick < tick_range[wave_num].second - 200;
             tick++) {

            file << wave_num + 1 << "," << tick << ",";

            std::vector<double> loss_list;
            std::vector<double> explode_loss_list;
            std::vector<double> hp_loss_list;

            for (const auto& merged_round_info : table.merged_round_infos) {
                if (wave_num < merged_round_info.size()
                    && tick >= merged_round_info[wave_num].start_tick
                    && tick < merged_round_info[wave_num].start_tick
                            + merged_round_info[wave_num].merged_loss_info.size()) {
                    int idx = tick - merged_round_info[wave_num].start_tick;
                    double explode
                        = sum(merged_round_info[wave_num].merged_loss_info[idx].explode) * 300.0;
                    double hp = merged_round_info[wave_num].merged_loss_info[idx].hp_loss;
                    loss_list.push_back((explode + hp) / table.repeat);
                    explode_loss_list.push_back(explode / table.repeat);
                    hp_loss_list.push_back(hp / table.repeat);
                } else {
                    loss_list.push_back(-1);
                    explode_loss_list.push_back(-1);
                    hp_loss_list.push_back(-1);
                }
            }

            file << to_string(loss_list) << "," << to_string(explode_loss_list) << ","
                 << to_string(hp_loss_list) << ",";

            file << "\n";
        }
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程.";

    return 0;
}