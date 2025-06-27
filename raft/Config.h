#ifndef RAFT_CONFIG_H
#define RAFT_CONFIG_H

#include <muduo/net/InetAddress.h>
#include <chrono>
#include <vector>

#include "Callback.h"

namespace raft {

struct Config {

  // 节点id
  int id;

  // leveldb 持久化存储路径
  std::string storagePath;

  // 心跳超时次数，每次tick间隔
  int heartbeatTimeout = 1;

  // 选举超时次数，每次tick间隔
  // assert(heartbeat < electionTimeout)
  int electionTimeout = 5;

  // tick间隔
  std::chrono::milliseconds timeUnit{100};

  // 当前节点的rpc服务端地址
  muduo::net::InetAddress serverAddress;

  // 所有节点的rpc服务端地址
  std::vector<muduo::net::InetAddress> peerAddresses;

  // user callback of a newly applied log
  ApplyCallback applyCallback;

  // user callback of a newly installed snapshot
  SnapshotCallback snapshotCallback;
};

}  // namespace raft

#endif  // RAFT_CONFIG_H
