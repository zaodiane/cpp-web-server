# cpp-web-server

基于 Reactor 模式的高性能 C++ Web 服务器，支持 HTTP/1.1 静态文件服务。

## 项目简介

本项目使用 C++11 实现了一个事件驱动的 Web 服务器，底层采用 **epoll 边缘触发 + 非阻塞 IO**，并实现了 **主从 Reactor 多线程模型**。

- **主从 Reactor**：主线程只负责 accept，子线程各自运行 EventLoop 处理已连接 socket 的 IO，职责分离，适合有阻塞操作的业务场景。压测 QPS 约 **9000**（并发 300，零失败）。

## 特性

- 基于 epoll ET + 非阻塞 IO 的事件驱动模型
- 主从 Reactor 多线程架构，支持跨线程任务分发（runInLoop + eventfd）
- 自动管理连接生命周期（shared_ptr + enable_shared_from_this）
- HTTP/1.1 静态文件服务，支持 GET 请求
- 支持 MIME 类型识别（根据扩展名返回 Content-Type）
- 压测验证：并发 300 零失败，QPS 约 9000

## 架构设计

### 整体架构

```
主线程
  EventLoop (mainLoop)
    └── Acceptor (lfd)
         └── accept() → cfd
              └── TcpServer::onNewConnection
                   ├── 轮询选 subLoop
                   ├── 创建 TcpConnection
                   └── subLoop->runInLoop
  connections_.erase(id) ←── runInLoop

子线程
  EventLoop (subLoop)
    ├── eventfd 被唤醒
    │    └── doPendingFunctors → conn->start()
    │         └── epoll_ctl 注册 cfd
    └── cfd 可读
         └── handleRead
              ├── recv 循环到 EAGAIN
              ├── 解析 HTTP
              ├── send 响应
              └── handleClose
                   ├── epoll_ctl 移除 cfd
                   ├── close(cfd)
                   └── closeCallback → 回到主线程 erase

```

### 核心模块

| 模块 | 职责 |
|------|------|
| EventLoop | 事件循环，封装 epoll_wait 和任务队列 |
| Channel | 绑定 fd 和事件回调，负责事件分发 |
| Acceptor | 监听 socket，处理新连接 |
| TcpConnection | 管理单个连接的生命周期和读写 |
| TcpServer | 总控，管理 Acceptor、子 Reactor 和连接表 |

## 目录结构

```
cpp-web-server/
├── code/
│   ├── include/
│   │   ├── EventLoop.h
│   │   ├── Channel.h
│   │   ├── Acceptor.h
│   │   ├── TcpConnection.h
│   │   └── TcpServer.h
│   ├── src/
│   │   ├── EventLoop.cpp
│   │   ├── Channel.cpp
│   │   ├── Acceptor.cpp
│   │   ├── TcpConnection.cpp
│   │   ├── TcpServer.cpp
│   │   └── main.cpp
│   ├── static/
│   │   ├── index.html
│   │   └── about.html
│   └── Makefile
├── README.md
└── .gitignore
```

## 构建与运行

### 环境要求

- Linux（Ubuntu 20.04 或更高）
- g++ 支持 C++11
- make

### 编译

```bash
cd code
make
```

### 运行

```bash
./bin/reactor
```

默认监听 `0.0.0.0:8080`，浏览器访问 `http://127.0.0.1:8080/` 即可。

### 配置子 Reactor 数量

在 `main.cpp` 中修改 `TcpServer` 构造参数：

```cpp
// 主从 Reactor，默认 4 个子线程
TcpServer server(&mainLoop, 8080, 4);
```

## 压测数据

使用 `ab`（Apache Bench）压测，命令：

```bash
ab -n 10000 -c 100 http://127.0.0.1:8080/
```

| 并发 | QPS | 失败请求 |
|------|-----|----------|
| 100 | 8908 | 0 |
| 300 | 9004 | 0 |
| 500 | ~9000 | 1 超时 |

> 注：压测在 4 核虚拟机中进行，ab 与服务器共享 CPU，QPS 数据受环境限制。主从 Reactor 在短连接、静态文件场景下开销大于收益，但架构本身适合有阻塞操作的业务。

## 遇到的问题与解决

### 1. fd 复用导致 double free

**现象**：主从 Reactor 版本压测时出现 `free(): double free detected`，服务器崩溃。

**排查**：

- 加日志定位到 `~TcpConnection` 时 `channel_` 不是 `nullptr`，说明 `channel_` 被删除两次。
- 进一步发现同一个 fd 被反复关闭和新建，意识到是 **fd 复用** 问题。
- `connections_` 用 fd 做 key，新连接覆盖了旧连接，导致旧连接的 `shared_ptr` 引用计数归零被销毁，但旧连接的 `handleClose` 可能还在子线程执行，于是 `channel_` 被双重释放。

**解决**：

- 给每个连接分配自增唯一 `id`，用 `id` 代替 `fd` 作为 `connections_` 的 key。
- `TcpConnection` 继承 `enable_shared_from_this`，回调捕获 `shared_ptr`，保证事件处理期间对象不被销毁。
- `handleClose` 使用 `std::atomic<bool>` 和 `exchange(true)` 保证只执行一次。

**结果**：重新压测，并发 100 和 300 稳定，QPS 9000 左右，零失败。

### 2. ET 模式下数据读取不完整

**解决**：`handleRead` 中使用 `while(true)` 循环 `recv`，直到返回 `EAGAIN` 才退出，保证一次事件把数据读完。

### 3. EINTR 处理

**解决**：`recv` 返回 `-1` 且 `errno == EINTR` 时 `continue`，继续读取。

## 后续优化方向

- **长连接**：支持 HTTP Keep-Alive，减少 accept/close 开销
- **内存池**：复用 TcpConnection 和 Channel 对象，减少 shared_ptr 原子操作
- **零拷贝**：使用 `sendfile` 发送静态文件
- **SO_REUSEPORT**：让多个子线程各自监听同一端口，避免主线程 accept 瓶颈
- **MIME 类型动态识别**：根据文件扩展名返回正确的 Content-Type

## 技术栈

- C++11
- epoll (ET)
- 非阻塞 IO
- 多线程
- shared_ptr / enable_shared_from_this
- eventfd

## 参考

- 《Linux 高性能服务器编程》—— 游双
- muduo 网络库

## License

MIT
