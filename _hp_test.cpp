/* 获得每波僵尸血量.
 */

#include "common/test.h"
#include "seml/refresh/lib.h"
#include "world.h"

#include <map>
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

std::mutex mtx;
std::vector<std::vector<int>> results; // 3, 6, 12, 15
std::map<int, int> wave_to_idx = {{3, 0}, {6, 1}, {12, 2}, {15, 3}};

void test_one(const Config& config, int repeat, const ZombieTypes& required_types,
    const ZombieTypes& banned_types)
{
    std::mt19937 rng(
        static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()));
    world w(config.setting.scene_type);
    std::vector<std::vector<int>> local_results;
    local_results.resize(wave_to_idx.size());

    for (int round_idx = 0; round_idx < repeat; round_idx++) {
        auto spawn_types = get_spawn_types(
            rng, config.setting.original_scene_type, required_types, banned_types);
        int giga_limit = 50;
        for (int i = 1; i <= 15; i++) {
            auto spawn_list = get_spawn_list(rng, spawn_types, false, true, giga_limit);
            for (const auto& s : spawn_list) {
                if (s == zombie_type::giga_gargantuar) {
                    giga_limit--;
                }
            }
            if (wave_to_idx.find(i) != wave_to_idx.end()) {
                w.scene.spawn.wave = 0;
                for (const auto& type : spawn_list) {
                    w.zombie_factory.create(type);
                }
                w.scene.spawn.wave++; // required for get_current_hp() to work correctly
                auto init_hp = static_cast<int>(w.spawn.get_current_hp());
                local_results[wave_to_idx[i]].push_back(init_hp);

                w.scene.reset();
                w.scene.stop_spawn = true;
            }
        }
    }

    std::lock_guard<std::mutex> guard(mtx);
    for (size_t i = 0; i < wave_to_idx.size(); i++) {
        for (const auto& l : local_results[i]) {
            results[i].push_back(l);
        }
    }
}

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();

    ::system("chcp 65001 > nul");

    results.resize(wave_to_idx.size());
    std::vector<std::string> args(argv, argv + argc);
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "hp_test");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "1000"));
    auto required_types = parse_zombie_types(get_cmd_arg(args, "req", ""));
    auto banned_types = parse_zombie_types(get_cmd_arg(args, "ban", ""));

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);

    std::vector<std::thread> threads;
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back([config, repeat, required_types, banned_types]() {
            test_one(config, repeat, required_types, banned_types);
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "测试环境: " << scene_type_to_str(config.setting.original_scene_type) << " ";
    file << "\n";
    file << "必出类型: " << zombie_types_to_names(required_types, "") << "\n";
    file << "禁出类型: " << zombie_types_to_names(banned_types, "") << "\n";
    for (const auto& [wave, idx] : wave_to_idx) {
        file << wave << "\n";
        std::sort(results[idx].begin(), results[idx].end());
        for (const auto& r : results[idx]) {
            file << r << ",";
        }
        file << "\n\n";
    }

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程." << std::endl;

    return 0;
}