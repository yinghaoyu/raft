# 简介

该版本的raft实现源自[MIT 6.824](http://nil.csail.mit.edu/6.824/2017/)。相比于go语言的版本，这个版本的特点有：

- RPC框架使用了[grpc](https://github.com/guangqianpeng/jrpc)，请求是异步的，而lab中是同步的
- 使用多线程 + EventLoop的并发模型，而不是goroutine
- 使用EventLoop, CountdownLatch之类的线程同步设施，拒绝直接使用mutex
- 使用JSON格式的序列化/反序列化，使用LevelDB持久化JSON文本（应该用protobuf ？）

# 功能

- Leader election
- Log replication
- Persistence

# TODO

- Log compaction
- Test
- Benchmark

# 实现

一个raft节点有3个逻辑线程（即3个EventLoop），一个跑gRPC server，一个跑raft算法，另一个跑gRPC client。

核心部分是raft的纯算法实现（[Raft.h](raft/Raft.h)/[Raft.cc](raft/Raft.cc)），它的rpc请求、回复以及时钟都需要外部输入，这些输入具体包括：

- rpc server收到的请求（`Raft::RequestVote()`，`Raft::AppendEntries()`）
- rpc client收到的回复 （`Raft::OnRequestVoteReply()`， `Raft::OnAppendEntriesReply()`）
- 固定频率的时钟激励（`Raft::Tick()`）
- raft用户尝试提交log（`Raft::Propose()`）

这里没有将rpc请求和回复关联起来，而是当成独立的消息输入来处理，这样方便处理 expired/duplicate/out-of-order rpc 消息。用户并不直接使用Raft类，而是使用Node类（[Node.h](raft/Node.h)/[Node.cc](raft/Node.cc)）。Node类封装了rpc通信、时钟、多线程等内容。

# 运行

## 安装

**首先，你需要gcc 7.x或者更高的版本，可以手动安装或者直接上Ubuntu 20.04**

```sh
sudo apt install libleveldb-dev libjsoncpp-dev libprotobuf-dev libgrpc++-dev protobuf-compiler-grpc libboost-all-dev
git clone https://github.com/chenshuo/muduo # 安装muduo网络库，具体步骤参考muduo文档
git clone https://github.com/yinghaoyu/raft.git
cd raft
./build.sh
cd ../raft-build/Release/bin
```

## 单节点

开一个单节点的raft，server端口号是`8090`，虽然这个server没什么用：

```sh
stdbuf -oL ./raft_demo 0 8090 | grep 'raft\['
```

你可以看到运行的流程，每隔1s，leader就会propose一条log，最后一行是每隔5s输出一次的统计信息：

```
20250627 11:36:37.982142Z 21266 DEBUG Raft raft[0] follower, term 7030, first_index 0, last_index 1857 - Raft.cc:31
20250627 11:36:37.983499Z 21268 DEBUG StartInLoop raft[0] peerNum = 1 starting... - Node.cc:80
20250627 11:36:38.484249Z 21268 DEBUG ToCandidate raft[0] follower -> candidate - Raft.cc:367
20250627 11:36:38.485052Z 21268 DEBUG ToLeader raft[0] candidate -> leader - Raft.cc:386
20250627 11:36:38.986076Z 21268 DEBUG Propose raft[0] leader, term 7031, propose log 1858 - Raft.cc:48
20250627 11:36:38.986130Z 21268 DEBUG ApplyLog raft[0] leader, term 7031, apply log (0, 1858] - Raft.cc:309
20250627 11:36:39.985031Z 21268 DEBUG Propose raft[0] leader, term 7031, propose log 1859 - Raft.cc:48
20250627 11:36:39.985086Z 21268 DEBUG ApplyLog raft[0] leader, term 7031, apply log [1859] - Raft.cc:304
20250627 11:36:40.984791Z 21268 DEBUG Propose raft[0] leader, term 7031, propose log 1860 - Raft.cc:48
20250627 11:36:40.984843Z 21268 DEBUG ApplyLog raft[0] leader, term 7031, apply log [1860] - Raft.cc:304
20250627 11:36:40.984883Z 21268 DEBUG DebugOutput raft[0] leader, term 7031, #votes 1, commit 1860 - Raft.cc:293
```

## 多节点

开3个节点的raft集群，需要起3个进程，server端口号分别是`8090,8091,8092`分别运行：

```shell
stdbuf -oL ./raft_demo 0 8090 8091 8092 | grep 'raft\['
stdbuf -oL ./raft_demo 1 8090 8091 8092 | grep 'raft\['
stdbuf -oL ./raft_demo 2 8090 8091 8092 | grep 'raft\['
```

你可以看到leader election的过程，然后重启一个进程，看看会发生什么？