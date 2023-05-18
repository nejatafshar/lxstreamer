/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "demuxer.hpp"
#include "ffmpeg_types.hpp"
#include "source_data.hpp"

#include <thread>

namespace lxstreamer {

struct demuxer::impl {
    source_data& super;

    explicit impl(source_data& sd) : super{sd} {}

    ~impl() = default;

    int open_stream() {
        if (auto ret = super.load_input(); ret != 0) {
            logError(
                "failed to open stream: src: %s err: %d, %s",
                super.iargs.name,
                ret,
                ffmpeg_make_error_string(ret));
            return ret;
        }
        if (auto ret = super.find_info(); ret != 0) {
            logError(
                "failed to find info for streams: src: %s err: %d, %s",
                super.iargs.name,
                ret,
                ffmpeg_make_error_string(ret));
            return ret;
        }

        super.demux_data.demuxer_initialized = true;
        super.on_open();

        return 0;
    }

    int process_next_packet() {
        if (super.demux_data.should_wait_to_present())
            return AVERROR(EAGAIN);
        packet pkt;
        int    nret = av_read_frame(super.input_ctx.get(), pkt.get());
        if (nret == 0) { // got the packet
            if (super.demux_data.on_packet(pkt.get())) {
                super.on_packet(pkt.get());
            }
        } else if (nret == AVERROR(EAGAIN)) {
        } else if (nret == AVERROR_EOF && super.demux_data.is_local) {
            logInfo("local file reached to end: src: %s", super.iargs.name);
        } else if (nret < 0) {
            logError(
                "failed to read packet: src: %s err: %d, %s",
                super.iargs.name,
                nret,
                ffmpeg_make_error_string(nret));
        }
        return nret;
    }
};


demuxer::demuxer(source_data& sd) : pimpl{std::make_unique<impl>(sd)} {}

demuxer::~demuxer() = default;

std::error_code
demuxer::run() {

    auto ret = pimpl->open_stream();
    if (ret)
        return ffmpeg_make_err(ret);

    std::error_code result;
    auto&           d = *pimpl;

    while (d.super.demuxing.load(std::memory_order_relaxed)) {
        if (d.super.demux_data.is_local) {
            auto& st = d.super.demux_data.local_file.seek_time;
            if (auto time = st.load(std::memory_order_relaxed); time > -1) {
                d.super.seek_to(time);
                st.store(-1);
            }
        }
        auto       time_point = std::chrono::steady_clock::now();
        const auto nret       = d.process_next_packet();
        if (nret >= 0) {
            if (!d.super.demux_data.should_present_faster())
                std::this_thread::sleep_until(
                    time_point + std::chrono::milliseconds{2});
        } else if (nret == AVERROR(EAGAIN)) {
            std::this_thread::sleep_until(
                time_point + std::chrono::milliseconds{5});
        } else if (nret < 0) {
            result = ffmpeg_make_err(nret);
            break; // stop demuxing
        }
    }
    d.super.demux_data.inter_handler.running = false;
    logInfo(
        "finished demuxing: src: %s err: %d, %s",
        d.super.iargs.name,
        result.value(),
        result.message());
    return result;
}

} // namespace lxstreamer
