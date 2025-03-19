## Http WebServer
-	Linux轻量级HTTP服务器，实现HTTP GET/POST请求解析与响应
-	使用Reactor模型、非阻塞IO、epoll多路复用、线程池相结合的并发模型
-	使用自动增长缓冲区缓解IO系统调用阻塞
-	使用最小堆定时器，实现关闭超时非活动连接
-	使用MySQL及数据库连接池，实现web端用户注册与登录
-	使用单例模式与阻塞队列，实现同步/异步日志系统记录服务器运行状态
- 参考了[@qinguoyi](https://github.com/qinguoyi/TinyWebServer) 和 [@JehanRio](https://github.com/JehanRio/TinyWebServer) . Thanks for help. ❤
- 压力测试记录/test 5000 clients 30 seconds.
- Windows Subsystem for Linux - Ubuntu 24.04 LTS & Visual Studio Code

