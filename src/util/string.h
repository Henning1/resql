#pragma once


#include <string>


static inline void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}


static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
}


std::string remove_extra_whitespaces ( const std::string &input ) {
    std::string output;
    unique_copy (input.begin(), input.end(), std::back_insert_iterator<std::string>(output),
                                     [](char a,char b){ return std::isspace(a) && std::isspace(b);});  
    return output;
}
