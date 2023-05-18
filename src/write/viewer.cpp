/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "viewer.hpp"
#include "ffmpeg_types.hpp"
#include "viewer_data.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <system_error>
#include <thread>

namespace lxstreamer {

constexpr const int max_pkt_count = 256;

struct viewer::impl : public viewer_data {
    std::queue<packet_ref, std::list<packet_ref>> queue;
    std::atomic_bool                              running{false};
    std::thread                                   worker;
    std::mutex                                    mutex;
    std::condition_variable                       cv;

public:
    explicit impl(const uri_data_t& ud, mg_connection* mc)
        : viewer_data{ud, mc} {}

    ~impl() {
        running = false;
        cv.notify_all();
        if (worker.joinable()) {
            try {
                worker.join();
            } catch (std::system_error& e) {
                logWarn(
                    "viewer failed to join: src: %s addr: %s err: %d, %s",
                    sd->iargs.name,
                    address,
                    e.code().value(),
                    e.what());
            }
        }
    }

    void start_worker() {
        worker = std::thread{[this]() {
            if (setup_output()) {
                while (running.load()) {
                    std::unique_lock<std::mutex> lock(mutex);
                    cv.wait(lock, [&] {
                        return !running.load() || !queue.empty();
                    });
                    while (!queue.empty()) {
                        const auto pkt = queue.front().get();
                        if (write_packet(pkt) < 0) {
                            running = false;
                            break;
                        }
                        queue.pop();
                    }
                }
                finalize();
            }
            running = false;
        }};
    }
};

viewer::viewer(const uri_data_t& ud, mg_connection* mc)
    : pimpl{std::make_unique<impl>(ud, mc)} {}

viewer::~viewer() {}

std::error_code
viewer::init(source_data* s) {
    if (!s)
        return make_err(error_t::invalid_argument);
    pimpl->sd = s;
    return pimpl->init_io();
}

void
viewer::start() {
    pimpl->running = true;
    pimpl->start_worker();
}

int
viewer::write_packet(const AVPacket* pkt) {
    if (!pimpl->running.load(std::memory_order_relaxed))
        return AVERROR_EOF;
    {
        std::scoped_lock lock{pimpl->mutex};
        if (pimpl->queue.size() < max_pkt_count)
            pimpl->queue.emplace(pkt);
    }
    pimpl->cv.notify_all();
    return 0;
}

uri_data_t&
viewer::uri_data() {
    return pimpl->uri_data;
}

//-----------------------------------------------------------------------------
} // namespace lxstreamer
//-----------------------------------------------------------------------------
