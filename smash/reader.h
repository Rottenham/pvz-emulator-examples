#pragma once

#include <cstdio>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "types.h"

namespace _SmashInternal {

void read_fodder_pos(const rapidjson::Value& val, FodderPos& pos)
{
    std::string type = val["type"].GetString();
    pos.type = (type == "Puff") ? FodderPos::Type::Puff : FodderPos::Type::Normal;
    pos.row = val["row"].GetInt();
    pos.col = val["col"].GetInt();
}

void read_action(const rapidjson::Value& val, Action& action)
{
    std::string op = val["op"].GetString();

    if (op == "Cob") {
        Cob cob;
        const auto positions = val["positions"].GetArray();
        cob.positions.reserve(positions.Size());

        cob.time = val["time"].GetInt();
        for (const auto& pos_val : positions) {
            Cob::CobPos pos;
            pos.row = pos_val["row"].GetInt();
            pos.col = pos_val["col"].GetFloat();
            cob.positions.push_back(pos);
        }
        action = cob;
    } else if (op == "FixedFodder") {
        FixedFodder fodder;
        const auto positions = val["positions"].GetArray();
        fodder.positions.reserve(positions.Size());

        fodder.time = val["time"].GetInt();
        if (val.HasMember("shovelTime")) {
            fodder.shovel_time = val["shovelTime"].GetInt();
        }
        for (const auto& pos_val : positions) {
            FodderPos pos;
            read_fodder_pos(pos_val, pos);
            fodder.positions.push_back(pos);
        }
        action = fodder;
    } else if (op == "SmartFodder") {
        SmartFodder fodder;
        const auto positions = val["positions"].GetArray();
        const auto waves = val["waves"].GetArray();
        fodder.positions.reserve(positions.Size());
        fodder.waves.reserve(waves.Size());

        fodder.time = val["time"].GetInt();
        if (val.HasMember("shovelTime")) {
            fodder.shovel_time = val["shovelTime"].GetInt();
        }
        for (const auto& pos_val : positions) {
            FodderPos pos;
            read_fodder_pos(pos_val, pos);
            fodder.positions.push_back(pos);
        }
        fodder.choose = val["choose"].GetInt();
        for (const auto& waveVal : waves) {
            fodder.waves.push_back(waveVal.GetInt());
        }
        action = fodder;
    } else {
        assert(false && "unreachable");
    }
}

void read_wave(const rapidjson::Value& val, Wave& wave)
{
    const auto ice_times = val["iceTimes"].GetArray();
    const auto actions = val["actions"].GetArray();
    wave.ice_times.reserve(ice_times.Size());
    wave.actions.reserve(actions.Size());

    for (const auto& iceTime : ice_times) {
        wave.ice_times.push_back(iceTime.GetInt());
    }
    wave.wave_length = val["waveLength"].GetInt();
    for (const auto& actionVal : actions) {
        Action action;
        read_action(actionVal, action);
        wave.actions.push_back(action);
    }
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

    for (auto it = val.MemberBegin(); it != val.MemberEnd(); it++) {
        if (strcmp(it->name.GetString(), "setting") == 0) {
            read_setting(it->value, config.setting);
        } else {
            assert(std::atoi(it->name.GetString()) == config.waves.size() + 1);
            Wave wave;
            read_wave(it->value, wave);
            config.waves.push_back(wave);
        }
    }
}

} // namespace _SmashInternal

Config read_json(const char* filename)
{
    ::system("chcp 65001 > nul");

    FILE* fp = std::fopen(filename, "rb");

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