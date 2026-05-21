C++ High Performance Web Server
这是一个基于 C++11 开发的高并发 Web 服务器。
架构：采用 半同步/半反应堆 (Reactor) 模型。
底层：基于 Epoll I/O 多路复用（ET 边缘触发模式） + 非阻塞 Socket。
多线程：手写基于 std::condition_variable 和 std::mutex 的高性能线程池。
业务层：利用状态机解析 HTTP 请求（持续开发中...）。
