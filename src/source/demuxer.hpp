/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef DEMUXER_HPP
#define DEMUXER_HPP

#include <memory>
#include <system_error>

namespace lxstreamer {
struct source_data;

class demuxer
{
public:
    explicit demuxer(source_data&);
    ~demuxer();

    /// starts the decoding loop (blocking)
    std::error_code run();

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // DEMUXER_HPP
