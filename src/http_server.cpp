/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "http_server.hpp"
#include "mongoose.h"
#include "streamer_data.hpp"
#include "utils.hpp"

#include <filesystem>
#include <thread>

namespace lxstreamer {

struct http_server::impl {
    streamer_data&          super;
    std::unique_ptr<mg_mgr> mgr{nullptr};
    std::thread             worker;
    mg_connection*          listener = nullptr;

    explicit impl(streamer_data& s) : super{s} {
        mgr = std::make_unique<mg_mgr>();
        mg_mgr_init(mgr.get(), nullptr);
    }
    ~impl() {
        if (worker.joinable())
            worker.join();
        mg_mgr_free(mgr.get());
    }

    bool setup();

    static void http_callback(mg_connection* mc, int ev, void* opaque) {
        if (ev == MG_EV_HTTP_REQUEST) {
            auto msg = reinterpret_cast<http_message*>(opaque);
            if (std::string{msg->method.p, msg->method.len} != "GET") {
                mc->flags |= MG_F_SEND_AND_CLOSE;
                return;
            }
            auto* self = reinterpret_cast<impl*>(mc->listener->user_data);
        } else if (ev == MG_EV_CLOSE) {
        }
    }
};

http_server::http_server(streamer_data& s) : pimpl{std::make_unique<impl>(s)} {}

http_server::~http_server() {}

bool
http_server::impl::setup() {
    const auto& address = format_string("tcp://0.0.0.0:%d", super.port);

    if (super.https) {
        namespace fs = std::filesystem;
        if (super.ssl_cert_path.empty())
            super.ssl_cert_path = super.ssl_key_path =
                (fs::path{current_app_path()}.parent_path() / "server.pem")
                    .string();
        if (super.ssl_key_path.empty())
            super.ssl_key_path =
                (fs::path{current_app_path()}.parent_path() / "server.key")
                    .string();

        struct mg_bind_opts bind_opts;
        memset(&bind_opts, 0, sizeof(bind_opts));
        bind_opts.ssl_cert = std::filesystem::absolute(
                                 std::filesystem::path{super.ssl_cert_path})
                                 .string()
                                 .c_str();
        bind_opts.ssl_key =
            std::filesystem::absolute(std::filesystem::path{super.ssl_key_path})
                .string()
                .c_str();
        const char* err        = nullptr;
        bind_opts.error_string = &err;

        listener =
            mg_bind_opt(mgr.get(), address.data(), http_callback, bind_opts);
        if (listener == nullptr) {
            // failed to bind
            return false;
        }
        listener->user_data = this;
    } else {
        listener = mg_bind(mgr.get(), address.data(), http_callback);
        if (listener == nullptr) {
            // failed to bind
            return false;
        }
        listener->user_data = this;
    }

    return true;
}

void
http_server::start() {
    pimpl->worker = std::thread{[&]() {
        pimpl->setup();
        while (pimpl->super.running) {
            mg_mgr_poll(pimpl->mgr.get(), 300);
        }
    }};
}

} // namespace lxstreamer
