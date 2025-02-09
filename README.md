# Torrent client

## Description

This is a simple torrent client written with only Linux sockets and OpenSSL library as a pet project to learn networking and asynchronous sockets. It doesn't seed files and lack all of advanced functionality.

## Build

```bash
cmake -S . -B build
cmake --build build
./myTorrent path-to-torrent-file
```
