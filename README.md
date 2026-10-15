# TCP 学习提交版


- 服务端保留 `socket -> bind -> listen -> accept` 流程。
- 客户端保留 `socket -> connect` 流程。
- 通信使用 `send/recv`。
- 每个客户端由一个独立线程处理。
- 以换行符 `\\n` 作为一条应用层消息的结束标志，用来处理 TCP 的半包和粘包问题。

## 编译

```bash
make
```

## 运行

终端 1：

```bash
./tcp_server 8888
```

终端 2：

```bash
./tcp_client 127.0.0.1 8888
```

输入普通文本后回车，服务器会返回 `server echo# ...`。输入 `q` 退出客户端。

## 清理

```bash
make clean
```
