/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef DECODER_HPP
#define DECODER_HPP

#include "../common_types.hpp"
#include "../ffmpeg_types.hpp"

#include <list>
#include <memory>
#include <string>

namespace lxstreamer {
struct source_data;

/// decoder for video and audio streams
class decoder final
{
public:
    explicit decoder(const source_data&);
    ~decoder();

    AVCodecContext* video_context() const;
    AVCodecContext* audio_context() const;
    int             initialize(const AVStream*);
    int             decode_frames(const AVPacket*, std::list<frame_ref>&);

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // DECODER_HPP
