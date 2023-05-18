/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef RECORDER_DATA_HPP
#define RECORDER_DATA_HPP

#include "writer_base.hpp"

namespace lxstreamer {

constexpr int64_t MB = 1024 * 1024;

struct recorder_data : public writer_base {
    std::string rec_path;
    std::string file_name;

    elapsed_timer         duration_time;
    elapsed_timer         buffer_write_time;
    std::atomic<uint64_t> written_packets{0};
    uint64_t              written_bytes{0};
    uint64_t              written_duration{0};
    int64_t               first_packet_time{-1};
    bool                  initialized{false};

    recorder_data();

    ~recorder_data();

    bool init_record();
    bool check_space_limit();
    bool setup_path();
    void close();
    bool try_setup_output();
    bool setup_output();
    void finalize();
};

} // namespace lxstreamer

#endif // RECORDER_DATA_HPP
