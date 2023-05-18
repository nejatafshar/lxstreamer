/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef RECORDER_HPP
#define RECORDER_HPP

#include <memory>
#include <string>
#include <system_error>

struct AVPacket;
struct source_data;

namespace lxstreamer {

struct source_data;

class recorder
{
public:
    explicit recorder();

    ~recorder();

    std::error_code init(source_data*);
    void            start();

    int write_packet(const AVPacket*);

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // RECORDER_HPP
