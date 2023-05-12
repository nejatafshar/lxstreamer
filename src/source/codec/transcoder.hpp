/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef TRANSCODER_HPP
#define TRANSCODER_HPP

#include "ffmpeg_types.hpp"

#include <list>
#include <memory>

namespace lxstreamer {
struct source_data;
struct encoding_t;

/// transcodes a packet or encodes a frame based on general settings and
/// settings for each client
class transcoder final
{
public:
    explicit transcoder(
        source_data&, const AVPacket*, const AVFrame* = nullptr);
    ~transcoder();

    const std::list<packet_ref>& make_packets(const encoding_t& config);

    std::list<frame_ref>& frames() const;

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // TRANSCODER_HPP
