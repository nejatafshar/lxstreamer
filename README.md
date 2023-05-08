# lxstreamer

lxstreamer is a light-weight, fast and cross-platform c++ library for http/s streaming and recording.

## Features

* http/https video/audio streaming for multiple sources
* cross-platform: compiles for any platform with a `c++17` compiler
* light-wight and fast with low overhead
* record: ability to record sources to `mp4`,`mkv`,... files
* webcam or local media files streaming and recording
* custom encoding: optionally define another encoding for streaming or recording
* seeking and speed change for local media files

## Dependencies

* FFmpeg 5.1.* or later
* OpenSSL 1.1.* for https

Run `depends.sh` to get depedent libs for your current OS. All dependencies are put into `3rdparty` directory.

## Build

Run in terminal:

```
mkdir build
cd build
cmake /path/to/lxstreamer
make
```
