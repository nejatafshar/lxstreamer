/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef RESAMPLER_HPP
#define RESAMPLER_HPP

#include "ffmpeg_types.hpp"

#include <list>
#include <memory>

namespace lxstreamer {
struct source_data;

/// resamples audio frames
class resampler final
{
public:
    explicit resampler(const source_data&);
    ~resampler();

    std::list<frame> make_frames(
        const AVFrame* frm, AVCodecContext* in_ctx, AVCodecContext* out_ctx);

    /// prunes unused resamplers
    void prune();

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // RESAMPLER_HPP
