/* 测试炸率.
 */

#include "common/io.h"
#include "common/pe.h"
#include "seml/explode/lib.h"
#include "world.h"

#include <mutex>
#include <optional>

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
            std::cerr << "Total number of waves must not exceed 200: " << config.rounds.size()
                      << std::endl;
            exit(1);
        }
        for (auto& wave : round) {
            if (wave.start_tick == -1) {
                wave.start_tick = wave.wave_length - 200;
            }
        }
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

Table table;

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Table local_table;

    for (int r = 0; r < repeat; r++) {
        std::vector<Test> tests;
        tests.reserve(config.rounds.size());

        for (const auto& round : config.rounds) {
            w.scene.reset();
            w.scene.stop_spawn = true;

            Test test;
            load_round(config.setting, round, test);

            auto it = test.ops.begin();
            int curr_tick = it->tick; // there are at least 2 op (setup and spawning)
            int base_tick = 0;
            for (size_t wave_num = 0; wave_num < round.size(); wave_num++) {
                const auto& wave = round[wave_num];

                for (; it != test.ops.end() && it->tick < base_tick + wave.start_tick; it++) {
                    run(w, curr_tick, it->tick);
                    it->f(w);
                }
                run(w, curr_tick, base_tick + wave.start_tick);

                while (curr_tick <= base_tick + wave.wave_length - 200) {
                    for (; it != test.ops.end() && it->tick == curr_tick; it++) {
                        it->f(w);
                    }

                    std::array<LossInfo, 6> loss_info = {};
                    for (const auto& plant : test.plants) {
                        loss_info[plant->row] = {plant->explode, plant->max_hp - plant->hp};
                    };
                    test.wave_infos[wave_num].loss_infos.push_back(loss_info);

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
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back([config, repeat]() { test_one(config, repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::vector<std::vector<std::string>> headers(config.rounds.size());
    size_t max_headaer_count = 0;
    size_t max_wave = 0;
    for (size_t round_num = 0; round_num < config.rounds.size(); round_num++) {
        const auto& round = config.rounds[round_num];

        for (size_t wave_num = 0; wave_num < round.size(); wave_num++) {
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
        max_headaer_count = std::max(max_headaer_count, headers[round_num].size());
        max_wave = std::max(max_wave, round.size());
    }

    std::vector<std::pair<int, int>> tick_range(max_wave, {INT_MAX, 0});
    for (const auto& round : config.rounds) {
        for (size_t wave_num = 0; wave_num < round.size(); wave_num++) {
            const auto& wave = round[wave_num];
            tick_range[wave_num].first = std::min(tick_range[wave_num].first, wave.start_tick);
            tick_range[wave_num].second = std::max(tick_range[wave_num].second, wave.wave_length);
        }
    }

    file << ",";
    for (const auto& str : {"Loss", "Loss from explosion", "Loss from eating"}) {
        file << "," << str;
        for (size_t i = 0; i < headers.size(); i++) {
            file << ",";
        }
    }
    file << "\n";

    for (size_t i = 0; i < max_headaer_count; i++) {
        if (i == max_headaer_count - 1) {
            file << "wave,tick,";
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

    auto to_string = [](const std::vector<std::optional<double>>& loss_list) -> std::string {
        std::optional<double> min;
        std::optional<size_t> min_idx;

        auto valid_count = std::count_if(
            loss_list.begin(), loss_list.end(), [](const auto& loss) { return loss.has_value(); });

        if (valid_count > 1) {
            for (size_t i = 0; i < loss_list.size(); i++) {
                if (loss_list[i].has_value() && (!min.has_value() || loss_list[i] <= min)) {
                    min = loss_list[i];
                    min_idx = i;
                }
            }
        }

        std::ostringstream os;
        for (size_t i = 0; i < loss_list.size(); i++) {
            if (loss_list[i].has_value()) {
                if (min_idx.has_value() && *min_idx == i) {
                    os << "[" << *loss_list[i] << "]";
                } else {
                    os << *loss_list[i];
                }
            }
            os << ",";
        }
        return os.str();
    };

    for (size_t wave_num = 0; wave_num < max_wave; wave_num++) {
        for (int tick = tick_range[wave_num].first; tick <= tick_range[wave_num].second - 200;
             tick++) {

            file << wave_num + 1 << "," << tick << ",";

            std::vector<std::optional<double>> loss_list;
            std::vector<std::optional<double>> explode_loss_list;
            std::vector<std::optional<double>> hp_loss_list;

            for (const auto& merged_round_info : table.merged_round_infos) {
                if (wave_num < merged_round_info.size()
                    && tick >= merged_round_info[wave_num].start_tick
                    && tick < merged_round_info[wave_num].start_tick
                            + static_cast<int>(
                                merged_round_info[wave_num].merged_loss_info.size())) {
                    const auto& loss_info
                        = merged_round_info[wave_num]
                              .merged_loss_info[tick - merged_round_info[wave_num].start_tick];
                    double explode = (loss_info.explode.from_upper + loss_info.explode.from_same
                                         + loss_info.explode.from_lower)
                        * 300.0;
                    double hp = loss_info.hp_loss;
                    loss_list.push_back((explode + hp) / table.repeat);
                    explode_loss_list.push_back(explode / table.repeat);
                    hp_loss_list.push_back(hp / table.repeat);
                } else {
                    loss_list.push_back(std::nullopt);
                    explode_loss_list.push_back(std::nullopt);
                    hp_loss_list.push_back(std::nullopt);
                }
            }

            file << to_string(loss_list) << "," << to_string(explode_loss_list) << ","
                 << to_string(hp_loss_list) << ",";

            file << "\n";
        }
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Output file has been saved to " << full_output_file << ".\n"
              << "Finished in " << std::fixed << std::setprecision(2) << elapsed.count()
              << "s with " << threads.size() << " threads.";

    return 0;
}