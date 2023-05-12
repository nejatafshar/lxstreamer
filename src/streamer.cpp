/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "streamer.hpp"
#include "error_types.hpp"
#include "server/http_server.hpp"
#include "streamer_data.hpp"

namespace lxstreamer {

struct streamer::impl : public streamer_data {
    http_server server{*this};

    explicit impl() : streamer_data() {}

    ~impl() {
        running = false;
    }

    source* get_source(std::string name) {
        if (auto it = sources.find(name); it != sources.cend())
            return it->second.get();
        return nullptr;
    }
};

streamer::streamer(int port, bool https) {
    pimpl->port  = port;
    pimpl->https = https;
}

void
streamer::start() {
    if (pimpl->running)
        return;
    pimpl->running = true;
    pimpl->server.start();
    for (const auto& s : pimpl->sources)
        s.second->start();
}

void
streamer::set_ssl_cert_path(std::string cert, std::string key) {
    pimpl->ssl_cert_path = cert;
    pimpl->ssl_key_path  = key;
}

std::error_code
streamer::add_source(const source_args_t& args) {
    if (pimpl->get_source(args.name))
        return make_err(error_t::already_exists);
    pimpl->sources.insert({args.name, std::make_unique<source>(args)});
    if (pimpl->running)
        pimpl->get_source(args.name)->start();
    return std::error_code{};
}

std::error_code
streamer::remove_source(std::string name) {
    if (!pimpl->get_source(name))
        return make_err(error_t::not_found);
    pimpl->sources.erase(name);
    return std::error_code{};
}

std::list<std::string>
streamer::sources() const {
    std::list<std::string> list;
    for (const auto& l : pimpl->sources)
        list.emplace_back(l.first);
    return list;
}

std::error_code
streamer::start_recording(std::string name, std::string path) {
    auto src = pimpl->get_source(name);
    if (!src)
        return make_err(error_t::not_found);
    return src->start_recording(path);
}

std::error_code
streamer::stop_recording(std::string name) {
    auto src = pimpl->get_source(name);
    if (!src)
        return make_err(error_t::not_found);
    return src->stop_recording();
}

std::error_code
streamer::seek(std::string name, int64_t time) {
    auto src = pimpl->get_source(name);
    if (!src)
        return make_err(error_t::not_found);
    return src->seek(time);
}

std::error_code
streamer::set_speed(std::string name, double speed) {
    auto src = pimpl->get_source(name);
    if (!src)
        return make_err(error_t::not_found);
    return src->set_speed(speed);
}

streamer::~streamer() = default;

} // namespace lxstreamer
