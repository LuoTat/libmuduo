# libmuduo

一个简单的网络库

## 项目结构

采用了多 Reactor 多线程的模型，作为一个服务器的核心框架，其主要的流程如下：

1. 主线程创建一个 MainReactor 对象，同时持有一个 Acceptor 对象，负责监听客户端的连接请求
2. 主线程创建多个子线程，每个子线程创建一个 SubReactor 对象，用来处理连接后的 I/O 事件

这里的 MainReactor 和 SubReactor 具体实现就是`EventLoop`类，Acceptor 具体实现是`Acceptor`类。而每一个 `EventLoop` 对象都持有一个 `EPoll` 对象，`EPoll` 对象负责监听多个 `Channel` 对象的事件。

## 主要类介绍

### Channel

封装了一个文件描述符的事件分发器，负责监听该文件描述符上的事件，并在事件发生时调用相应的回调函数。

### EPoll

封装了 Linux 的 epoll 机制，负责管理多个 Channel 对象，并监听它们的事件。采用了水平触发（Level Triggered）模式。

### EventLoop

封装了事件循环的逻辑，负责不断地监听 EPoll 对象上的事件，并分发给相应的 Channel 对象进行处理。每个 EventLoop 对象运行在一个独立的线程中。

### Acceptor

相当与一个特殊的 Channel，负责监听客户端的连接请求，并在有新的连接请求时，创建一个新的 TcpConnection 对象。

### TcpServer

主要负责服务器的启动和停止，管理所有的 TcpConnection 对象，并处理新连接的到来。设定了 Acceptor 的回调函数，以便在有新的连接请求时，创建一个新的 TcpConnection 对象。并从 EventLoopThreadPool 里面获取一个子 EventLoop 来绑定这个新的 TcpConnection 对象。

同时也是用户与库交互的主要接口，用户可以通过 TcpServer 对象来设置各种回调函数，以处理不同的事件。

### TcpConnection

封装了一个 TCP 连接，负责处理该连接上的 I/O 事件，并调用相应的回调函数。

## 工具类介绍

### Buffer

封装了一个缓冲区，用来给 TcpConnection 提供读和写的缓冲区。

### SockAddress

封装了套接字地址相关的操作，提供了方便的接口来设置和获取 IP 地址和端口号。

### Socket

封装了套接字相关的操作，提供了方便的接口来创建、绑定

### EventLoopThread

封装了一个线程，负责运行 SubEventLoop 对象。
其实基本上是对 `std::jthread` 的一个简单封装。主要是为了提供一个可以延迟启动线程的接口。

### EventLoopThreadPool

封装了一个线程池，负责管理多个 EventLoopThread 对象，并提供接口来获取一个子 EventLoop 对象。

### Timestamp

封装了时间戳相关的操作，提供了方便的接口来获取当前时间和格式化时间。

### Logger

封装了日志相关的操作，提供了方便的接口来输出日志信息。

## 性能测试

测试硬件环境：

-   操作系统： Gentoo Linux 2.18
-   KDE Plasma 版本： 6.4.5
-   KDE 程序框架版本： 6.18.0
-   Qt 版本： 6.9.3
-   内核版本： 6.17.1-gentoo (64 位)
-   图形平台： Wayland
-   处理器： 16 × AMD Ryzen 9 7940HS w/ Radeon 780M Graphics
-   内存： 16 GiB 内存 (14.9 GiB 可用)
-   图形处理器： AMD Radeon 780M Graphics
-   制造商： ASUSTeK COMPUTER INC.
-   产品名称： VivoBook_ASUSLaptop M1605XA_M1605XA
-   系统版本： 1.0

使用 `ab` 工具对服务器进行性能测试，测试命令如下：

```bash
ab -c 1000 -n 1000000 http://127.0.0.1:8888/
```

测试结果如下：

```
This is ApacheBench， Version 2.3 <$Revision: 1923142 $>
Copyright 1996 Adam Twiss， Zeus Technology Ltd， http://www.zeustech.net/
Licensed to The Apache Software Foundation， http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)
Completed 100000 requests
Completed 200000 requests
Completed 300000 requests
Completed 400000 requests
Completed 500000 requests
Completed 600000 requests
Completed 700000 requests
Completed 800000 requests
Completed 900000 requests
Completed 1000000 requests
Finished 1000000 requests


Server Software:
Server Hostname:        127.0.0.1
Server Port:            8080

Document Path:          /
Document Length:        13 bytes

Concurrency Level:      1024
Time taken for tests:   15.021 seconds
Complete requests:      1000000
Failed requests:        0
Total transferred:      97000000 bytes
HTML transferred:       13000000 bytes
Requests per second:    66571.81 [#/sec] (mean)
Time per request:       15.382 [ms] (mean)
Time per request:       0.015 [ms] (mean， across all concurrent requests)
Transfer rate:          6306.12 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.2      0       7
Processing:     4   15   1.3     15      30
Waiting:        0   15   1.3     15      30
Total:          8   15   1.3     15      30

Percentage of the requests served within a certain time (ms)
  50%     15
  66%     15
  75%     16
  80%     16
  90%     17
  95%     18
  98%     19
  99%     19
 100%     30 (longest request)
```

使用 `wrk` 工具对服务器进行性能测试，测试命令如下：

```bash
wrk -t16 -c1024 -d30s http://127.0.0.1:8080/
```

测试结果如下：

```
Running 30s test @ http://127.0.0.1:8080/
  16 threads and 1024 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    13.14ms    7.25ms  65.20ms   67.57%
    Req/Sec     3.05k   630.81     4.53k    66.03%
  1453625 requests in 30.10s， 134.47MB read
Requests/sec:  48293.19
Transfer/sec:      4.47MB

```
