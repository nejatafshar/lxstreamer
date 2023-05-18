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

enum class log_level_t {
    trace   = 0,
    info    = 1,
    warning = 2,
    error   = 3,
    fatal   = 4,
    off     = 5,
};

enum class container_t {
    matroska = 1,
    mpegts   = 2,
    flv      = 3,
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

enum class file_format_t {
    mp4     = 1,
    ts      = 2,
    mkv     = 3,
    avi     = 4,
    flv     = 5,
    mov     = 6,
    webm    = 7,
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

// arguments for source to be added
struct source_args_t {
    std::string name;           ///< a unique name for source
    std::string url;            ///< source url
    std::string auth_session;   ///< a string to be provided in uri query by
                                ///< <session> field for stream authentication
    encoding_t  video_encoding; ///< optional video encoding for streaming
    encoding_t  audio_encoding; ///< optional audio encoding for streaming
    container_t container{
        container_t::unknown}; ///< preferred container format, automatically
                               ///< chosen if not defined
};

// options for source recording
struct record_options_t {
    std::string   path; ///< record output dir path or file path
    file_format_t format{
        file_format_t::unknown};   ///< preferred file format, automatically
                                   ///< chosen if not defined
    encoding_t video_encoding;     ///< optional video encoding for recording
    encoding_t audio_encoding;     ///< optional audio encoding for recording
    size_t     file_size{1024};    ///< chunk file size in mega bytes
    size_t     file_duration{0};   ///< chunk file duration in sec
    size_t     write_interval{5};  ///< interval for writing to file in seconds
    bool       record_audio{true}; ///< if audio should be recorded
};

} // namespace lxstreamer

#endif // COMMON_TYPES_HPP
