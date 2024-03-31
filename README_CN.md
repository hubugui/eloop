# Eloop

[English](README.md)

## 介绍

Eloop是一个参考基于C语言的事件驱动基础库，提供：
* TCP Server，异步处理IO。
  - 目前支持Windows下select和MacOs下的KQueue。
* 定时器。
* 异步函数调用。

实现过程中参考了[libae](https://github.com/aisk/libae)。感谢！

![](eloop.png)
![](eloop_sequence.png)

## 开发

### 操作系统

1. Windows
   * 使用Msys2，[msys2-x86_64-20231026.exe](https://objects.githubusercontent.com/github-production-release-asset-2e65be/80988227/2e09490c-3e60-4f04-aadc-c38d76dd741c?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAVCODYLSA53PQK4ZA%2F20240108%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20240108T143209Z&X-Amz-Expires=300&X-Amz-Signature=9db964a8691b5547d0adb0f6a8b0af976e156b868640d59631d529983e565cec&X-Amz-SignedHeaders=host&actor_id=1264972&key_id=0&repo_id=80988227&response-content-disposition=attachment%3B%20filename%3Dmsys2-x86_64-20231026.exe&response-content-type=application%2Foctet-stream)
2. Linux
   * 进行中，待验证。
3. MacOs
   *  

## 构建

1. make
2. make clean

## 使用

1. 启动pingpong服务，如Windows上的`./pingpong.exe`。
1. 如下打开telnet客户端：
   - `telnet 127.0.0.1 14317`
   - 输入`ping`，回车，会收到服务器端的`pong`响应。
   - 输入`exit`, 回车，退出。

### 待办

* [x] MacOs Kqueue
* [ ] Linux epoll
* [ ] Log callback function
* [ ] Http Server example
* [ ] CI automated testing, base on pingpong.c
* [ ] Binary tree implement timer operate
* [ ] Performance test

## 贡献

1. 欢迎贡献代码和提出问题！