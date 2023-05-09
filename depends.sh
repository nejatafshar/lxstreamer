#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [[ $# > 0 ]]
  then
    GITSRV=$1
  else
    GITSRV=github.com/nejatafshar
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then # Linux
   BRANCH=linux
elif [[ "$OSTYPE" == "darwin"* ]]; then # Mac OSX
   BRANCH=mac
elif [[ "$OSTYPE" == "cygwin" || "$OSTYPE" == "msys" ]]; then # Windows
   BRANCH=win
fi

P=$DIR/3rdparty/ffmpegBin && git -C $P pull || git clone --recursive -j8 -b $BRANCH --single-branch https://$GITSRV/FFmpegBin.git $P

# copy pulled ffmpeg files to ffmpeg dir and remove pulled repo
mkdir -p $DIR/3rdparty/ffmpeg
if [[ "$OSTYPE" == "linux-gnu"* ]]; then # Linux
   cp -r $DIR/3rdparty/ffmpegBin/linux/* $DIR/3rdparty/ffmpeg
elif [[ "$OSTYPE" == "darwin"* ]]; then # Mac OSX
   cp -r $DIR/3rdparty/ffmpegBin/mac/* $DIR/3rdparty/ffmpeg
elif [[ "$OSTYPE" == "cygwin" || "$OSTYPE" == "msys" ]]; then # Windows
   cp -r $DIR/3rdparty/ffmpegBin/win/64/* $DIR/3rdparty/ffmpeg
fi
rm -rf $DIR/3rdparty/ffmpegBin