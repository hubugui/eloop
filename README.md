# Eloop

[中文](README_CN.md)

## Introduction

Eloop is an event-driven foundation library based on the C language, providing:

* TCP Server for asynchronous IO.
  - Currently, only supports select on Windows and KQueue on MacOs.
* Timers.
* Asynchronous function callback.

During the implementation, reference was made to [libae](https://github.com/aisk/libae). Thanks!

![](eloop.png)

## Development

### Operating Systems

1. Windows
   * [msys2-x86_64-20231026.exe](https://objects.githubusercontent.com/github-production-release-asset-2e65be/80988227/2e09490c-3e60-4f04-aadc-c38d76dd741c?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAVCODYLSA53PQK4ZA%2F20240108%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20240108T143209Z&X-Amz-Expires=300&X-Amz-Signature=9db964a8691b5547d0adb0f6a8b0af976e156b868640d59631d529983e565cec&X-Amz-SignedHeaders=host&actor_id=1264972&key_id=0&repo_id=80988227&response-content-disposition=attachment%3B%20filename%3Dmsys2-x86_64-20231026.exe&response-content-type=application%2Foctet-stream)
2. Linux
   * In progress, to be verified.
3. MacOs

### Build

* make
* make clean

### Usage

1. Start the pingpong service, such as ./pingpong.exe on Windows.
2. Open a telnet client as follows:
   * `telnet 127.0.0.1 14317`
   * Enter "ping", press Enter, and you will receive a "pong" response from the server.
   * Enter "exit", press Enter, exit game.

### Todo

* [x] MacOs Kqueue
* [ ] Linux epoll
* [ ] Log callback function
* [ ] Http Server example
* [ ] CI automated testing, base on pingpong.c
* [ ] Binary tree implement timer operate
* [ ] Performance test

## Contribution

Contributions of code and issue reports are welcome!