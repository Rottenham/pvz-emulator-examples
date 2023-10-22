#pragma once

#include <cstdio>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "types.h"

namespace _SemlInternal {

void read_cards(
    const rapidjson::GenericArray<true, rapidjson::Value> card_vals, std::vector<Fodder>& cards)
{
    cards.reserve(card_vals.Size());
    for (const auto& card_val : card_vals) {
        std::string card = card_val.GetString();
        if (card == "Normal") {
            cards.push_back(Fodder::Normal);
        } else if (card == "Puff") {
            cards.push_back(Fodder::Puff);
        } else if (card == "Pot") {
            cards.push_back(Fodder::Pot);
        } else {
            assert(false && "unreachable");
        }
    }
}

void read_fodder_positions(
    const rapidjson::GenericArray<true, rapidjson::Value> position_vals, std::vector<CardPos>& positions)
{
    positions.reserve(position_vals.Size());
    for (const auto& position_val : position_vals) {
        CardPos card_pos;
        card_pos.row = position_val["row"].GetInt();
        card_pos.col = position_val["col"].GetInt();
        positions.push_back(card_pos);
    }
}

void read_action(const rapidjson::Value& val, Action& action)
{
    std::string op = val["op"].GetString();

    if (op == "Cob") {
        Cob cob;

        cob.symbol = val["symbol"].GetString();
        cob.time = val["time"].GetInt();

        const auto position_vals = val["positions"].GetArray();
        cob.positions.reserve(position_vals.Size());
        for (const auto& position_val : position_vals) {
            CobPos cob_pos;
            cob_pos.row = position_val["row"].GetInt();
            cob_pos.col = position_val["col"].GetFloat();
            cob.positions.push_back(cob_pos);
        }

        auto cob_col_val = val.FindMember("cobCol");
        if (cob_col_val != val.MemberEnd()) {
            cob.cob_col = cob_col_val->value.GetInt();
        }

        action = cob;
    } else if (op == "Jalapeno") {
        Jalapeno jalapeno;

        jalapeno.symbol = val["symbol"].GetString();
        jalapeno.time = val["time"].GetInt();
        jalapeno.position.row = val["position"]["row"].GetInt();
        jalapeno.position.col = val["position"]["col"].GetInt();

        action = jalapeno;
    } else if (op == "FixedFodder") {
        FixedFodder fodder;

        fodder.symbol = val["symbol"].GetString();
        fodder.time = val["time"].GetInt();
        auto shovel_time_val = val.FindMember("shovelTime");
        if (shovel_time_val != val.MemberEnd()) {
            fodder.shovel_time = shovel_time_val->value.GetInt();
        }
        read_cards(val["fodders"].GetArray(), fodder.fodders);
        read_fodder_positions(val["positions"].GetArray(), fodder.positions);

        action = fodder;
    } else if (op == "SmartFodder") {
        SmartFodder fodder;

        fodder.symbol = val["symbol"].GetString();
        fodder.time = val["time"].GetInt();
        auto shovel_time_val = val.FindMember("shovelTime");
        if (shovel_time_val != val.MemberEnd()) {
            fodder.shovel_time = shovel_time_val->value.GetInt();
        }
        read_cards(val["fodders"].GetArray(), fodder.fodders);
        read_fodder_positions(val["positions"].GetArray(), fodder.positions);
        fodder.choose = val["choose"].GetInt();

        const auto waves = val["waves"].GetArray();
        fodder.waves.reserve(waves.Size());
        for (const auto& wave_val : waves) {
            fodder.waves.insert(wave_val.GetInt());
        }

        action = fodder;
    } else {
        assert(false && "unreachable");
    }
}

void read_wave(const rapidjson::Value& val, Wave& wave)
{
    const auto ice_times = val["iceTimes"].GetArray();
    wave.ice_times.reserve(ice_times.Size());
    for (const auto& ice_time : ice_times) {
        wave.ice_times.push_back(ice_time.GetInt());
    }

    wave.wave_length = val["waveLength"].GetInt();

    const auto actions = val["actions"].GetArray();
    wave.actions.reserve(actions.Size());
    for (const auto& action_val : actions) {
        Action action;
        read_action(action_val, action);
        wave.actions.push_back(action);
    }

    auto start_tick_val = val.FindMember("startTick");
    if (start_tick_val != val.MemberEnd()) {
        wave.start_tick = start_tick_val->value.GetInt();
    }
}

void read_round(const rapidjson::Value& val, Round& round)
{
    const auto& waves_val = val.GetArray();
    round.reserve(waves_val.Size());

    for (const auto& wave_val : waves_val) {
        Wave wave;
        read_wave(wave_val, wave);
        round.push_back(wave);
    }
}

void read_rounds(const rapidjson::Value& val, std::vector<Round>& rounds)
{
    const auto& rounds_val = val.GetArray();
    rounds.reserve(rounds_val.Size());

    for (const auto& round_val : rounds_val) {
        Round round;
        read_round(round_val, round);
        rounds.push_back(round);
    }
}

void read_setting(const rapidjson::Value& val, Setting& setting)
{
    for (auto it = val.MemberBegin(); it != val.MemberEnd(); it++) {
        std::string key = it->name.GetString();
        if (key == "scene") {
            std::string scene = it->value.GetString();
            if (scene == "NE" || scene == "DE") {
                setting.scene_type = pvz_emulator::object::scene_type::night;
            } else if (scene == "FE" || scene == "PE") {
                setting.scene_type = pvz_emulator::object::scene_type::fog;
            } else if (scene == "ME" || scene == "RE") {
                setting.scene_type = pvz_emulator::object::scene_type::moon_night;
            } else {
                assert(false && "unreachable");
            }
        } else if (key == "protect") {
            auto protect_vals = it->value.GetArray();
            setting.protect_positions.reserve(protect_vals.Size());

            for (const auto& protect_val : protect_vals) {
                std::string type = protect_val["type"].GetString();
                int row = protect_val["row"].GetInt();
                int col = protect_val["col"].GetInt();

                if (type == "Cob") {
                    setting.protect_positions.push_back({Setting::ProtectPos::Type::Cob, row, col});
                } else if (type == "Normal") {
                    setting.protect_positions.push_back(
                        {Setting::ProtectPos::Type::Normal, row, col});
                } else {
                    assert(false && "unreachable");
                }
            }
        } else {
            assert(false && "unreachable");
        }
    }
}

void read_config(const rapidjson::Value& val, Config& config)
{
    config = {};

    for (auto it = val.MemberBegin(); it != val.MemberEnd(); it++) {
        std::string key = it->name.GetString();

        if (key == "setting") {
            read_setting(it->value, config.setting);
        } else if (key == "rounds") {
            read_rounds(it->value, config.rounds);
        } else {
            assert(false && "unreachable");
        }
    }
}

} // namespace _SemlInternal

Config read_json(const std::string& filename)
{
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
    _SemlInternal::read_config(doc, config);

    return config;
}
