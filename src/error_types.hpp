/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef ERROR_TYPES_HPP
#define ERROR_TYPES_HPP

#include <system_error>
#include <unordered_map>

namespace lxstreamer {

enum class error_t {
    success               = 0,
    invalid_argument      = 1,
    already_done          = 2,
    already_exists        = 3,
    not_found             = 4,
    not_ready             = 5,
    not_supported         = 6,
    busy                  = 7,
    bad_state             = 8,
    timeout               = 9,
    stalled               = 10,
    authentication_failed = 11,
    unknown               = -1,
};

inline static std::unordered_map<error_t, std::string> ErrorMessages = {
    {error_t::success, "success"},
    {error_t::invalid_argument, "invalid argument"},
    {error_t::already_done, "already done"},
    {error_t::already_exists, "already exists"},
    {error_t::not_found, "not found"},
    {error_t::not_ready, "not ready"},
    {error_t::not_supported, "not supported"},
    {error_t::busy, "busy"},
    {error_t::bad_state, "bad state"},
    {error_t::timeout, "timed out"},
    {error_t::stalled, "stalled"},
    {error_t::authentication_failed, "authentication failed"},
    {error_t::unknown, "unknown"},
};

inline std::string
to_string(error_t err) {
    return ErrorMessages.at(err);
}

struct error_category : public std::error_category {
    ~error_category() override = default;

    const char* name() const noexcept override {
        return "lxstreamer";
    }

    std::string message(int err) const override {
        return to_string(static_cast<error_t>(err));
    }

    static const auto& instance() {
        static error_category c;
        return c;
    }
};

inline std::error_code
make_err(error_t err) {
    return std::make_error_code(static_cast<std::errc>(err));
}

} // namespace lxstreamer

#endif // ERROR_TYPES_HPP
