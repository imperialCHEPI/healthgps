#ifndef HEALTHGPS_CONSOLE_HPP_INCLUDED
#define HEALTHGPS_CONSOLE_HPP_INCLUDED

#include <iostream>
#include <chrono>
#include <ctime>

std::string getTimeNowStr() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string s(30, '\0');

    struct tm localtime;

    localtime_s(&localtime, &now);

    std::strftime(&s[0], s.size(), "%c", &localtime);

    return s;
}

#endif //HEALTHGPS_CONSOLE_HPP_INCLUDED