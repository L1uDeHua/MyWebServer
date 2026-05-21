#include "../include/WebServer.h"

int main() {
    // 创建一个端口 8080，持有 4 个工作线程的 Web 服务器
    WebServer server(8080, 4);
    
    // 点火，起飞！
    server.start();
    
    return 0;
}