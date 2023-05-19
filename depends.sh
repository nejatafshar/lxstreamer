#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [[ $# > 0 ]]
  then
    GITSRV=$1
  else
    GITSRV=github.com/nejatafshar
fi

if [[ "$OSTYPE" == "linux-gnu"* ]]; then # Linux
   BRANCH=linux_v6
elif [[ "$OSTYPE" == "darwin"* ]]; then # Mac OSX
   BRANCH=mac_v6
elif [[ "$OSTYPE" == "cygwin" || "$OSTYPE" == "msys" ]]; then # Windows
   BRANCH=win_v6
fi

P=$DIR/3rdparty/ffmpeg && git -C $P pull || git clone --recursive -j8 -b $BRANCH --single-branch https://$GITSRV/FFmpegBin.git $P

if [[ "$OSTYPE" == "linux-gnu"* ]]; then # Linux
   BRANCH=linux
elif [[ "$OSTYPE" == "darwin"* ]]; then # Mac OSX
   BRANCH=mac
elif [[ "$OSTYPE" == "cygwin" || "$OSTYPE" == "msys" ]]; then # Windows
   BRANCH=win
fi

P=$DIR/3rdparty/openssl && git -C $P pull || git clone --recursive -j8 -b $BRANCH --single-branch https://$GITSRV/openssl_builds.git $P