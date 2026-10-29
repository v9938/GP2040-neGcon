// GP2040-neGcon
// PSX neGon to USB Joystick Converter
//
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 @v9938

#pragma once
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace preset {
constexpr size_t max_bytes = 65536;
using Value = std::variant<std::string, double, bool>;
using Section = std::map<std::string, Value>;
using Document = std::map<std::string, Section>;
enum class Kind { String, Number, Boolean, Action, ReactiveAction };
struct Rule {
    Kind kind = Kind::Number;
    double min = 0, max = 4294967295.0;
    bool decimal = false;
    std::vector<Value> choices;
    Value initial = 0.0;
};
using Schema = std::map<std::string, std::map<std::string, Rule>>;
const std::vector<std::string>& inputs();
const std::vector<std::string>& actions();
Schema schema(const Document& document);
Document defaults();
Document parse(const std::string& text);
Value parse_value(const std::string& text);
std::string format_value(const Value& value);
std::string serialize(const Document& document);
void validate(const Document& document);
void validate_value(const Value& value, const Rule& rule, const std::string& path);
void set(Document& document, const std::string& section, const std::string& key, Value value);
bool valid_utf8(const std::string& text);
std::string trim(const std::string& text);
}
