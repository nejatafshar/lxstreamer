/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "recorder.hpp"
#include "ffmpeg_types.hpp"
#include "recorder_data.hpp"

#include <atomic>
#include <mutex>
#include <queue>
#include <system_error>
#include <thread>

namespace lxstreamer {

constexpr const int max_pkt_count = 256;

struct recorder::impl : public recorder_data {
    std::queue<packet_ref, std::list<packet_ref>> queue;
    std::atomic_bool                              running{false};
    std::thread                                   worker;
    std::mutex                                    mutex;
    std::condition_variable                       cv;
    std::queue<packet_ref, std::list<packet_ref>> rec_buffer;

public:
    explicit impl() : recorder_data{} {}

    ~impl() {
        running = false;
        cv.notify_all();
        if (worker.joinable()) {
            try {
                worker.join();
            } catch (std::system_error& e) {
                logWarn(
                    "recorder failed to join: src: %s err: %d, %s",
                    sd->iargs.name,
                    e.code().value(),
                    e.what());
            }
        }
    }

    void start_worker();
    void run();
    void write_buffer();
    bool check_limits(int packet_size, int64_t packet_time);
    void set_packet_times(AVPacket* pkt);
    int  packet_size(const AVPacket* pkt);
    void finalize_record();
};

void
recorder::impl::start_worker() {
    worker = std::thread{[this]() { run(); }};
}

void
recorder::impl::run() {
    while (running.load()) {
        if (!output) {
            if (!init_record())
                break;
        }

        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return !running.load() || !queue.empty(); });
        while (!queue.empty()) {
            const auto pkt = queue.front().get();

            size_t passed = buffer_write_time.seconds();

            if (pkt->pts == AV_NOPTS_VALUE)
                set_packet_times(const_cast<AVPacket*>(pkt));
            if (sd->record_options.write_interval > 0) {
                rec_buffer.emplace(pkt);
                if (passed >= sd->record_options.write_interval)
                    write_buffer();
            } else {
                if (write_packet(pkt) < 0) {
                    running = false;
                    break;
                } else if (passed >= 5)
                    buffer_write_time.start();
            }
            if (!check_limits(packet_size(pkt), -1))
                finalize_record();

            queue.pop();
        }
    }
    finalize();
    running = false;
}

void
recorder::impl::write_buffer() {
    while (!rec_buffer.empty()) {
        const auto pkt = rec_buffer.front().get();
        if (write_packet(pkt) < 0) {
            running = false;
            break;
        }
        rec_buffer.pop();
    }
    buffer_write_time.start();
}

bool
recorder::impl::check_limits(int packet_size, int64_t packet_time) {
    written_bytes += packet_size;
    if (first_packet_time == -1 || first_packet_time > packet_time) {
        first_packet_time = packet_time;
    }
    uint64_t real_elapsed = duration_time.seconds();
    uint64_t duration     = packet_time - first_packet_time;
    if (packet_time == -1)
        duration = real_elapsed;
    if ((duration - written_duration) > 30) // system sleeped
        return false;
    written_duration = duration;
    auto& c          = sd->record_options;
    if ((c.file_size > 0 && written_bytes >= c.file_size * MB) ||
        (c.file_duration > 0 && duration > c.file_duration))
        return false;
    // finish on low space
    if (real_elapsed % 10 == 0 && !check_space_limit())
        return false;
    return true;
}

void
recorder::impl::set_packet_times(AVPacket* pkt) {
    auto in_idx  = pkt->stream_index;
    auto out_idx = out_stream_map[in_idx];
    if (out_idx == -1)
        return;
    pkt->pts = av_rescale_q(
        duration_time.seconds(),
        {1, 1000000000},
        output->streams[out_idx]->time_base);
    pkt->dts      = AV_NOPTS_VALUE;
    pkt->duration = 0;
}

int
recorder::impl::packet_size(const AVPacket* pkt) {
    auto size = pkt->size;
    for (int i = 0; i < pkt->side_data_elems; ++i)
        size += static_cast<int>(pkt->side_data[i].size);
    return size;
}

void
recorder::impl::finalize_record() {
    write_buffer();
    finalize();
    close();
}

recorder::recorder() : pimpl{std::make_unique<impl>()} {}

recorder::~recorder() {}

std::error_code
recorder::init(source_data* s) {
    if (!s)
        return make_err(error_t::invalid_argument);
    pimpl->sd = s;
    return std::error_code{};
}

void
recorder::start() {
    pimpl->running = true;
    pimpl->start_worker();
}

int
recorder::write_packet(const AVPacket* pkt) {
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

//-----------------------------------------------------------------------------
} // namespace lxstreamer
//-----------------------------------------------------------------------------
