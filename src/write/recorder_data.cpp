/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#include "recorder_data.hpp"
#include "error_types.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace lxstreamer {

inline std::unordered_map<file_format_t, std::string> FormatNames = {
    {file_format_t::mp4, "mp4"},
    {file_format_t::ts, "ts"},
    {file_format_t::mkv, "mkv"},
    {file_format_t::avi, "avi"},
    {file_format_t::flv, "flv"},
    {file_format_t::mov, "mov"},
    {file_format_t::webm, "webm"},
    {file_format_t::unknown, ""},
};

std::string
to_string(const file_format_t& f) {
    return FormatNames.at(f);
}

std::string
now_string() {
    auto               t  = std::time(nullptr);
    auto               tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

namespace fs = std::filesystem;

recorder_data::recorder_data() : writer_base(writer_type::record) {}

recorder_data::~recorder_data() {
    finalize();
    close();
}

bool
recorder_data::init_record() {
    if (!check_space_limit())
        return false;
    if (!setup_path())
        return false;
    if (!setup_output())
        return false;

    duration_time.start();
    buffer_write_time.start();
    written_bytes     = 0;
    written_duration  = 0;
    written_packets   = 0;
    first_packet_time = -1;
    initialized       = true;

    return true;
}

bool
recorder_data::check_space_limit() {
    const auto& path = sd->record_options.path;
    if (path.empty())
        return true;
    int64_t space = -1;
    try {
        space = fs::space(path).available;
    } catch (const std::exception&) {
        std::error_code e;
        if (!fs::exists(path, e)) {
            fs::create_directories(path, e);
            space = fs::space(path, e).available;
        }
    }
    if (space < MB) {
        logError("recorder: low space for recording src: %s ", sd->iargs.name);
        return false;
    }
    return true;
}

bool
recorder_data::setup_path() {
    std::string file_name;

    const auto&     rp = sd->record_options.path;
    std::error_code ec;
    if (fs::is_regular_file(rp)) {
        rec_path  = rp;
        file_name = fs::path{rp}.filename().string();
    } else {
        auto dir = rp;
        if (!fs::is_directory(rp))
            dir = (fs::path{current_app_path()}.parent_path() /
                   std::string{"records"} / sd->iargs.name)
                      .string();
        file_name = format_string(
            "%s-%s.%s",
            sd->iargs.name,
            now_string(),
            to_string(sd->record_options.format));
        rec_path = (fs::path{dir} / file_name).string();
    }

    const auto& rec_dir = fs::path{rec_path}.parent_path();
    if (fs::exists(rec_dir, ec))
        return true;
    fs::create_directories(rec_dir, ec);
    if (!fs::exists(rec_dir, ec)) {
        logFatal(
            "recorder: failed to create output path: %s src: %s",
            rec_path,
            sd->iargs.name);
        return false;
    }
    return true;
}

void
recorder_data::close() {
    if (!output)
        return;
    if (output->pb && !(output->oformat->flags & AVFMT_NOFILE)) {
        avio_close(output->pb);
        output->pb = nullptr;
    }
    logTrace("recorder: closed file: %s", rec_path);
    output.reset();
}

bool
recorder_data::try_setup_output() {
    if (!sd)
        return false;
    AVFormatContext* octx{nullptr};
    int              ret = avformat_alloc_output_context2(
        &octx, nullptr, nullptr, rec_path.data());
    if (ret < 0 || !octx) {
        logFatal(
            "recorder: failed to alloc output context: src: %s err:%d, %s",
            sd->iargs.name,
            ret,
            ffmpeg_make_error_string(ret));
        return false;
    }

    output.reset(octx);

    if (!(output->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&octx->pb, rec_path.data(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            logFatal(
                "recorder: failed to create file: src: %s err:%d, %s",
                sd->iargs.name,
                ret,
                ffmpeg_make_error_string(ret));
            return false;
        }
    }

    auto& conf       = sd->record_encoding;
    conf.audio.codec = codec_t::unknown;
    if (sd->record_options.record_audio &&
        fs::path{rec_path}.extension() == "ts")
        if (auto codec = alternate_proper_audio_codec();
            codec != codec_t::unknown)
            conf.audio.codec = codec;

    if (is_valid(conf.video))
        if (sd->iencoder.initialize(conf.video, octx) != 0)
            return false;
    if (is_valid(conf.audio))
        if (sd->iencoder.initialize(conf.audio, octx) != 0)
            return false;

    if (!make_output_streams())
        return false;

    av_dict_set(&octx->metadata, "Streamer", "lxstreamer", 0);
    av_dict_set(
        &octx->metadata,
        "Copyright",
        "(C) 2022-present Nejat Afshar <nejatafshar@gmail.com>",
        0);
    av_dict_set(&octx->metadata, "Source", sd->iargs.name.c_str(), 0);

    ret = avformat_write_header(octx, nullptr);
    if (ret < 0) {
        logWarn(
            "recorder: failed to write header: src: %s path: %s err:%d, %s",
            sd->iargs.name,
            rec_path,
            ret,
            ffmpeg_make_error_string(ret));
        return false;
    }

    return true;
}

bool
recorder_data::setup_output() {
    if (!sd)
        return false;

    std::list<file_format_t> formats{
        file_format_t::mkv,
        file_format_t::ts,
        file_format_t::mp4,
        file_format_t::avi,
        file_format_t::mov,
        file_format_t::flv,
        file_format_t::webm};

    if (sd->record_options.format == file_format_t::unknown)
        sd->record_options.format = formats.front();

    // first try preferred format, if fails try others in order
    formats.erase(
        std::find(formats.begin(), formats.end(), sd->record_options.format));
    formats.emplace_front(sd->record_options.format);

    for (const auto& f : formats) {
        const auto& extension = to_string(f);
        rec_path = fs::path{rec_path}.replace_extension(extension).string();
        if (try_setup_output()) {
            file_name = fs::path{rec_path}.filename().string();
            return true;
        } else {
            close();
            std::error_code e;
            fs::remove(rec_path, e);
        }
    }

    return false;
}

void
recorder_data::finalize() {
    if (output)
        av_write_trailer(output.get());
}

} // namespace lxstreamer
