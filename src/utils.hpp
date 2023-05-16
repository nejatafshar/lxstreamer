/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef UTILS_HPP
#define UTILS_HPP

#include "common_types.hpp"
#include "server/mongoose.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#endif

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
inline std::string
format_string(const std::string& format, Args&&... args) {
    using namespace std;
    auto size =
        snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...);
    string output(size, '\0');
    snprintf(&output[0], size, format.c_str(), std::forward<Args>(args)...);
    return output;
}

inline std::string
query_value(std::string query, std::string key) {
    size_t key_start = -1;
    if (auto k = query.find(key, 0); k != std::string::npos)
        key_start = k;
    else
        return std::string{};

    auto key_end      = key_start + key.size();
    auto equation_pos = query.find('=', key_end);
    if (equation_pos != key_end)
        return std::string{};
    auto value_end = query.find('&', equation_pos);
    if (value_end == std::string::npos)
        value_end = query.size() - 1;
    else
        --value_end;
    if (value_end > equation_pos) {
        auto value_start = equation_pos + 1;
        auto value_size  = value_end - equation_pos;
        return query.substr(value_start, value_size);
    }
    return std::string{};
}

inline std::string
to_std_string(const mg_str& str) {
    if (str.p && str.len > 0)
        return std::string{str.p, str.len};
    return std::string{};
}

/// logging
//-----------------------------------------------------------------

inline std::mutex                       log_mutex;
inline log_level_t                      log_level{log_level_t::info};
inline bool                             log_to_stdout = true;
inline std::function<void(std::string)> log_cb{nullptr};

template <log_level_t level, typename... Args>
inline void
log(const std::string& str, Args&&... args) {
    std::scoped_lock lock{log_mutex};
    if (level < log_level)
        return;
    if (log_to_stdout) {
        auto out = level >= log_level_t::error ? stderr : stdout;
        fprintf(out, str.c_str(), std::forward<Args>(args)...);
        fprintf(out, "\n");
    }
    if (log_cb)
        log_cb(format_string(str, std::forward<Args>(args)...));
}

template <typename... Args>
inline void
logTrace(const std::string& str, Args&&... args) {
    log<log_level_t::trace>(str, std::forward<Args>(args)...);
}
template <typename... Args>
inline void
logInfo(const std::string& str, Args&&... args) {
    log<log_level_t::info>(str, std::forward<Args>(args)...);
}
template <typename... Args>
inline void
logWarn(const std::string& str, Args&&... args) {
    log<log_level_t::warning>(str, std::forward<Args>(args)...);
}
template <typename... Args>
inline void
logError(const std::string& str, Args&&... args) {
    log<log_level_t::error>(str, std::forward<Args>(args)...);
}
template <typename... Args>
inline void
logFatal(const std::string& str, Args&&... args) {
    log<log_level_t::fatal>(str, std::forward<Args>(args)...);
}

/// other utils
//-----------------------------------------------------------------

inline std::string
current_app_path() {
#if defined(__linux__) || defined(__unix__) || defined(__MACH__)
#if defined(__linux__)
    const auto* name = program_invocation_name;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    const auto* name = getprogname();
#endif
    if (name) {
        std::error_code e;
        const auto&     path = std::filesystem::canonical(name, e);
        if (ec)
            return std::string{name};
        else
            return path.string();
    }
#elif defined(_WIN32)
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::string{buf};
#endif
    return {};
}

} // namespace lxstreamer

#endif // UTILS_HPP
