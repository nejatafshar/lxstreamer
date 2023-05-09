/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "scaler.hpp"
#include "../source_data.hpp"

struct scale_config {
    int           src_w{0};
    int           src_h{0};
    AVPixelFormat src_pixel_fmt{AV_PIX_FMT_NONE};
    int           dest_w{0};
    int           dest_h{0};
    AVPixelFormat dest_pixel_fmt{AV_PIX_FMT_NONE};

    bool operator==(const scale_config& o) const {
        return src_w == o.src_w && src_h == o.src_h && dest_w == o.dest_w &&
               dest_h == o.dest_h && src_pixel_fmt == o.src_pixel_fmt &&
               dest_pixel_fmt == o.dest_pixel_fmt;
    }
};


// implement hashing for scale_config type
namespace std {

template <> struct hash<scale_config> {
    auto operator()(const scale_config& h) const {
        return std::hash<int>{}(h.src_w) ^ std::hash<int>{}(h.src_h) ^
               std::hash<AVPixelFormat>{}(h.src_pixel_fmt) ^
               std::hash<int>{}(h.dest_w) ^ std::hash<int>{}(h.dest_h) ^
               std::hash<AVPixelFormat>{}(h.dest_pixel_fmt);
    }
};

} // namespace std

namespace lxstreamer {

struct scaler::impl {

    const source_data&                            super;
    std::unordered_map<scale_config, SwsContext*> scales;

    explicit impl(const source_data& sup) : super{sup} {}

    ~impl() {}

    int initialize_scale(const scale_config& config) {
        if (auto it = scales.find(config); it != scales.cend())
            return 0;

        SwsContext* context;
        context = sws_getContext(
            config.src_w,
            config.src_h,
            config.src_pixel_fmt,
            config.dest_w,
            config.dest_h,
            config.dest_pixel_fmt,
            SWS_FAST_BILINEAR,
            nullptr,
            nullptr,
            nullptr);
        if (!context) {
            return AVERROR_INVALIDDATA;
        }

        scales[config] = context;

        return 0;
    }
};

scaler::scaler(const source_data& s) : pimpl{std::make_unique<impl>(s)} {}

scaler::~scaler() {}

int
scaler::perform_scale(
    const AVFrame* frm, int width, int height, frame& result) {
    if (height % 2 == 1) // check even
        --height;
    auto dest_w =
        width == -1 ? calc_width(frm->width, frm->height, height) : width;
    auto dest_format =
        pimpl->super.is_webcam ? AV_PIX_FMT_YUV420P : frm->format;
    scale_config config{
        frm->width,
        frm->height,
        AVPixelFormat(frm->format),
        dest_w,
        height,
        AVPixelFormat(dest_format)};
    if (auto it = pimpl->scales.find(config); it == pimpl->scales.cend())
        if (auto ret = pimpl->initialize_scale(config); ret != 0)
            return ret;

    auto r = result.get();

    r->width  = dest_w;
    r->height = config.dest_h;
    r->format = config.dest_pixel_fmt;
    auto ret  = av_frame_get_buffer(r, 0);
    if (ret < 0) {
        return AVERROR_INVALIDDATA;
    }

    av_frame_copy_props(r, frm);
    r->width  = dest_w;
    r->height = config.dest_h;

    ret = sws_scale(
        pimpl->scales[config],
        frm->data,
        frm->linesize,
        0,
        config.src_h,
        r->data,
        r->linesize);

    if (ret == 0) {
        return AVERROR_INVALIDDATA;
    }

    r->pts       = frm->pts;
    r->pkt_dts   = frm->pkt_dts;
    r->flags     = frm->flags;
    r->duration  = frm->duration;
    r->pkt_pos   = frm->pkt_pos;
    r->time_base = frm->time_base;
    r->pict_type = frm->pict_type;
    r->format    = config.dest_pixel_fmt;
    r->key_frame = frm->key_frame;

    return 0;
}

} // namespace lxstreamer
