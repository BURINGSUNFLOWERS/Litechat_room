# chat_room
基于 epoll + 线程池 的 Linux 高并发 TCP 聊天室服务端，支持多客户端实时群聊、用户注册登录、在线状态同步，使用 MySQL 持久化用户数据。全程 C++ 编写，无第三方框架依赖。

## 功能特性

- **多客户端并发**：基于 epoll 多路复用，支持大量客户端同时在线
- **实时群聊**：消息广播，所有在线用户实时接收
- **用户管理**：注册、登录、修改昵称、注销账号
- **在线状态**：用户上下线自动通知，异常断开自动清理
- **权限控制**：登录后才能发送消息
- **数据持久化**：MySQL 存储用户信息
  
## 技术栈

| 层次 | 技术 |
|------|------|
| 语言 | C++11 |
| 编译器 | GCC |
| 网络 I/O | epoll + 非阻塞 Socket |
| 并发 | 线程池（pthread + 条件变量） |
| 数据库 | MySQL C API |
| 构建 | Makefile |

## 系统架构

\`\`\`
                    ┌──────────┐  ┌──────────┐
                    │ 客户端1   │  │ 客户端2   │
                    └─────┬─────┘  └─────┬─────┘
                          │              │
                          └──────┬───────┘
                                 │ TCP
                    ┌─────────────┴──────────────┐
                    │    主线程（epoll 事件循环）  │
                    │    accept / recv / send     │
                    └─────────────┬──────────────┘
                                  │ 任务队列
                    ┌─────────────┴──────────────┐
                    │    线程池（4 个工作线程）    │
                    │    业务处理 + 数据库操作     │
                    └─────────────┬──────────────┘
                                  │
                    ┌─────────────┴──────────────┐
                    │          MySQL             │
                    └────────────────────────────┘
\`\`\`

## 快速开始

### 环境要求

- Linux 系统（Ubuntu 20.04+）
- GCC 5.0+（支持 C++11）
- MySQL 5.7+
- libmysqlclient-dev

### 安装依赖

\`\`\`bash
sudo apt update
sudo apt install g++ mysql-server mysql-client libmysqlclient-dev
\`\`\`

### 初始化数据库

\`\`\`bash
# 登录 MySQL
mysql -u root -p

# 创建数据库和表
CREATE DATABASE IF NOT EXISTS chatroom DEFAULT CHARACTER SET utf8;
USE chatroom;
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(50) NOT NULL,
    nickname VARCHAR(50) DEFAULT NULL,
    online TINYINT DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
\`\`\`

### 配置数据库密码

编辑 \`mysql_db.cpp\`，修改密码为你的 MySQL 密码：

\`\`\`cpp
static const char* DB_PASS = "你的密码";
\`\`\`

### 编译

\`\`\`bash
make
\`\`\`

### 运行

\`\`\`bash
# 终端1：启动服务端
./server

# 终端2：启动客户端
./client 127.0.0.1 8888
\`\`\`

## 使用说明

### 命令列表

| 命令 | 格式 | 说明 |
|------|------|------|
| 注册 | `REGISTER 用户名 密码` | 创建新账号 |
| 登录 | `LOGIN 用户名 密码` | 登录聊天室 |
| 发消息 | `MSG 消息内容` | 群发消息 |
| 改昵称 | `CHNICK 新昵称` | 修改显示名称 |
| 注销 | `DELETEACCOUNT` | 永久删除账号 |
| 退出 | `QUIT` | 正常退出 |

### 演示

\`\`\`
# Alice 注册并登录
REGISTER alice 123456
LOGIN alice 123456

# Bob 注册并登录（另一个终端）
REGISTER bob 123456
LOGIN bob 123456

# Alice 发消息
MSG Hello everyone!
→ 所有在线用户收到: MSG alice: Hello everyone!

# Bob 修改昵称
CHNICK Bobby
→ Bob 之后的发言显示为 Bobby

# Alice 退出
QUIT
→ 其他用户收到: USER_OFFLINE alice
\`\`\`

## 目录结构


chatroom/
├── common.h           # 公共数据结构
├── threadpool.h       # 线程池接口
├── threadpool.cpp     # 线程池实现
├── mysql_db.h         # 数据库接口
├── mysql_db.cpp       # 数据库实现
├── server.cpp         # 服务端主程序
├── client.cpp         # 客户端
├── Makefile           # 编译脚本
└── README.md          # 项目文档


## 设计要点

### 网络模型
采用单 Reactor + 半同步/半异步模型。主线程运行 epoll 事件循环，
只负责网络 I/O；业务逻辑提交给线程池异步处理，避免阻塞事件循环。

### 数据库连接管理
每个工作线程维护独立的 MySQL 连接，用 `map<thread::id, MYSQL*>`
管理。线程启动时建立连接，退出时释放，避免锁竞争。

### 线程安全
- 全局用户列表用互斥锁保护，broadcast 先复制快照再发送，缩短锁持有时间
- 每个 socket 的写操作用独立的 send_mutex 保护，防止多线程并发发送数据交错
- 任务队列用互斥锁 + 条件变量实现生产者-消费者模型

