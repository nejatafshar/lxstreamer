/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "source_data.hpp"
#include "utils.hpp"

#include <filesystem>

namespace lxstreamer {

std::string
preferred_video_framework() {
#if defined(__linux__)
    return "video4linux2";
#elif defined(__APPLE__) || defined(__FreeBSD__)
    return "avfoundation";
#elif defined(_WIN32)
    return "dshow";
#endif
}

int
source_data::load_input() {
    auto* ctx = avformat_alloc_context();
    if (ctx == nullptr)
        return AVERROR(ENOMEM);
    input_ctx.reset(ctx);

    // check webcam
    auto str = iargs.url;
    if (str.rfind("avdevice:", 0) == 0) {
        std::string delimiter = ":";
        size_t      pos       = 0;
        int         index     = 0;
        std::string video_framework;
        std::string device_name;
        while ((pos = str.find(delimiter)) != std::string::npos) {
            auto token = str.substr(0, pos);
            if (index == 1)
                video_framework = token;
            str.erase(0, pos + delimiter.length());
            ++index;
            if (index == 2)
                break;
        }
        if (video_framework.empty())
            video_framework = preferred_video_framework();
        device_name = str;
        if (!video_framework.empty()) {
            input_format = av_find_input_format(video_framework.data());
            if (!input_format) {
                logError(
                    "webcam unknown format: src: %s format: %s",
                    iargs.name,
                    video_framework);
            }
            str       = device_name;
            is_webcam = true;
        } else
            str = iargs.url;
    }
    iargs.url = str;

    dictionary      options;
    std::error_code ec;
    demux_data.is_local = std::filesystem::is_regular_file(iargs.url, ec);
    if (input_format) {
        logTrace("webcam detected: src: %s", iargs.name);
    } else if (demux_data.is_local) {
        logTrace("local file detected: src: %s", iargs.name);
    } else {
        options.set("threads", 1, 0);
        if (to_lower(iargs.url).rfind("rtsp:", 0) == 0)
            options.set("rtsp_flags", "prefer_tcp", 0);
    }

    demux_data.inter_handler.set_context(ctx);

    const auto ret = avformat_open_input(
        &ctx, iargs.url.data(), input_format, options.ref());
    if (ret == 0) {
        // set load parameters
        // ctx->probesize = probe_size;
        // ctx->max_analyze_duration = analyze_msec * 1000;
        ctx->flags |= AVFMT_FLAG_GENPTS | AVFMT_FLAG_FLUSH_PACKETS;
    } else {
        input_ctx.release();
    }
    return ret;
}

int
source_data::find_info() {
    AVFormatContext* ctx = input_ctx.get();
    avformat_find_stream_info(ctx, nullptr);

    auto fill_stream_info = [&](stream_data& s, int idx) {
        s.stream_idx = idx;
        s.stream     = ctx->streams[idx];
        if (s.stream->time_base.den && s.stream->r_frame_rate.den) {
            const auto d =
                av_q2d(s.stream->r_frame_rate) * av_q2d(s.stream->time_base);
            if (d != 0.0)
                s.duration = static_cast<int64_t>(1.0 / d);
        }
    };


    if (auto ret =
            av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        ret >= 0)
        fill_stream_info(demux_data.video_stream, ret);

    if (auto ret =
            av_find_best_stream(ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        ret >= 0)
        fill_stream_info(demux_data.audio_stream, ret);

    if (demux_data.video_stream.stream_idx < 0 &&
        demux_data.audio_stream.stream_idx < 0)
        return demux_data.video_stream.stream_idx;
    else
        return 0;
}

bool
source_data::seek_to(int64_t time) {
    auto duration         = input_ctx->duration;
    auto duration_seconds = av_rescale_q(duration, {1, AV_TIME_BASE}, {1, 1});
    auto pos =
        std::max(double(time), 0.0) / std::max(int64_t{1}, duration_seconds);
    auto p    = int64_t(pos * duration);
    int  flag = AVSEEK_FLAG_BACKWARD; // seeks to nearest I-frame
    int  ret  = av_seek_frame(input_ctx.get(), -1, p, flag);
    demux_data.local_file.seeked = true;
    return ret >= 0;
}

double
source_data::calculate_progress(AVPacket* pkt) {
    const AVStream* stream = nullptr;
    if (pkt->stream_index == demux_data.video_stream.stream_idx)
        stream = demux_data.video_stream.stream;
    else if (pkt->stream_index == demux_data.audio_stream.stream_idx)
        stream = demux_data.audio_stream.stream;
    if (!stream)
        return -1;
    auto elapsed = av_rescale_q(pkt->pts, stream->time_base, {1, AV_TIME_BASE});
    auto duration = input_ctx->duration;
    return double(elapsed) / std::max(int64_t{1}, duration);
}


} // namespace lxstreamer
