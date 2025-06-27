#ifndef RAFT_RAFTPEER_H
#define RAFT_RAFTPEER_H

#include "Callback.h"
#include "RaftAsyncGrpcClient.h"
#include "Struct.h"

#include <muduo/base/noncopyable.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/InetAddress.h>

namespace raft {

class RaftPeer : muduo::noncopyable {
 public:
  RaftPeer(int peer, const muduo::net::InetAddress serverAddress);

  ~RaftPeer() = default;

  void Start();

  void RequestVote(const RequestVoteArgs& args);

  void AppendEntries(const AppendEntriesArgs& args);

  void SetRequestVoteReplyCallback(const RequestVoteReplyCallback& cb) {
    requestVoteReply_ = cb;
  }

  void SetAppendEntriesReplyCallback(const AppendEntriesReplyCallback& cb) {
    appendEntriesReply_ = cb;
  }

 private:
  void AssertInLoop() {}

 private:
  const int peer_;
  muduo::net::EventLoopThread rpcLoopThread_;
  std::unique_ptr<RaftAsyncGrpcClient> raftAsyncGrpcClient_;
  RequestVoteReplyCallback requestVoteReply_;
  AppendEntriesReplyCallback appendEntriesReply_;
};

}  // namespace raft

#endif  // RAFT_RAFTPEER_H
