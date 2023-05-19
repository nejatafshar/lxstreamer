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

    // add webcam device name in this format: avdevice::video=webcamName
    // the video framework for webcam is auto detected, if desired you can
    // define a preferred video framework like:
    // avdevice:video4linux2:video=webcamName
    streamer.add_source({"webcam1", "avdevice::video=USB2.0_Camera"});

    // optionally add an other webcam source with defined encoding for streams,
    // if encoding is not defined as in the previous source, a proper one is
    // selected automatically
    /**
    lxstreamer::source_args_t args;
    args.name                              = "webcam2";
    args.url                               = "avdevice::video=USB2.0_Camera_2";
    args.video_encoding_view.codec         = lxstreamer::codec_t::h264;
    args.video_encoding_view.max_bandwidth = 500; // kb/s
    streamer.add_source(args);
    **/

    // start streaming sources
    streamer.start();

    // streams are available with these urls:
    //   "http://127.0.0.1:8000/stream?source=webcam1"
    //   "http://127.0.0.1:8000/stream?source=webcam2"

    while (gSignalStatus == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

    return 0;
}
