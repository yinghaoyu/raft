//
// Created by frank on 18-5-15.
//

#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Logging.h>
#include <chrono>
#include <functional>

#include "Node.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

using namespace raft;

namespace {

void CheckConfig(const Config& c) {
  // todo
}

}  // namespace

Node::Node(const Config& c)
    : id_(c.id),
      peerNum_(static_cast<int>(c.peerAddresses.size())),
      tickInterval_(c.timeUnit),
      serverAddress_(c.serverAddress),
      loop_(loopThread_.startLoop()) {
  CheckConfig(c);

  std::vector<RaftPeer*> rawPeers;
  for (int i = 0; i < peerNum_; i++) {
    auto ptr = new RaftPeer(i, c.peerAddresses[i]);
    rawPeers.push_back(ptr);
    peers_.emplace_back(ptr);
  }
  raft_ = std::make_unique<Raft>(c, rawPeers);

  for (auto peer : rawPeers) {
    peer->SetRequestVoteReplyCallback(
        std::bind(&Node::OnRequestVoteReply, this, _1, _2, _3));
    peer->SetAppendEntriesReplyCallback(
        std::bind(&Node::OnAppendEntriesReply, this, _1, _2, _3));
  }

  raftAsyncGrpcServer_.SetDoRequestVoteCallback(
      std::bind(&Node::RequestVote, this, _1, _2));
  raftAsyncGrpcServer_.SetDoAppendEntriesCallback(
      std::bind(&Node::AppendEntries, this, _1, _2));
}

void Node::Start() {
  RunTaskInLoopAndWait([=]() { StartInLoop(); });
}

void Node::StartInLoop() {
  AssertInLoop();

  if (started_.exchange(true))
    return;

  // start rpc server
  auto rpc_loop = rpcLoopThread_.startLoop();
  rpc_loop->runInLoop(
      [this]() { raftAsyncGrpcServer_.Run(serverAddress_.toIpPort()); });

  // connect other peerAddresses, non-blocking!
  for (int i = 0; i < peerNum_; i++) {
    if (i != id_) {
      peers_[i]->Start();
    }
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "raft[%d] peerNum = %d starting...", id_,
           peerNum_);
  LOG_DEBUG << buf;

  loop_->runEvery(3, [this]() { raft_->DebugOutput(); });
  loop_->runEvery(static_cast<double>(tickInterval_.count()) / 1000.0,
                  [this]() { raft_->Tick(); });
}

RaftState Node::GetState() {
  AssertStarted();

  RaftState state;
  RunTaskInLoopAndWait([&, this]() {
    AssertStarted();
    state = raft_->GetState();
  });

  return state;
}

ProposeResult Node::Propose(const Json::Value& command) {
  AssertStarted();

  ProposeResult result;
  RunTaskInLoopAndWait([&, this]() {
    AssertStarted();
    result = raft_->Propose(command);
  });
  return result;
}

void Node::RequestVote(const RequestVoteArgs& args,
                       const RequestVoteDoneCallback& done) {
  AssertStarted();

  RunTaskInLoop([=]() {
    RequestVoteReply reply;
    raft_->RequestVote(args, reply);
    done(reply);
  });
}

//
// RequestVote done callback, thread safe.
// In current implementation, it is only called in Raft thread
//
void Node::OnRequestVoteReply(int peer, const RequestVoteArgs& args,
                              const RequestVoteReply& reply) {
  AssertStarted();

  RunTaskInLoop([=]() { raft_->OnRequestVoteReply(peer, args, reply); });
}

void Node::AppendEntries(const AppendEntriesArgs& args,
                         const AppendEntriesDoneCallback& done) {
  AssertStarted();

  RunTaskInLoop([=]() {
    AppendEntriesReply reply;
    raft_->AppendEntries(args, reply);
    done(reply);
  });
}

//
// AppendEntries RPC handler, thread safe
// In current implementation, it is only called in Raft thread
//
void Node::OnAppendEntriesReply(int peer, const AppendEntriesArgs& args,
                                const AppendEntriesReply& reply) {
  AssertStarted();

  RunTaskInLoop([=]() { raft_->OnAppendEntriesReply(peer, args, reply); });
}

template <typename Task>
void Node::RunTaskInLoop(Task&& task) {
  loop_->runInLoop(std::forward<Task>(task));
}

template <typename Task>
void Node::QueueTaskInLoop(Task&& task) {
  loop_->queueInLoop(std::forward<Task>(task));
}

template <typename Task>
void Node::RunTaskInLoopAndWait(Task&& task) {
  muduo::CountDownLatch latch(1);
  RunTaskInLoop([&, this]() {
    task();
    latch.countDown();
  });
  latch.wait();
}