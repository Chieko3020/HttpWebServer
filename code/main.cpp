#include <unistd.h>
#include "server/webserver.h"

int main() 
{

    Web_Server server
    (
        3020, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "suzune", "123456", "tinywebserver", /* Mysql配置 */
        12, 6, true, 1, 1024);             /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    server.Start();
} 
