#!/bin/bash
dl(){ local repo=$1 br=$2 out=$3
  curl -sL -C - --max-time 900 "https://gh-proxy.org/https://github.com/$repo/archive/refs/heads/$br.zip" -o "$out.zip"
  unzip -qo "$out.zip" 2>/dev/null || tar -xf "$out.zip" 2>/dev/null
  echo "DONE $out"
}
dl "PSP-Archive/PicoDrive-notaz" "main" PicoDrive-notaz &
dl "cowboyh4t/JTube" "master" JTube &
dl "Hexadecinull/QRScan" "main" QRScan &
dl "Klozz/Symbian-Open-Source" "master" Symbian-Open-Source &
dl "wayfinder/Wayfinder-CppCore-v2" "master" Wayfinder-CppCore-v2 &
dl "yomcube/ClassiCube" "master" ClassiCube &
dl "libwbxml/libwbxml" "master" libwbxml &
wait
echo ALL_DONE
