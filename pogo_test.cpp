/* 测试收跳跳的最左炮准星.
 */

#include "common/io.h"
#include "common/pe.h"
#include "constants/constants.h"
#include "seml/reader/lib.h"
#include "world.h"

#include <mutex>
#include <optional>

using namespace pvz_emulator;
using namespace pvz_emulator::object;
using namespace pvz_emulator::system;

std::mutex mtx;

std::vector<int> upper_cob;
std::vector<int> same_cob;
std::vector<int> lower_cob;
std::optional<int> ice_time;
int hit_cob_col = -1;

bool is_range_overlap(std::pair<int, int> range1, std::pair<int, int> range2)
{
    return (range1.first <= range2.second && range1.second >= range2.first)
        || (range2.first <= range1.second && range2.second >= range1.first);
}

std::pair<int, int> get_col_range(const Setting::ProtectPos& protect_position)
{
    if (protect_position.type == Setting::ProtectPos::Type::Cob) {
        return {protect_position.col - 1, protect_position.col};
    } else {
        return {protect_position.col, protect_position.col};
    }
}

void get_hit_cob_col(const Wave& wave)
{
    for (const auto& action : wave.actions) {
        if (auto a = std::get_if<Cob>(&action)) {
            hit_cob_col = a->cob_col;
            return;
        }
    }
    std::cerr << "屋顶场合, 请提供炮尾所在列." << std::endl;
    exit(1);
}

void validate_config(Config& config)
{
    if (config.waves.empty()) {
        std::cerr << "请提供操作." << std::endl;
        exit(1);
    }

    if (config.waves.size() > 1) {
        std::cout << "警告: 共有 " << config.waves.size() << " 波操作, 但跳跳测试只考虑第 1 波."
                  << std::endl;
    }

    auto& wave = config.waves[0];

    if (is_roof(config.setting.scene_type)) {
        get_hit_cob_col(wave);
    }

    if (wave.start_tick == -1) {
        wave.start_tick = wave.wave_length - 200;
    }

    if (!wave.ice_times.empty()) {
        ice_time = wave.ice_times.front();
    }

    std::vector<Setting::ProtectPos> prev_protect_positions;
    for (const auto& protect_position : config.setting.protect_positions) {
        for (const auto& prev_position : prev_protect_positions) {
            if (is_range_overlap(get_col_range(protect_position), get_col_range(prev_position))) {
                std::cerr << "列重复: " << prev_position.col << ", " << protect_position.col
                          << std::endl;
                exit(1);
            }
        }
    }

    upper_cob.resize(wave.wave_length - wave.start_tick + 1, 999);
    same_cob.resize(wave.wave_length - wave.start_tick + 1, 999);
    lower_cob.resize(wave.wave_length - wave.start_tick + 1, 999);
}

void test(const Config& config, int repeat)
{
    const auto& wave = config.waves[0];

    world w(config.setting.scene_type);

    std::vector<int> local_upper_cob(wave.wave_length - wave.start_tick + 1, 999);
    std::vector<int> local_same_cob(wave.wave_length - wave.start_tick + 1, 999);
    std::vector<int> local_lower_cob(wave.wave_length - wave.start_tick + 1, 999);

    for (int r = 0; r < repeat; r++) {
        w.scene.reset();
        w.scene.stop_spawn = true;

        for (int tick = -100; tick <= wave.wave_length; tick++, run(w, 1)) {
            if (ice_time.has_value() && tick == *ice_time - 100) {
                w.plant_factory.create(plant_type::iceshroom, 0, 0);
            }

            if (tick == 0) {
                for (const auto& protect_position : config.setting.protect_positions) {
                    for (int cob_row = 1; cob_row <= 3; cob_row++) {
                        w.plant_factory.create(
                            plant_type::cob_cannon, cob_row - 1, protect_position.col - 2);
                    }
                }
                for (int i = 0; i < 1000; i++) {
                    w.zombie_factory.create(zombie_type::pogo, 1);
                }
            }

            if (tick >= wave.start_tick) {
                for (auto& z : w.scene.zombies) {
                    assert(z.type == zombie_type::pogo);

                    auto upper_cob_x_range = get_cob_hit_x_range(get_hit_box(z),
                        get_cob_hit_xy(w.scene.type, z.row, 9.0f, hit_cob_col).second);
                    auto same_cob_x_range = get_cob_hit_x_range(get_hit_box(z),
                        get_cob_hit_xy(w.scene.type, z.row + 1, 9.0f, hit_cob_col).second);
                    auto lower_cob_x_range = get_cob_hit_x_range(get_hit_box(z),
                        get_cob_hit_xy(w.scene.type, z.row + 2, 9.0f, hit_cob_col).second);

                    local_upper_cob[tick - wave.start_tick] = std::min(
                        local_upper_cob[tick - wave.start_tick], upper_cob_x_range.second);
                    local_same_cob[tick - wave.start_tick]
                        = std::min(local_same_cob[tick - wave.start_tick], same_cob_x_range.second);
                    local_lower_cob[tick - wave.start_tick] = std::min(
                        local_lower_cob[tick - wave.start_tick], lower_cob_x_range.second);
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    for (int tick = wave.start_tick; tick <= wave.wave_length; tick++) {
        upper_cob[tick - wave.start_tick]
            = std::min(upper_cob[tick - wave.start_tick], local_upper_cob[tick - wave.start_tick]);
        same_cob[tick - wave.start_tick]
            = std::min(same_cob[tick - wave.start_tick], local_same_cob[tick - wave.start_tick]);
        lower_cob[tick - wave.start_tick]
            = std::min(lower_cob[tick - wave.start_tick], local_lower_cob[tick - wave.start_tick]);
    }
}

int min_garg_walk(int tick)
{
    if (!ice_time.has_value()) {
        return tick;
    } else {
        int res = *ice_time - 1;
        int walk = std::max(tick - *ice_time - (600 - 2), 0);
        int uniced_walk = std::max(walk - (2000 - 600), 0);
        return static_cast<int>(res + (walk - uniced_walk) / 2.0 + uniced_walk);
    }
}

std::string bool_to_string(bool b) { return b ? "OK" : "ERROR"; }

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();
    ::system("chcp 65001 > nul");

    std::vector<std::string> args(argv, argv + argc);
    auto config_file = get_cmd_arg(args, "f");
    auto output_file = get_cmd_arg(args, "o", "pogo_test");
    auto total_repeat_num = std::stoi(get_cmd_arg(args, "r", "1000"));

    auto [file, full_output_file] = open_csv(output_file);

    auto config = read_json(config_file);
    validate_config(config);

    std::vector<std::thread> threads;
    for (int repeat : assign_repeat(total_repeat_num, std::thread::hardware_concurrency())) {
        threads.emplace_back([config, repeat]() { test(config, repeat); });
    }
    for (auto& t : threads) {
        t.join();
    }

    file << "时刻,收上行跳跳,收本行跳跳,收下行跳跳,收上行巨人,收本行巨人,收下行巨人,"
         << "上跳上巨,上跳本巨,上跳下巨,"
         << "本跳上巨,本跳本巨,本跳下巨,"
         << "下跳上巨,下跳本巨,下跳下巨,"
         << "\n";
         
    const auto& wave = config.waves[0];
    for (int tick = wave.start_tick; tick <= wave.wave_length; tick++) {
        auto upper_cob_pogo = upper_cob[tick - wave.start_tick];
        auto same_cob_pogo = same_cob[tick - wave.start_tick];
        auto lower_cob_pogo = lower_cob[tick - wave.start_tick];

        rect garg_hit_box;
        garg_hit_box.x = static_cast<int>(get_garg_x_max(min_garg_walk(tick))) - 17;
        garg_hit_box.y = (is_roof(config.setting.scene_type) ? 40 : 50)
            + (is_frontyard(config.setting.scene_type) ? 100 : 85) - 38;
        garg_hit_box.width = 125;
        garg_hit_box.height = 154;

        auto upper_cob_garg = get_cob_hit_x_range(
            garg_hit_box, get_cob_hit_xy(config.setting.scene_type, 1, 9.0f, hit_cob_col).second)
                                  .first;
        auto same_cob_garg = get_cob_hit_x_range(
            garg_hit_box, get_cob_hit_xy(config.setting.scene_type, 2, 9.0f, hit_cob_col).second)
                                 .first;
        auto lower_cob_garg = get_cob_hit_x_range(
            garg_hit_box, get_cob_hit_xy(config.setting.scene_type, 3, 9.0f, hit_cob_col).second)
                                  .first;

        file << tick << "," << lower_cob_pogo << "," << same_cob_pogo << "," << upper_cob_pogo
             << "," << lower_cob_garg << "," << same_cob_garg << "," << upper_cob_garg << ",";

        file << bool_to_string(lower_cob_garg <= lower_cob_pogo) << ",";
        file << bool_to_string(same_cob_garg <= lower_cob_pogo) << ",";
        file << bool_to_string(upper_cob_garg <= lower_cob_pogo) << ",";

        file << bool_to_string(lower_cob_garg <= same_cob_pogo) << ",";
        file << bool_to_string(same_cob_garg <= same_cob_pogo) << ",";
        file << bool_to_string(upper_cob_garg <= same_cob_pogo) << ",";

        file << bool_to_string(lower_cob_garg <= upper_cob_pogo) << ",";
        file << bool_to_string(same_cob_garg <= upper_cob_pogo) << ",";
        file << bool_to_string(upper_cob_garg <= upper_cob_pogo) << ",";

        file << "\n";
    }

    file.close();

    std::chrono::duration<double> elapsed = std::chrono::high_resolution_clock::now() - start;
    std::cout << "输出文件已保存至 " << full_output_file << ".\n"
              << "耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒, 使用了 "
              << threads.size() << " 个线程." << std::endl;
    return 0;
}