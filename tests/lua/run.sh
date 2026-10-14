#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."
mkdir -p tests/lua/build

native_dir="$PWD/nexilis/build"
export NEXILISC_LIBRARY="$native_dir/libnexilisc.so"

luajit tests/lua/client_test.lua
shopt -s nullglob
boost_includes=()
for include in nexilis/third-party/boost/libs/*/include; do
    if [[ -d "$include" ]]; then boost_includes+=(-I "$include"); fi
done
g++ -std=c++20 -I nexilis/include "${boost_includes[@]}" tests/lua/server.cc \
    -L "$native_dir" -Wl,-rpath,"$native_dir" -lnexilis -lssl -lcrypto -pthread \
    -o tests/lua/build/server

tests/lua/build/server > tests/lua/build/server.log 2>&1 &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true; wait "$server_pid" 2>/dev/null || true' EXIT
sleep 1
luajit tests/lua/client_test.lua integration
