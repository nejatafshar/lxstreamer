# lxstreamer

lxstreamer is a light-weight, fast and cross-platform c++ library for http/s streaming and recording.

## Features

* http/https video/audio streaming for multiple sources
* cross-platform: compiles for any platform with a `c++14` compiler
* light-wight and fast with low overhead
* record: ability to record sources to `mp4`,`mkv`,... files
* webcam or local video files streaming and recording
* custom encoding: optionally define another encoding for streams or record

## Dependencies

* FFmpeg
* OpenSSL for https

Run `depends.sh` to get depedent libs for your current OS. All dependencies are put into `3rdparty` directory.

## Build

Run in terminal:

```
mkdir build
cd build
cmake /path/to/lxstreamer
make
```
