#ifndef RAFT_NODE_H
#define RAFT_NODE_H

#include <atomic>
#include <memory>

#include <muduo/base/noncopyable.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include "Callback.h"
#include "Config.h"
#include "Raft.h"
#include "RaftAsyncGrpcServer.h"
#include "RaftPeer.h"
#include "Struct.h"

namespace raft {

class Node : muduo::noncopyable {
 public:
  Node(const Config& config);

  //
  // start the node instance, thread safe
  //
  void Start();

  //
  // wrapper of Raft::GetState(), thread safe
  //
  RaftState GetState();

  //
  // wrapper of Raft::Propose(), thread safe
  //
  ProposeResult Propose(const Json::Value& command);

 private:
  void StartInLoop();

  //
  // wrapper of Raft::RequestVote(), thread safe
  //
  void RequestVote(const RequestVoteArgs& args, const RequestVoteDoneCallback& done);

  //
  // wrapper of Raft::OnRequestVoteReply(), thread safe
  //
  void OnRequestVoteReply(int peer, const RequestVoteArgs& args, const RequestVoteReply& reply);

  //
  // wrapper of Raft::AppendEntries(), thread safe
  //
  void AppendEntries(const AppendEntriesArgs& args, const AppendEntriesDoneCallback& done);

  //
  // Wrapper of Raft::OnAppendEntriesReply(), thread safe
  //
  void OnAppendEntriesReply(int peer, const AppendEntriesArgs& args, const AppendEntriesReply& reply);

 private:
  //
  // three kinds of eventloop schedulers
  //
  template <typename Task>
  void RunTaskInLoop(Task&& task);

  template <typename Task>
  void QueueTaskInLoop(Task&& task);

  template <typename Task>
  void RunTaskInLoopAndWait(Task&& task);

  void AssertInLoop() const { loop_->assertInLoopThread(); }

  void AssertStarted() const { assert(started_); }

  void AssertNotStarted() const { assert(!started_); }

 private:
  typedef std::unique_ptr<Raft> RaftPtr;
  typedef std::unique_ptr<RaftPeer> RaftPeerPtr;
  typedef std::vector<RaftPeerPtr> RaftPeerList;

  std::atomic<bool> started_{false};
  RaftPtr raft_;
  RaftPeerList peers_;

  const int id_;
  const int peerNum_;

  std::chrono::milliseconds tickInterval_;

  muduo::net::InetAddress serverAddress_;
  RaftAsyncGrpcServer raftAsyncGrpcServer_;
  muduo::net::EventLoopThread rpcLoopThread_;

  muduo::net::EventLoopThread loopThread_;
  muduo::net::EventLoop* loop_;
};

}  // namespace raft

#endif  // RAFT_NODE_H
