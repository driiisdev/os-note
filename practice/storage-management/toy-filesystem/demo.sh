#!/usr/bin/env bash
# walks through every tinyfs command on a fresh image, then shows the raw bytes on "disk"
set -e
cd "$(dirname "$0")"
fs() { echo "\$ tinyfs $*"; python3 tinyfs.py disk.img "$@"; }

fs mkfs
fs mkdir /docs
echo "hello from inside a disk image" | fs write /docs/hello.txt
fs ls /
fs ls /docs
fs cat /docs/hello.txt
fs ln /docs/hello.txt /hi.txt
fs stat /hi.txt
fs rm /docs/hello.txt
fs cat /hi.txt
fs df

echo
echo "\$ xxd -s \$((11*1024)) -l 96 disk.img   # first data block = the root directory's entries"
xxd -s $((11 * 1024)) -l 96 disk.img
rm -f disk.img
