/****************************************************************************
** Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>
** Distributed under the MIT License (http://opensource.org/licenses/MIT)
**
** This file is part of lxstreamer.
** Light-weight http/s streamer.
****************************************************************************/

#ifndef INTERRUPT_HANDLER_HPP
#define INTERRUPT_HANDLER_HPP

#include "ffmpeg_types.hpp"
#include "utils.hpp"


namespace lxstreamer {

struct interrupt_handler {

    void set_context(AVFormatContext* ctx) {
        running                              = true;
        context                              = ctx;
        context->interrupt_callback.callback = &interrupt_handler::callback;
        context->interrupt_callback.opaque   = this;
        interrupt_count                      = 0;
    }

    void set_timeout(std::chrono::seconds seconds) noexcept {
        timeout = seconds;
    }

    void on_packet() {
        elapsed.start();
    }

    static int callback(void* opaque) noexcept {
        auto* self = reinterpret_cast<interrupt_handler*>(opaque);
        if (!self)
            return 1;
        ++(self->interrupt_count);
        if (!self->running.load(std::memory_order_relaxed))
            return 1;
        if (self->interrupt_count % 10 == 0)
            if (self->elapsed.elapsed() > self->timeout)
                return 1;
        return 0;
    }

    std::atomic_bool     running = true;
    AVFormatContext*     context = nullptr;
    elapsed_timer        elapsed;
    std::chrono::seconds timeout{20};
    int64_t              interrupt_count = 0;
};

} // namespace lxstreamer

#endif // INTERRUPT_HANDLER_HPP
