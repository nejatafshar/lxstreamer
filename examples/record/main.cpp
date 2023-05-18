/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "streamer.hpp"

#include <csignal>
#include <thread>

namespace {
volatile std::sig_atomic_t gSignalStatus;
}

void
signal_handler(int signal) {
    gSignalStatus = signal;
}

int
main(int argc, char* argv[]) {

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    lxstreamer::streamer::set_log_level(lxstreamer::log_level_t::trace);

    // an http server on port 8000
    lxstreamer::streamer streamer{8000};

    // add a source
    streamer.add_source({"src1", "rtsp://192.168.1.10/main"});

    // start recording src1 in chunks of 100 MB size, record path would be
    // alongside app exe as no path is defined. packets are buffered and wrriten
    // every 3 seconds to disk
    lxstreamer::record_options_t opt;
    opt.format         = lxstreamer::file_format_t::mkv;
    opt.file_size      = 100; // MB
    opt.write_interval = 3;
    streamer.start_recording("src1", opt);

    // start streaming and recording sources
    streamer.start();

    while (gSignalStatus == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
}
