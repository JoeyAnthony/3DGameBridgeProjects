//
// Created by BramTeurlings on 22-8-2024.
//

#pragma once

#include <iostream>
#include <regex>

class PathGetter {
public:
    PathGetter() = default;

    std::string getSRBinPath() {
        std::regex path_regex("^Path");
        std::string path_environment_variable = std::getenv("PATH");
        //std::cout << path_environment_variable << "\n";

        std::regex path_search_regex("[a-zA-Z0-9+_\\-\\.:%()\\s\\\\]+");
        std::smatch match_results;
        std::regex_search(path_environment_variable, match_results, path_search_regex);

        auto words_begin = std::sregex_iterator(path_environment_variable.begin(), path_environment_variable.end(), path_search_regex);
        auto words_end = std::sregex_iterator();

        std::string simulated_reality_bin_path;
        for (std::sregex_iterator i = words_begin; i != words_end; ++i)
        {
            std::smatch match = *i;
            std::string match_str = match.str();
            std::cout << match_str << '\n';
            // Todo: Change this match based on if we're running 32 bit or 64 bit.
            if(match_str.find("Program Files\\Simulated Reality") != std::string::npos)
            {
                simulated_reality_bin_path = match_str;
                break;
            }
        }

        return simulated_reality_bin_path;
    }
};
