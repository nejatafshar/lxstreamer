/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef DEMUXER_DATA_HPP
#define DEMUXER_DATA_HPP

#include "interrupt_handler.hpp"
#include "utils.hpp"

namespace lxstreamer {

struct stream_data {
    AVStream* stream        = nullptr;
    int       stream_idx    = -1;
    int64_t   frames        = 0;
    int64_t   duration      = 0;
    int64_t   last_pts      = 0;
    int64_t   last_dts      = 0;
    int64_t   last_pts_diff = 0;
    int64_t   first_dts     = 0;
    int64_t   dts_offset    = 0;
    double    last_speed    = 1;

    void reset() {
        stream        = nullptr;
        stream_idx    = -1;
        frames        = 0;
        duration      = 0;
        last_pts      = 0;
        last_dts      = 0;
        last_pts_diff = 0;
        first_dts     = 0;
        dts_offset    = 0;
        last_speed    = 1;
    }
};

struct demuxer_data {

    bool should_wait_to_present() const {
        if (is_local)
            return (local_file.last_dts - local_file.seek_dts) >
                   local_file.elapsed.elapsed();
        return false;
    }
    bool should_present_faster() const {
        if (is_local)
            return (local_file.last_dts - local_file.seek_dts) <
                   local_file.elapsed.elapsed();
        return false;
    }

    bool on_packet(AVPacket* pkt) {
        inter_handler.on_packet();
        if (pkt->flags & AV_PKT_FLAG_CORRUPT)
            return false;
        if (pkt->stream_index == video_stream.stream_idx) {
            if (is_local)
                apply_speed(video_stream, pkt);
            analyze(video_stream, pkt);
            if (is_local)
                parse_local_file_packet(pkt, video_stream.stream);
        } else if (pkt->stream_index == audio_stream.stream_idx) {
            if (is_local)
                apply_speed(video_stream, pkt);
            analyze(audio_stream, pkt);
        } else
            return false; // not interested in this packet

        return true;
    }

    void reset() {
        is_local = false;
        video_stream.reset();
        audio_stream.reset();
    }

    interrupt_handler inter_handler;
    bool              is_local = false;
    stream_data       video_stream;
    stream_data       audio_stream;

    struct local_file_data {
        std::atomic<int64_t>      seek_time{-1};
        std::atomic<double>       playback_speed{1};
        std::chrono::microseconds last_dts{0};
        std::chrono::microseconds seek_dts{0};
        bool                      seeked{false};
        int64_t                   first_pkt_pos{0};
        elapsed_timer             elapsed;
    };
    local_file_data local_file;

private:
    void analyze(stream_data& sd, AVPacket* pkt) {
        ++sd.frames;
        if (pkt->pts == AV_NOPTS_VALUE && sd.duration > 0) {
            pkt->pts = pkt->dts = sd.frames * sd.duration;
            pkt->duration       = sd.duration;
        }
        sd.last_pts_diff = pkt->pts - sd.last_pts;
        sd.last_pts      = pkt->pts;
        sd.last_dts      = pkt->dts;
    }

    void apply_speed(stream_data& sd, AVPacket* pkt) {
        if (pkt->dts < 0)
            pkt->dts = pkt->pts;
        auto speed = local_file.playback_speed.load();
        if (sd.last_speed != speed) {
            sd.dts_offset = sd.last_dts;
            sd.first_dts  = pkt->dts;
            sd.last_speed = speed;
        }
        auto cts = pkt->pts - pkt->dts;
        pkt->dts =
            sd.dts_offset +
            static_cast<int64_t>((pkt->dts - sd.first_dts) * (1.0 / speed));
        if (pkt->dts != AV_NOPTS_VALUE && pkt->dts <= sd.last_dts)
            pkt->dts = sd.last_dts + 1;
        pkt->pts      = pkt->dts + cts;
        pkt->duration = 0;
    }

    void parse_local_file_packet(const AVPacket* pkt, const AVStream* stream) {
        if (local_file.first_pkt_pos <= 0 && pkt->pos > 0)
            local_file.first_pkt_pos = pkt->pos;
        // from time_base -> micro seconds
        local_file.last_dts = std::chrono::microseconds{av_rescale_q(
            pkt->dts != AV_NOPTS_VALUE ? pkt->dts : pkt->pts,
            stream->time_base,
            AVRational{1, 1000000})};
        if (local_file.seeked) {
            local_file.seek_dts = local_file.last_dts;
            local_file.seeked   = false;
        }
    }
};

} // namespace lxstreamer

#endif // DEMUXER_DATA_HPP
