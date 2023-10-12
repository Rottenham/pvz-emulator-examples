#pragma once

#include <cstdio>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "types.h"

namespace _SmashInternal {

void read_cards(
    const rapidjson::GenericArray<true, rapidjson::Value> card_vals, std::vector<Card>& cards)
{
    cards.reserve(card_vals.Size());
    for (const auto& card_val : card_vals) {
        std::string card = card_val.GetString();
        if (card == "Normal") {
            cards.push_back(Card::Normal);
        } else if (card == "Puff") {
            cards.push_back(Card::Puff);
        } else if (card == "Pot") {
            cards.push_back(Card::Pot);
        } else {
            assert(false && "unreachable");
        }
    }
}

void read_fodder_positions(const rapidjson::GenericArray<true, rapidjson::Value> pos_vals,
    std::vector<FodderPos>& positions)
{
    positions.reserve(pos_vals.Size());
    for (const auto& pos_val : pos_vals) {
        FodderPos pos;
        pos.row = pos_val["row"].GetInt();
        pos.col = pos_val["col"].GetInt();
        positions.push_back(pos);
    }
}

void read_action(Config& config, const rapidjson::Value& val, Action& action)
{
    std::string op = val["op"].GetString();

    if (op == "Cob") {
        Cob cob;
        const auto positions = val["positions"].GetArray();
        cob.positions.reserve(positions.Size());

        cob.symbol = val["symbol"].GetString();
        cob.time = val["time"].GetInt();
        for (const auto& pos_val : positions) {
            CobPos pos;
            pos.row = pos_val["row"].GetInt();
            pos.col = pos_val["col"].GetFloat();
            cob.positions.push_back(pos);
        }
        action = cob;

        config.setting.op_count += positions.Size();
    } else if (op == "FixedFodder") {
        FixedFodder fodder;

        fodder.symbol = val["symbol"].GetString();
        fodder.time = val["time"].GetInt();
        auto shovel_time_val = val.FindMember("shovelTime");
        if (shovel_time_val != val.MemberEnd()) {
            fodder.shovel_time = shovel_time_val->value.GetInt();
        }
        read_cards(val["cards"].GetArray(), fodder.cards);
        read_fodder_positions(val["positions"].GetArray(), fodder.positions);
        action = fodder;

        config.setting.op_count += shovel_time_val != val.MemberEnd() ? 2 : 1;
    } else if (op == "SmartFodder") {
        SmartFodder fodder;
        const auto waves = val["waves"].GetArray();
        fodder.waves.reserve(waves.Size());

        fodder.symbol = val["symbol"].GetString();
        fodder.time = val["time"].GetInt();
        auto shovel_time_val = val.FindMember("shovelTime");
        if (shovel_time_val != val.MemberEnd()) {
            fodder.shovel_time = shovel_time_val->value.GetInt();
        }
        read_cards(val["cards"].GetArray(), fodder.cards);
        read_fodder_positions(val["positions"].GetArray(), fodder.positions);
        fodder.choose = val["choose"].GetInt();
        for (const auto& wave_val : waves) {
            fodder.waves.insert(wave_val.GetInt());
        }
        action = fodder;

        config.setting.op_count += shovel_time_val != val.MemberEnd() ? 2 : 1;
    } else {
        assert(false && "unreachable");
    }
}

void read_wave(Config& config, const rapidjson::Value& val, Wave& wave)
{
    const auto ice_times = val["iceTimes"].GetArray();
    const auto actions = val["actions"].GetArray();
    wave.ice_times.reserve(ice_times.Size());
    wave.actions.reserve(actions.Size());

    for (const auto& ice_time : ice_times) {
        wave.ice_times.push_back(ice_time.GetInt());
    }
    wave.wave_length = val["waveLength"].GetInt();
    for (const auto& action_val : actions) {
        Action action;
        read_action(config, action_val, action);
        wave.actions.push_back(action);
    }

    config.setting.op_count += 1 + ice_times.Size();
    config.setting.action_count += actions.Size();
}

void read_setting(const rapidjson::Value& val, Setting& setting)
{
    for (auto it = val.MemberBegin(); it != val.MemberEnd(); it++) {
        if (strcmp(it->name.GetString(), "scene") == 0) {
            std::string scene = it->value.GetString();
            if (scene == "NE") {
                setting.scene_type = pvz_emulator::object::scene_type::night;
            } else if (scene == "FE") {
                setting.scene_type = pvz_emulator::object::scene_type::fog;
            } else if (scene == "ME") {
                setting.scene_type = pvz_emulator::object::scene_type::moon_night;
            } else {
                assert(false && "unreachable");
            }
        } else if (strcmp(it->name.GetString(), "protect") == 0) {
            auto protect_positions = it->value.GetArray();
            setting.protect_positions.reserve(protect_positions.Size());

            for (const auto& protect_position : protect_positions) {
                std::string type = protect_position["type"].GetString();
                setting.protect_positions.push_back(
                    {type == "Cob" ? Setting::ProtectPos::Type::Cob
                                   : Setting::ProtectPos::Type::Normal,
                        protect_position["row"].GetInt(), protect_position["col"].GetInt()});
            }
        } else {
            assert(false && "unreachable");
        }
    }
}

void read_config(const rapidjson::Value& val, Config& config)
{
    config = {};
    config.waves.reserve(val.MemberCount());

    bool contains_smart_fodder = false;
    for (auto it = val.MemberBegin(); it != val.MemberEnd(); it++) {
        if (strcmp(it->name.GetString(), "setting") == 0) {
            read_setting(it->value, config.setting);
        } else {
            assert(std::atoi(it->name.GetString()) == config.waves.size() + 1);
            Wave wave;
            read_wave(config, it->value, wave);
            config.waves.push_back(wave);

            if (!contains_smart_fodder) {
                for (const auto& action : wave.actions) {
                    if (std::holds_alternative<SmartFodder>(action)) {
                        contains_smart_fodder = true;
                        break;
                    }
                }
            }
        }
    }

    if (contains_smart_fodder) {
        config.setting.garg_total = 5 * config.waves.size();
    }
}

} // namespace _SmashInternal

Config read_json(const std::string& filename)
{
    ::system("chcp 65001 > nul");

    FILE* fp = std::fopen(filename.c_str(), "rb");

    if (!fp) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        exit(1);
    }

    char read_buffer[65536];
    rapidjson::FileReadStream is(fp, read_buffer, sizeof(read_buffer));

    rapidjson::Document doc;
    doc.ParseStream(is);
    fclose(fp);

    if (doc.HasParseError()) {
        std::cerr << "不是有效的 JSON 格式: " << filename << std::endl;
        exit(1);
    }

    Config config;
    _SmashInternal::read_config(doc, config);

    return config;
}