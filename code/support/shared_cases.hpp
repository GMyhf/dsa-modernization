#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace dsa::shared_cases {
struct Case {
    std::string name;
    std::string operation;
    std::string input;
    std::string expected;
    std::string expected_error;
};

inline std::vector<Case> load(const std::string& path = "cases.tsv") {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open " + path);
    std::string line;
    std::getline(input, line);
    if (line != "name\toperation\tinput\texpected\texpected_error") {
        throw std::runtime_error("bad shared-case header");
    }
    std::vector<Case> cases;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> fields;
        std::istringstream row(line);
        std::string field;
        while (std::getline(row, field, '\t')) fields.push_back(field);
        while (fields.size() < 5) fields.emplace_back();
        if (fields.size() != 5) throw std::runtime_error("bad shared-case row");
        cases.push_back({fields[0], fields[1], fields[2], fields[3], fields[4]});
    }
    return cases;
}

inline std::vector<int> integers(const std::string& text, char separator = ',') {
    std::vector<int> values;
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, separator)) {
        if (!token.empty()) values.push_back(std::stoi(token));
    }
    return values;
}

inline std::vector<std::string> strings(const std::string& text, char separator = ',') {
    std::vector<std::string> values;
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, separator)) values.push_back(token);
    return values;
}
}  // namespace dsa::shared_cases
