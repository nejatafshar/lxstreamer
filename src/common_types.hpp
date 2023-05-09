/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef COMMON_TYPES_HPP
#define COMMON_TYPES_HPP

#include <string>

namespace lxstreamer {

enum class transport_format_t {
    mpegts   = 1,
    flv      = 2,
    matroska = 3,
    unknown  = -1,
};

enum class codec_t {
    // video codecs
    h264 = 1,
    hevc = 2,
    av1  = 3,
    vp9  = 4,
    // audio codecs
    ac3     = 100,
    mp2     = 101,
    mp3     = 102,
    aac     = 103,
    unknown = -1,
};

struct encoding_t {
    codec_t codec{codec_t::unknown};
    // video only
    int    width{0};
    int    height{0};
    size_t max_bandwidth{0};
    int    frame_rate{-1};
    // audio only
    int         sample_rate{-1};
    std::string sample_fmt;
    std::string channel_layout;

    bool operator==(const encoding_t& o) const {
        return codec == o.codec && codec == o.codec && width == o.width &&
               height == o.height && max_bandwidth == o.max_bandwidth &&
               frame_rate == o.frame_rate && sample_rate == o.sample_rate &&
               sample_fmt == o.sample_fmt && channel_layout == o.channel_layout;
    }
};

inline bool
is_valid(encoding_t enc) {
    return enc.codec != codec_t::unknown;
}

inline bool
is_video(encoding_t enc) {
    return is_valid(enc) && enc.codec < codec_t::ac3;
}

inline bool
is_audio(encoding_t enc) {
    return is_valid(enc) && enc.codec >= codec_t::ac3;
}

/// arguments for source to be added
struct source_args_t {
    std::string name;
    std::string url;
    encoding_t  video_encoding;
    encoding_t  audio_encoding;
};

} // namespace lxstreamer

#endif // COMMON_TYPES_HPP
