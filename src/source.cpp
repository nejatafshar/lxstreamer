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

    impl(const source_args_t& args) : source_data(args) {
        start_worker();
    }
    ~impl() {
        finished.store(true);
        if (worker.joinable()) {
            try {
                worker.join();
            } catch (std::system_error&) {
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
    if (!finished.load() && !worker.joinable()) {
        worker = std::thread{[this]() {
            while (!finished.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds{2000});
                if (running || recording) {
                    try {
                        run();
                    } catch (std::system_error&) {
                    } catch (std::exception&) {
                    }
                }
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

    idemuxer.reset();
}

void
source::impl::on_open() {
    if (is_video(iargs.video_encoding) || is_webcam) {
        view_encoding.video.codec         = iargs.video_encoding.codec;
        view_encoding.video.height        = iargs.video_encoding.height;
        view_encoding.video.max_bandwidth = iargs.video_encoding.max_bandwidth;
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
}

void
source::impl::on_packet(const AVPacket* pkt) {}

void
source::impl::start_recording() {}

source::source(const source_args_t& args)
    : pimpl{std::make_unique<impl>(args)} {}

source::~source() {}

const source_args_t&
source::args() const {
    return pimpl->iargs;
}

bool
source::is_recording() const {
    return pimpl->recording.load();
}

std::error_code
source::start_recording(std::string path) {
    if (pimpl->recording)
        return make_err(error_t::already_done);
    pimpl->record_path = path;
    pimpl->recording   = true;
    pimpl->running     = true;
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
source::add_client() {
    return std::error_code{};
}

} // namespace lxstreamer
