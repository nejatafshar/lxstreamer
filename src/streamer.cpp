/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "streamer.hpp"
#include "error_types.hpp"
#include "ffmpeg_types.hpp"
#include "server/http_server.hpp"
#include "streamer_data.hpp"

namespace lxstreamer {

struct streamer::impl : public streamer_data {
    http_server server{*this};

    explicit impl(int port, bool https) : streamer_data{port, https} {
        avdevice_register_all();
    }

    ~impl() {
        running = false;
    }
};

streamer::streamer(int port, bool https)
    : pimpl{std::make_unique<impl>(port, https)} {
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
    pimpl->sources.insert({args.name, std::make_unique<source>(*pimpl, args)});
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
streamer::start_recording(std::string name, const record_options_t& options) {
    auto src = pimpl->get_source(name);
    if (!src)
        return make_err(error_t::not_found);
    return src->start_recording(options);
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

void
streamer::set_log_level(log_level_t level) {
    std::scoped_lock lock{log_mutex};
    log_level = level;
}

void
streamer::set_log_to_stdout(bool flag) {
    std::scoped_lock lock{log_mutex};
    log_to_stdout = flag;
}

void
streamer::set_log_callback(
    std::function<void(std::string, log_level_t)> callback) {
    std::scoped_lock lock{log_mutex};
    log_cb = std::move(callback);
}

streamer::~streamer() = default;

} // namespace lxstreamer
