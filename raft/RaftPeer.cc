#include "RaftPeer.h"
#include "Raft.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

using namespace raft;

RaftPeer::RaftPeer(int peer, const muduo::net::InetAddress serverAddress)
    : peer_(peer), raftAsyncGrpcClient_(std::make_unique<RaftAsyncGrpcClient>(serverAddress.toIpPort())) {}

void RaftPeer::Start() {
  AssertInLoop();
  // start rpc client
  auto rpc_loop = rpcLoopThread_.startLoop();
  rpc_loop->runInLoop([this]() { raftAsyncGrpcClient_->AsyncCompleteRpc(); });
}

void RaftPeer::RequestVote(const RequestVoteArgs& args) {
  AssertInLoop();

  RequestVoteReq req;
  req.set_term(args.term);
  req.set_candidateid(args.candidateId);
  req.set_lastlogindex(args.lastLogIndex);
  req.set_lastlogterm(args.lastLogTerm);

  raftAsyncGrpcClient_->AsyncRequestVote(req, [=](const RequestVoteRsp& response) {
    RequestVoteReply reply{response.term(), response.votegranted()};
    requestVoteReply_(peer_, args, reply);
  });
}

void RaftPeer::AppendEntries(const AppendEntriesArgs& args) {
  AssertInLoop();

  AppendEntriesReq req;
  req.set_term(args.term);
  req.set_prevlogindex(args.prevLogIndex);
  req.set_prevlogterm(args.prevLogTerm);
  for (const auto& entry : args.entries) {
    auto* e = req.add_entries();
    e->set_term(entry["term"].asInt());
    e->set_command(entry["command"].asString());
  }
  req.set_leadercommit(args.leaderCommit);

  raftAsyncGrpcClient_->AsyncAppendEntries(req, [=](const AppendEntriesRsp& response) {
    AppendEntriesReply reply{response.term(), response.success(), response.expectindex(), response.expectterm()};
    appendEntriesReply_(peer_, args, reply);
  });
}
