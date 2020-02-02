#!/bin/bash
set -e

tag="build-aae625b2bb06c13a2a40e61fc21444840e40054f"

platform=""

uname="$(uname)"

case "$uname" in
  Linux)
    platform="Linux"
    ;;
  Darwin)
    platform="macOS"
    ;;
  MINGW*)
    platform="Windows"
    ;;
esac

if [[ -z "$platform" ]]; then
  echo "Invalid platform $uname"
  exit 1
fi

zip_file="gbemu-build-$platform.zip"

wget "https://github.com/jack-karamanian/gbemu-build/releases/download/$tag/$zip_file"
unzip "$zip_file" -d ./


