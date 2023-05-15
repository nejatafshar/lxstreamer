/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef SOURCE_HPP
#define SOURCE_HPP

#include "common_types.hpp"

#include <memory>
#include <system_error>

namespace lxstreamer {

class viewer;
struct streamer_data;

struct source final {
    explicit source(const streamer_data&, const source_args_t&);
    ~source();

    std::error_code start();

    const source_args_t& args() const;

    /// returns if it's recording
    bool is_recording() const;

    /// starts recording to path
    std::error_code start_recording(std::string path);
    /// stops recording
    std::error_code stop_recording();

    /// seeks to pos for file inputs
    std::error_code seek(int64_t time);
    /// sets playback speed for file inputs
    std::error_code set_speed(double speed);

    /// adds a client for streaming
    std::error_code add_viewer(std::unique_ptr<viewer> v);

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // SOURCE_HPP
