/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef SCALER_HPP
#define SCALER_HPP

#include "ffmpeg_types.hpp"

#include <list>
#include <memory>

namespace lxstreamer {
struct source_data;

/// scales video frames
class scaler final
{
public:
    explicit scaler(const source_data&);
    ~scaler();

    int perform_scale(const AVFrame* frm, int width, int height, frame& result);

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // SCALER_HPP
