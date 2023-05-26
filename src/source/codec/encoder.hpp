/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef ENCODER_HPP
#define ENCODER_HPP

#include "common_types.hpp"
#include "ffmpeg_types.hpp"

#include <list>
#include <memory>
#include <string>

namespace lxstreamer {
struct source_data;

/// returns even width for height to stay aspect ratio
int
calc_width(int src_w, int src_h, int dest_h);

/// returns height suitable for bitrate(in kbps)
int
height_for_bitrate(size_t bitrate);

/// sets width and height for encoding baded on parameters
void
init_resolution(encoding_t& enc, int input_width = 0, int input_height = 0);

/// returns a proper encoder for codec
const AVCodec*
get_encoder(codec_t codec);

struct encoder_config {
    encoding_t video;
    encoding_t audio;
};

/// encoder holding contextes with different settings
class encoder final
{
public:
    explicit encoder(const source_data&);
    ~encoder();

    AVCodecContext* context(const encoding_t& config) const;
    int initialize(const encoding_t& config, const AVFormatContext* octx);
    int encode_packets(
        const encoding_t&      config,
        const AVFrame*         frm,
        std::list<packet_ref>& packets);
    /// prunes unused encoders
    void prune();

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

// implement hashing for encoding_t type
namespace std {

template <> struct hash<lxstreamer::encoding_t> {
    auto operator()(const lxstreamer::encoding_t& h) const {
        return std::hash<lxstreamer::codec_t>{}(h.codec) ^
               std::hash<int>{}(h.height) ^ std::hash<size_t>{}(h.max_bitrate) ^
               std::hash<int64_t>{}(h.sample_rate) ^
               std::hash<string>{}(h.sample_fmt) ^
               std::hash<string>{}(h.channel_layout);
    }
};

} // namespace std

#endif // ENCODER_HPP
