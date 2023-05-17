/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef VIEWER_HPP
#define VIEWER_HPP

#include <memory>
#include <string>
#include <system_error>

struct AVPacket;
struct mg_connection;
struct source_data;

namespace lxstreamer {

struct uri_data_t {
    std::string path;
    std::string query;
    std::string source_name;
    std::string session;
};

struct source_data;

class viewer
{
public:
    explicit viewer(const uri_data_t&, mg_connection*);

    ~viewer();

    std::error_code init(source_data*);
    void            start();

    int write_packet(const AVPacket*);

    uri_data_t& uri_data();

protected:
    struct impl;
    std::unique_ptr<impl> pimpl;
};

} // namespace lxstreamer

#endif // VIEWER_HPP
