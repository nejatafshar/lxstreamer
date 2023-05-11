/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef SOURCE_DATA_HPP
#define SOURCE_DATA_HPP

#include "codec/decoder.hpp"
#include "codec/encoder.hpp"
#include "codec/resampler.hpp"
#include "codec/scaler.hpp"
#include "demuxer_data.hpp"
#include "ffmpeg_types.hpp"

#include <chrono>
#include <memory>
#include <mutex>
#include <system_error>
#include <unordered_map>

namespace lxstreamer {

struct source_data {
    source_args_t               iargs;
    std::atomic_bool            running{false}; // is false on class destruction
    std::atomic_bool            demuxing{false};
    std::atomic_bool            recording{false};
    std::string                 record_path;
    std::chrono::milliseconds   wait_interval{10000};
    unique_ptr<AVFormatContext> input_ctx;
    const AVInputFormat*        input_format{nullptr};
    demuxer_data                demux_data;
    bool                        is_webcam{false};
    decoder                     idecoder{*this};
    encoder                     iencoder{*this};
    scaler                      iscaler{*this};
    resampler                   iresampler{*this};
    encoder_config              view_encoding;
    encoder_config              record_encoding;


    explicit source_data(const source_args_t& args) : iargs{args} {}

    /// a callback that is called when source is opened
    virtual void on_open() = 0;
    /// a callback for every packet that is read by demuxer
    virtual void on_packet(const AVPacket* pkt) = 0;


    int load_input();

    int find_info();

    void check_webcam();

    bool seek_to(int64_t time);

    double calculate_progress(AVPacket* pkt);
};

} // namespace lxstreamer

#endif // SOURCE_DATA_HPP
