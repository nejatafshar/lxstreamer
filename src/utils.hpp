/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef UTILS_HPP
#define UTILS_HPP

#include <algorithm>
#include <chrono>
#include <string>

namespace lxstreamer {

struct elapsed_timer {

    elapsed_timer() {
        start();
    }

    ~elapsed_timer() = default;

    void start() {
        m_time_point = std::chrono::steady_clock::now();
    }

    auto elapsed() const {
        return std::chrono::steady_clock::now() - m_time_point;
    }

    auto nanoseconds() const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed())
            .count();
    }

    auto microseconds() const {
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsed())
            .count();
    }

    auto milliseconds() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed())
            .count();
    }

    auto seconds() const {
        return std::chrono::duration_cast<std::chrono::seconds>(elapsed())
            .count();
    }

    auto restart() {
        auto el = elapsed();
        start();
        return el;
    }

protected:
    std::chrono::steady_clock::time_point m_time_point;
};

/// string utils
//-----------------------------------------------------------------------------
inline std::string
to_lower(std::string str) {
    std::string data = str;
    std::transform(data.begin(), data.end(), data.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return data;
}

template <typename... Args>
std::string
format_string(const std::string& format, Args&&... args) {
    using namespace std;
    auto size =
        snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
    string output(size, '\0');
    snprintf(&output[0], size, format.c_str(), std::forward<Args>(args)...);
    return output;
}

} // namespace lxstreamer

#endif // UTILS_HPP
