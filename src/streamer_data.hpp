/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef STREAMER_DATA_HPP
#define STREAMER_DATA_HPP

#include "source.hpp"

#include <memory>
#include <unordered_map>

namespace lxstreamer {

struct streamer_data {
    std::atomic_bool running{false};

    int         port  = 80;
    bool        https = true;
    std::string ssl_cert_path;
    std::string ssl_key_path;

    std::unordered_map<std::string, std::unique_ptr<source>> sources;

    explicit streamer_data() {}
};

} // namespace lxstreamer

#endif // STREAMER_DATA_HPP
