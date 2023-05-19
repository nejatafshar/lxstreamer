# lxstreamer

a light-weight, fast and cross-platform c++ library for http/s streaming and recording based on FFmpeg

## Features

* **streaming:**
  - video/audio streaming for multiple sources
  - **http** streaming
  - optional **https** streaming with `OpenSSL`as dependency
  - sources could be any live stream, camera, webcam, file and whatever `FFmpeg` supports
  - custom stream encoding for video and audio
  - stream authentication
  - preferred transport container selection
* **record:** 
  - record sources to `mp4`,`mkv`,... files
  - chunked record by size or duration
  - record to defined files without chunking
  - custom video and audio encoding
  - optional batch writes to disk
* **webcam:**
  - record
  - streaming with custom encoding
  - auto defined encoding config if not defined
* **local media files:**
  - stream, record and reencode
  - seek
  - speed change
* **cross-platform**: compiles for any platform with a `c++17` compiler
*  light-weight and fast with low overhead

## Dependencies

* **FFmpeg** 5.1.* or later
* **OpenSSL** 1.1.* for https

Run `depends.sh` to get depedent libs for your current OS. All dependencies are put into `3rdparty` directory.

`depends.sh` gets pre-compiled **FFmpeg** libraries that include the most commonly used decoders and encoders, optimized for a suitable build size. If you require additional encoders or decoders not included in these builds, you can provide your own compiled **FFmpeg** libraries.

## Build

Run in terminal:

```bash
mkdir build
cd build
cmake /path/to/lxstreamer
make
```

lxstreamer is built as a static lib. Examples are also built by default which could be disabled by trunning off CMake option `BUILD_EXAMPLES`.

`HTTPS` option should be `ON` for **https** capability with **OpenSSL** libs in `3rdparty` directory.

## Usage samples

A simple **http** streamer on port **8000** streaming a local video file:

```cpp
lxstreamer::streamer streamer{8000};
streamer.add_source({"src1", "path/to/local/video/file"});
streamer.start();
```

---

**https** streamer with multiple sources:

```c++
lxstreamer::streamer streamer{8000, true};
// set ssl cert and key file names in app dir (or full pathes)
streamer.set_ssl_cert_path("server.pem", "server.key");
streamer.add_source({"src1", "rtsp://192.168.1.10/main"});
streamer.add_source({"src2", "rtsp://192.168.1.49/main"});
...
streamer.start();
```

---

a streamer with a **webcam** as a source, streaming in **h264** codec with max **500 kb/s** bandwidth:

```c++
lxstreamer::streamer streamer{8000};

lxstreamer::source_args_t args;
args.name                              = "webcam1";
args.url                               = "avdevice::video=USB2.0_Camera";
args.video_encoding_view.codec         = lxstreamer::codec_t::h264;
args.video_encoding_view.max_bandwidth = 500; // kb/s
streamer.add_source(args);

streamer.start();
```
---

A streamer **recording** an added source in chunks of **100** MB **mkv** files, buffering packets and writing every **3 seconds** to disk:

```c++
lxstreamer::streamer streamer{8000};
streamer.add_source({"src1", "rtsp://192.168.1.10/main"});

lxstreamer::record_options_t opt;
opt.format         = lxstreamer::file_format_t::mkv;
opt.file_size      = 100; // MB
opt.write_interval = 3; // seconds
streamer.start_recording("src1", opt);

streamer.start();
```
---

turn off **stdout** logging, make it verbose and use a custom handler to write messages and errors to files:

```c++
lxstreamer::streamer streamer{8000};

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
...
streamer.start();
```

---

## Notes

To get stream in a client the following format should be used:

- for **http**
```
http://{IP}:{PORT}/stream?source={NAME}
```

- for **https**
```
https://{IP}:{PORT}/stream?source={NAME}
```

For stream authentication you can set `auth_session` value when adding a source to streamer. Then clients should provide a `session` in stream query with the proper value:

```
https://{IP}:{PORT}/stream?source={NAME}&session={AUTH_SESSION}
```


## License

Copyright (C) 2022-present Nejat Afshar <nejatafshar@gmail.com>

Distributed under the MIT License (http://opensource.org/licenses/MIT)