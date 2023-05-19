/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "streamer.hpp"

#include <csignal>
#include <fstream>
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

    // an http server on port 8000
    lxstreamer::streamer streamer{8000};

    // add a source
    streamer.add_source({"src1", "rtsp://192.168.1.10/main"});

    // create and open log files
    std::ofstream trace_log("log_trace.txt", std::ios_base::app);
    std::ofstream normal_log("log_normal.txt", std::ios_base::app);

    lxstreamer::streamer::set_log_level(lxstreamer::log_level_t::trace);
    streamer.set_log_to_stdout(false);
    streamer.set_log_callback(
        [&](std::string str, lxstreamer::log_level_t level) {
            if (level == lxstreamer::log_level_t::trace) {
                trace_log << str << std::endl;
                trace_log.flush();
            } else {
                normal_log << str << std::endl;
                normal_log.flush();
            }
        });

    // start streaming sources
    streamer.start();

    while (gSignalStatus == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

    trace_log.close();
    normal_log.close();

    return 0;
}
