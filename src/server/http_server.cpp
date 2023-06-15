/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "http_server.hpp"
#include "streamer_data.hpp"

#include <filesystem>
#include <thread>

namespace lxstreamer {

namespace {

enum class http_error_t {
    ok                    = 200,
    bad_request           = 400,
    unauthorized          = 401,
    forbidden             = 403,
    not_found             = 404,
    method_not_allowed    = 405,
    request_timeout       = 408,
    conflict              = 409,
    unsupported_media     = 415,
    internal_server_error = 500,
    not_implemented       = 501,
    bad_gateway           = 502,
    service_unavailable   = 503,
};

inline static std::unordered_map<error_t, http_error_t> ErrorMap = {
    {error_t::success, http_error_t::ok},
    {error_t::authentication_failed, http_error_t::unauthorized},
    {error_t::not_found, http_error_t::not_found},
    {error_t::not_ready, http_error_t::forbidden},
};

http_error_t
to_http_error(const std::error_code& ec) {
    if (const auto& err = ErrorMap.find(static_cast<error_t>(ec.value()));
        err != ErrorMap.cend())
        return err->second;
    return http_error_t::bad_request;
}

const constexpr int Init_Try_Max = 20;

} // namespace

struct http_server::impl {
    streamer_data&          super;
    std::unique_ptr<mg_mgr> mgr{nullptr};
    std::thread             worker;
    mg_connection*          listener       = nullptr;
    inline static bool      initialized    = false;
    int                     init_try_count = 0;

    explicit impl(streamer_data& s) : super{s} {}
    ~impl() {
        if (worker.joinable())
            worker.join();
        mg_mgr_free(mgr.get());
    }

    bool setup();
    void prepare_ssl_cert_pathes();
    void initServer();

    static void http_callback(mg_connection* mc, int ev, void* opaque) {
        if (ev == MG_EV_HTTP_REQUEST) {
            auto msg = reinterpret_cast<http_message*>(opaque);
            if (to_std_string(msg->method) != "GET") {
                mc->flags |= MG_F_SEND_AND_CLOSE;
                return;
            }
            auto* self = reinterpret_cast<impl*>(mc->listener->user_data);
            auto  uri  = to_std_string(msg->uri);
            if (uri == "/stream") {
                auto ec = self->super.make_stream(
                    mc,
                    to_std_string(msg->uri),
                    to_std_string(msg->query_string));
                if (ec) {
                    mg_http_send_error(
                        mc, static_cast<int>(to_http_error(ec)), nullptr);
                    mc->flags |= MG_F_SEND_AND_CLOSE;
                }
            } else {
                logWarn("http server: unknown api: %s", uri);
                mc->flags |= MG_F_SEND_AND_CLOSE;
                return;
            }
        } else if (ev == MG_EV_CLOSE) {
        }
    }

    static void connect_handler(mg_connection* nc, int ev, void*) {
        auto* self = reinterpret_cast<impl*>(nc->user_data);
        if (ev == MG_EV_SEND) {
            nc->flags |= MG_F_SEND_AND_CLOSE;
            initialized = true;
            logInfo("https server listening on port: %d", self->super.port);
        } else if (ev == MG_EV_CLOSE && !initialized)
            self->initServer();
    }
};

http_server::http_server(streamer_data& s) : pimpl{std::make_unique<impl>(s)} {}

http_server::~http_server() {}

bool
http_server::impl::setup() {
    mgr = std::make_unique<mg_mgr>();
    mg_mgr_init(mgr.get(), nullptr);

    const auto& address = format_string("tcp://0.0.0.0:%d", super.port);

    if (super.https) {
        prepare_ssl_cert_pathes();

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
            if (init_try_count == Init_Try_Max)
                logFatal(
                    "http server: failed to listen on: %s err: %s",
                    address,
                    err);
            return false;
        }
        mg_set_protocol_http_websocket(listener);
        listener->user_data = this;
    } else {
        listener = mg_bind(mgr.get(), address.data(), http_callback);
        if (listener == nullptr) {
            logFatal("http server: failed to listen on: %s", address);
            return false;
        }
        mg_set_protocol_http_websocket(listener);
        listener->user_data = this;
        logInfo("http server listening on port: %d", super.port);
    }

    return true;
}

void
http_server::impl::prepare_ssl_cert_pathes() {
    if (super.ssl_cert_path.empty())
        super.ssl_cert_path = "server.pem";
    if (super.ssl_key_path.empty())
        super.ssl_key_path = "server.key";
    namespace fs              = std::filesystem;
    const auto&     app_dir   = fs::path{current_app_path()}.parent_path();
    const auto&     cert_path = fs::path{super.ssl_cert_path};
    const auto&     key_path  = fs::path{super.ssl_key_path};
    std::error_code ec;
    if (fs::is_regular_file(cert_path, ec) && !fs::exists(cert_path, ec)) {
        super.ssl_cert_path = cert_path.filename().string();
        super.ssl_cert_path = (app_dir / super.ssl_cert_path).string();
    }
    if (fs::is_regular_file(key_path, ec) && !fs::exists(key_path, ec)) {
        super.ssl_key_path = key_path.filename().string();
        super.ssl_key_path = (app_dir / super.ssl_key_path).string();
    }
}

void
http_server::impl::initServer() {
    if (++init_try_count > Init_Try_Max)
        return;
    if (mgr) {
        mg_mgr_free(mgr.get());
        mgr.reset();
    }
    while (!setup()) {
        mg_mgr_free(mgr.get());
        mgr.reset();
    }

    if (!super.https)
        return;
    auto client = mg_connect_ws(
        mgr.get(),
        connect_handler,
        format_string("wss://127.0.0.1:%d", super.port).data(),
        "wss",
        nullptr);
    if (client)
        client->user_data = this;
}

void
http_server::start() {
    pimpl->worker = std::thread{[&]() {
        pimpl->initServer();
        while (pimpl->super.running) {
            mg_mgr_poll(pimpl->mgr.get(), 300);
        }
        logInfo("http server finished");
    }};
}

} // namespace lxstreamer
