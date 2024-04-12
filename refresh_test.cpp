/* 测试意外刷新概率.
 */

#include "common/test.h"
#include "seml/refresh/lib.h"
#include "world.h"

#include <mutex>

using namespace pvz_emulator;
using namespace pvz_emulator::object;

ZombieTypes parse_zombie_types(const std::string& zombie_nums)
{
    ZombieTypes zombie_types;
    for (const auto& zombie_num : split(zombie_nums, ',')) {
        auto type = static_cast<zombie_type>(std::stoi(zombie_num));
        zombie_types.insert(type);
    }
    return zombie_types;
}

zombie_dance_cheat get_dance_cheat(bool use_dance_cheat, bool assume_activate)
{
    if (use_dance_cheat) {
        return assume_activate ? zombie_dance_cheat::fast : zombie_dance_cheat::slow;
    } else {
        return zombie_dance_cheat::none;
    }
}

void validate_config(Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "请提供操作." << std::endl;
        exit(1);
    }
}

double get_accident_rate(int init_hp, int curr_hp, bool assume_activate)
{
    double hp_ratio = static_cast<double>(curr_hp) / static_cast<double>(init_hp);
    double refresh_prob = (0.65 - std::clamp(hp_ratio, 0.5, 0.65)) / 0.15;
    return assume_activate ? 1.0 - refresh_prob : refresh_prob;
}

std::mutex mtx;
TestInfos test_infos;

void test_one(const Config& config, int repeat, const ZombieTypes& required_types,
    const ZombieTypes& banned_types, bool huge, bool assume_activate,
    zombie_dance_cheat dance_cheat, bool natural)
{
    std::mt19937 rng(
        static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
    world w(config.setting.scene_type);
    TestInfos local_test_infos;

    for (int round_idx = 0; round_idx < repeat; round_idx++) {
        auto spawn_types = get_spawn_types(
            rng, config.setting.original_scene_type, required_types, banned_types);

        for (int wave_idx = 0; wave_idx < 20; wave_idx++) { // test 20 times for each repeat
            std::vector<Test> tests;
            tests.reserve(config.waves.size());

            for (const auto& wave : config.waves) {
                Test test;
                load_wave(config.setting, wave, get_spawn_list(rng, spawn_types, huge, natural),
                    huge, dance_cheat, test);

                w.scene.reset();
                w.scene.stop_spawn = true;
                if (dance_cheat == zombie_dance_cheat::slow) {
                    w.scene.is_zombie_dance = true;
                }

                auto it = test.ops.begin();
                int curr_tick = it->tick; // there is at least 1 op (spawn)

                for (; it != test.ops.end() && it->tick < wave.wave_length - 200; it++) {
                    run(w, curr_tick, it->tick);
                    it->f(w);
                }
                run(w, curr_tick, wave.wave_length - 200);

                auto curr_hp = w.spawn.get_current_hp();
                auto accident_rate = get_accident_rate(test.init_hp, curr_hp, assume_activate);
                test.accident_rates[spawn_types].push_back(static_cast<float>(accident_rate));

                tests.push_back(std::move(test));
            }
            local_test_infos.update(tests);
        }
    }

    std::lock_guard<std::mutex> guard(mtx);
    test_infos.merge(local_test_infos);
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();

    ::system("chcp 65001 > nul");

    std::vector<std::string> args(argv, argv + argc);
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "refresh_test");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "1000"));
    auto required_types = parse_zombie_types(get_cmd_arg(args, "req", ""));
    auto banned_types = parse_zombie_types(get_cmd_arg(args, "ban", ""));
    auto huge = get_cmd_flag(args, "h");
    auto assume_activate = get_cmd_flag(args, "a");
    auto use_dance_cheat = get_cmd_flag(args, "d");
    auto natural = get_cmd_flag(args, "n");
    auto dance_cheat = get_dance_cheat(use_dance_cheat, assume_activate);

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);

    std::vector<std::thread> threads;
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back([config, repeat, required_types, banned_types, huge, assume_activate,
                                 dance_cheat, natural]() {
            test_one(config, repeat, required_types, banned_types, huge, assume_activate,
                dance_cheat, natural);
        });
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

    auto table = test_infos.make_table();

    file << "测试环境: " << scene_type_to_str(config.setting.original_scene_type) << " "
         << (huge ? "旗帜波" : "普通波") << " " << (assume_activate ? "激活" : "分离") << " ";
    if (dance_cheat == zombie_dance_cheat::none) {
        file << "无dance";
    } else if (dance_cheat == zombie_dance_cheat::fast) {
        file << "dance快";
    } else if (dance_cheat == zombie_dance_cheat::slow) {
        file << "dance慢";
    } else {
        assert(false && "unreachable");
    }
    file << " " << (natural ? "自然出怪" : "均匀出怪") << "\n";
    file << "必出类型: " << zombie_types_to_names(required_types, "") << "\n";
    file << "禁出类型: " << zombie_types_to_names(banned_types, "") << "\n";

    for (size_t i = 0; i < max_header_count; i++) {
        for (const auto& header : headers) {
            file << ",";
            if (i < header.size()) {
                file << header[i];
            }
            file << ",,";
        }
        file << "\n";
    }

    file << std::fixed << std::setprecision(3);
    for (const auto& col : table.cols) {
        file << "平均意外率,";
        file << 100.0 * col.average_accident_rate << "%,,";
    }
    file << "\n";

    auto all_zombie_types_names = all_zombie_types_to_names();
    for (size_t i = 0; i < table.cols.size(); i++) {
        file << all_zombie_types_names << ",比照,,";
    }
    file << "\n";

    for (size_t i = 0; i < table.max_row_count; i++) {
        for (const auto& col : table.cols) {
            if (i < col.rows.size()) {
                file << zombie_types_to_names(col.rows[i].first) << ","
                     << 100.0 * col.rows[i].second << "%,";
            } else {
                file << ",,";
            }
            file << ",";
        }
        file << "\n";
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程." << std::endl;

    return 0;
}