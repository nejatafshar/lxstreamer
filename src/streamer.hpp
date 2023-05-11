/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef STREAMER_HPP
#define STREAMER_HPP

#include "common_types.hpp"

#include <list>
#include <memory>
#include <string>
#include <system_error>

namespace lxstreamer {

class streamer
{

public:
    /// constructs a streamer to listen on <port> with an option for http/s
    streamer(int port, bool https = false);
    ~streamer();

    /// starts streamer
    void start();

    /// sets pathes for SSL certificate and key files
    void set_ssl_cert_path(std::string cert, std::string key);

    /// adds a source with a args to be streamed
    std::error_code add_source(const source_args_t& args);

    /// removes source <name>
    std::error_code remove_source(std::string name);

    /// returns source names
    std::list<std::string> sources() const;

    /// starts recording source <name> to path
    std::error_code start_recording(std::string name, std::string path);

    /// stops recording source <name>
    std::error_code stop_recording(std::string name);

    /// seeks source <name> to pos if it's a file
    std::error_code seek(std::string name, int64_t time);

    /// sets playback speed for source <name> if it's a file
    std::error_code set_speed(std::string name, double speed);

private:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // STREAMER_HPP
