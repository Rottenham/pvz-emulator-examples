/* 测试炸率.
 */

#include "common/pe.h"
#include "common/test.h"
#include "seml/explode/lib.h"
#include "world.h"

#include <mutex>
#include <optional>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

void validate_config(Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "请提供操作." << std::endl;
        exit(1);
    }
    for (auto& wave : config.waves) {
        if (wave.start_tick == -1) {
            wave.start_tick = wave.wave_length;
        }
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

std::mutex mtx;
Table table;

void test_one(const Config& config, int repeat)
{
    world w(config.setting.scene_type);
    Table local_table;

    for (int r = 0; r < repeat; r++) {
        std::vector<Test> tests;
        tests.reserve(config.waves.size());

        for (const auto& wave : config.waves) {
            Test test;
            load_wave(config.setting, wave, test);

            w.scene.reset();
            w.scene.stop_spawn = true;

            auto it = test.ops.begin();
            int curr_tick = it->tick; // there is at least 1 op (setup)

            for (; it != test.ops.end() && it->tick < wave.start_tick; it++) {
                run(w, curr_tick, it->tick);
                it->f(w);
            }
            run(w, curr_tick, wave.start_tick);

            while (curr_tick <= wave.wave_length) {
                for (; it != test.ops.end() && it->tick == curr_tick; it++) {
                    it->f(w);
                }

                std::array<LossInfo, 6> loss_info = {};
                for (const auto& plant : test.protect_plants) {
                    loss_info[plant->row] = {plant->explode, plant->max_hp - plant->hp};
                };
                test.loss_infos.push_back(loss_info);

                run(w, curr_tick, curr_tick + 1);
            }
            tests.push_back(std::move(test));
        }

        local_table.update(tests);
    }

    std::lock_guard<std::mutex> guard(mtx);
    table.merge(local_table);
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

    std::vector<std::vector<std::string>> headers(config.waves.size());
    size_t max_header_count = 0;
    for (size_t i = 0; i < config.waves.size(); i++) {
        const auto& wave = config.waves[i];
        for (const auto& action : wave.actions) {
            headers[i].push_back(desc(action));
        }
        max_header_count = std::max(max_header_count, headers[i].size());
    }

    std::pair<int, int> tick_range {INT_MAX, 0};
    for (const auto& wave : config.waves) {
        tick_range.first = std::min(tick_range.first, wave.start_tick);
        tick_range.second = std::max(tick_range.second, wave.wave_length);
    }

    for (const auto& str : {"炮伤", "瞬伤", "损伤"}) {
        file << "," << str;
        for (size_t i = 0; i < headers.size(); i++) {
            file << ",";
        }
    }
    file << "\n";

    for (size_t i = 0; i < max_header_count; i++) {
        if (i == max_header_count - 1) {
            file << "时刻";
        }
        file << ",";

        for (int r = 0; r < 3; r++) { // for "炮伤", "瞬伤", "损伤"
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

    for (int tick = tick_range.first; tick <= tick_range.second; tick++) {

        file << tick << ",";

        std::vector<std::optional<double>> loss_list;
        std::vector<std::optional<double>> explode_loss_list;
        std::vector<std::optional<double>> hp_loss_list;

        for (const auto& test_info : table.test_infos) {
            if (tick < test_info.start_tick
                || tick
                    >= test_info.start_tick + static_cast<int>(test_info.merged_loss_info.size())) {
                loss_list.push_back(std::nullopt);
                explode_loss_list.push_back(std::nullopt);
                hp_loss_list.push_back(std::nullopt);
            } else {
                const auto& loss_info = test_info.merged_loss_info[tick - test_info.start_tick];
                double explode = (loss_info.explode.from_upper + loss_info.explode.from_same
                                     + loss_info.explode.from_lower)
                    * 300.0;
                double hp = loss_info.hp_loss;
                loss_list.push_back((explode + hp) / table.repeat);
                explode_loss_list.push_back(explode / table.repeat);
                hp_loss_list.push_back(hp / table.repeat);
            }
        }

        file << to_string(loss_list) << "," << to_string(explode_loss_list) << ","
             << to_string(hp_loss_list) << ",";

        file << "\n";
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程." << std::endl;

    return 0;
}