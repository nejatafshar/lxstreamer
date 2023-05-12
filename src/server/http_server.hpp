/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <memory>

namespace lxstreamer {
struct streamer_data;

class http_server
{
public:
    explicit http_server(streamer_data&);
    ~http_server();

    void start();

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // HTTP_SERVER_HPP
