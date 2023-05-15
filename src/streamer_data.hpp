/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef STREAMER_DATA_HPP
#define STREAMER_DATA_HPP

#include "error_types.hpp"
#include "source/source.hpp"
#include "utils.hpp"
#include "write/viewer.hpp"

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

    explicit streamer_data(int port_, bool https_)
        : port{port_}, https{https_} {}

    source* get_source(std::string name) {
        if (auto it = sources.find(name); it != sources.cend())
            return it->second.get();
        return nullptr;
    }

    std::error_code
    make_stream(mg_connection* mc, std::string path, std::string query) {
        uri_data_t uri_data;
        uri_data.path        = path;
        uri_data.query       = query;
        uri_data.source_name = query_value(uri_data.query, "source");
        uri_data.session     = query_value(uri_data.query, "session");
        if (auto src = get_source(uri_data.source_name); src) {
            if (uri_data.session != src->args().auth_session)
                return make_err(error_t::authentication_failed);
            auto v = std::make_unique<viewer>(uri_data, mc);
            if (auto ec = v->init(); ec)
                return ec;
            src->add_viewer(std::move(v));
            return std::error_code{};
        }
        return make_err(error_t::not_found);
    }
};

} // namespace lxstreamer

#endif // STREAMER_DATA_HPP
