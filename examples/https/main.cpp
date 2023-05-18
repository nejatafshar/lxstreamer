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

    // an https server on port 8000
    lxstreamer::streamer streamer{8000, true};
    // set ssl cert and key file names in app dir (or full pathes)
    streamer.set_ssl_cert_path("server.pem", "server.key");
    // add a local file (modify path in the second arg)
    streamer.add_source({"src1", "path/to/local/video/file"});
    // optionally add an other source url
    /**
    streamer.add_source({"src2", "rtsp://192.168.1.10/main"});
    **/

    // start streaming sources
    streamer.start();

    // streams are available with these urls:
    //   "https://127.0.0.1:8000/stream?source=src1"
    //   "https://127.0.0.1:8000/stream?source=src2"

    while (gSignalStatus == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
}
