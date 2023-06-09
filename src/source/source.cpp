/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "source.hpp"
#include "codec/transcoder.hpp"
#include "demuxer.hpp"
#include "error_types.hpp"
#include "source_data.hpp"
#include "utils.hpp"

#include <thread>

namespace lxstreamer {

struct source::impl : public source_data {
    std::thread              worker;
    std::unique_ptr<demuxer> idemuxer;
    elapsed_timer            run_elapsed_time;
    elapsed_timer            viewless_time;
    std::mutex               mutex;

    impl(const streamer_data& s, const source_args_t& args)
        : source_data(s, args) {}
    ~impl() {
        running.store(false);
        demux_data.inter_handler.running = false;
        if (worker.joinable()) {
            try {
                worker.join();
            } catch (std::system_error& e) {
                logWarn(
                    "source failed to join: src: %s addr: %s err: %d, %s",
                    iargs.name,
                    e.code().value(),
                    e.what());
            }
        }
    };

    std::error_code start_worker();
    void            run();
    void            on_open() override;
    void            on_packet(const AVPacket*) override;

    void start_recording();
};

std::error_code
source::impl::start_worker() {
    if (!running.load() && !worker.joinable()) {
        running.store(true);
        worker = std::thread{[this]() {
            while (running.load()) {
                if (demuxing || recording) {
                    try {
                        run();
                    } catch (std::system_error& e) {
                        logFatal(
                            "source system error: src: %s err: %d, %s",
                            iargs.name,
                            e.code().value(),
                            e.what());
                    } catch (std::exception& e) {
                        logFatal(
                            "source unknown error: src: %s err: %s",
                            iargs.name,
                            e.what());
                    }
                }
                if (running.load())
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds{2000});
            }
        }};
    }
    return std::error_code{};
}

void
source::impl::run() {
    idemuxer = std::make_unique<demuxer>(*this);

    run_elapsed_time.start();

    idemuxer->run();

    {
        std::scoped_lock lock{mutex};
        viewers.clear();
    }
    if (irecorder)
        irecorder.reset();
    idemuxer.reset();
    demux_data.reset();
}

void
source::impl::on_open() {
    if (is_video(iargs.video_encoding) || is_webcam) {
        auto codec       = iargs.video_encoding.codec;
        auto height      = iargs.video_encoding.height;
        auto max_bitrate = iargs.video_encoding.max_bitrate;
        if (codec == codec_t::unknown)
            codec = codec_t::h264;
        if (max_bitrate <= 0)
            max_bitrate = 2000;
        view_encoding.video.codec       = codec;
        view_encoding.video.height      = height;
        view_encoding.video.max_bitrate = max_bitrate;
        init_resolution(
            view_encoding.video,
            demux_data.video_stream.stream->codecpar->width,
            demux_data.video_stream.stream->codecpar->height);
    } else
        view_encoding.video.codec = codec_t::unknown;

    if (is_audio(iargs.audio_encoding)) {
        view_encoding.audio.codec       = iargs.audio_encoding.codec;
        view_encoding.audio.sample_rate = iargs.audio_encoding.sample_rate;
        view_encoding.audio.sample_fmt  = iargs.audio_encoding.sample_fmt;
        view_encoding.audio.channel_layout =
            iargs.audio_encoding.channel_layout;
    } else
        view_encoding.audio.codec = codec_t::unknown;

    std::scoped_lock lock{mutex};
    for (const auto& v : viewers)
        v->start();
}

void
source::impl::on_packet(const AVPacket* pkt) {
    auto is_video = pkt->stream_index == demux_data.video_stream.stream_idx;
    transcoder tc{*this, pkt};
    if (irecorder) {
        if (is_video || record_options.record_audio)
            for (const auto& p : tc.make_packets(
                     is_video ? record_encoding.video : record_encoding.audio))
                if (auto ret = irecorder->write_packet(p.get()); ret < 0)
                    irecorder.reset();
    }

    std::scoped_lock lock{mutex};
    for (auto iter = viewers.begin(); iter != viewers.end();) {
        const auto& packets = tc.make_packets(
            is_video ? view_encoding.video : view_encoding.audio);
        int nret = 0;
        for (const auto& p : packets) {
            nret = iter->get()->write_packet(p.get());
            if (nret < 0)
                break;
        }
        if (nret < 0) {
            iter = viewers.erase(iter);
        } else {
            ++iter;
        }
    }

    if (run_elapsed_time.seconds() > 5) {
        if (recording && !irecorder)
            start_recording();
        if (!recording && irecorder)
            irecorder.reset();

        if (viewers.empty()) {
            if (viewless_time.seconds() > 30 && !recording) {
                demuxing = false;
                logTrace(
                    "source stalled due to not having any viewer: src: %s",
                    iargs.name);
            }
        } else
            viewless_time.start();

        iencoder.prune();
        iresampler.prune();
        run_elapsed_time.start();
    }
}

void
source::impl::start_recording() {
    if (is_video(record_options.video_encoding) || is_webcam) {
        auto codec       = record_options.video_encoding.codec;
        auto height      = record_options.video_encoding.height;
        auto max_bitrate = record_options.video_encoding.max_bitrate;
        if (codec == codec_t::unknown)
            codec = codec_t::h264;
        if (max_bitrate <= 0)
            max_bitrate = 2000;
        record_encoding.video.codec       = codec;
        record_encoding.video.height      = height;
        record_encoding.video.max_bitrate = max_bitrate;
        init_resolution(
            record_encoding.video,
            demux_data.video_stream.stream->codecpar->width,
            demux_data.video_stream.stream->codecpar->height);
    } else
        record_encoding.video.codec = codec_t::unknown;

    irecorder = std::make_unique<recorder>();
    if (auto ec = irecorder->init(this); ec) {
        irecorder.reset();
        return;
    }
    irecorder->start();
}

source::source(const streamer_data& s, const source_args_t& args)
    : pimpl{std::make_unique<impl>(s, args)} {}

source::~source() {}

std::error_code
source::start() {
    return pimpl->start_worker();
}

const source_args_t&
source::args() const {
    return pimpl->iargs;
}

bool
source::is_recording() const {
    return pimpl->recording.load();
}

std::error_code
source::start_recording(const record_options_t& options) {
    if (pimpl->recording)
        return make_err(error_t::already_done);
    pimpl->record_options = options;
    pimpl->recording      = true;
    pimpl->demuxing       = true;
    return std::error_code{};
}

std::error_code
source::stop_recording() {
    if (!pimpl->recording)
        return make_err(error_t::already_done);
    pimpl->recording = false;
    return std::error_code{};
}

std::error_code
source::seek(int64_t time) {
    pimpl->demux_data.local_file.seek_time.store(time);
    return std::error_code{};
}

std::error_code
source::set_speed(double speed) {
    pimpl->demux_data.local_file.playback_speed.store(speed);
    return std::error_code{};
}

std::error_code
source::add_viewer(std::unique_ptr<viewer> v) {
    if (auto ec = v->init(pimpl.get()); ec)
        return ec;
    std::scoped_lock lock{pimpl->mutex};
    if (pimpl->demuxing && pimpl->demux_data.demuxer_initialized)
        v->start();
    pimpl->viewers.emplace_back(std::move(v));
    pimpl->demuxing = true;
    return std::error_code{};
}

} // namespace lxstreamer
