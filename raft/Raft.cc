//
// Created by frank on 18-4-19.
//

#include "Raft.h"
#include "RaftPeer.h"
#include "Storage.h"

#include <muduo/base/Logging.h>

using namespace raft;

Raft::Raft(const Config& c, const std::vector<RaftPeer*>& peers)
    : id_(c.id),
      peerNum_(static_cast<int>(peers.size())),
      storage_(c.storagePath),
      currentTerm_(storage_.GetCurrentTerm()),
      votedFor_(storage_.GetVotedFor()),
      log_(&storage_),
      heartbeatTimeout_(c.heartbeatTimeout),
      electionTimeout_(c.electionTimeout),
      randomGen_(id_, electionTimeout_, 2 * electionTimeout_),
      peers_(peers),
      applyCallback_(c.applyCallback),
      snapshotCallback_(c.snapshotCallback) {
  ResetTimer();
  char buf[128];
  snprintf(buf, sizeof(buf),
           "raft[%d] %s, term %d, first_index %d, last_index %d", id_,
           RoleString(), currentTerm_, log_.FirstIndex(), log_.LastIndex());
  LOG_DEBUG << buf;
}

RaftState Raft::GetState() const {
  return {currentTerm_, role_ == kLeader};
}

ProposeResult Raft::Propose(const Json::Value& command) {
  int index = log_.LastIndex() + 1;
  int currentTerm = currentTerm_;
  bool isLeader = (role_ == kLeader);

  if (isLeader) {
    log_.Append(currentTerm_, command);
    char buf[128];
    snprintf(buf, sizeof(buf), "raft[%d] %s, term %d, propose log %d", id_,
             RoleString(), currentTerm_, index);
    LOG_DEBUG << buf;
  }

  if (IsStandalone()) {
    // there is only one node in raft cluster,
    // log proposed can be committed and applied right now
    commitIndex_ = index;
    ApplyLog();
  }

  return {index, currentTerm, isLeader};
}

void Raft::StartRequestVote() {
  RequestVoteArgs args;
  args.term = currentTerm_;
  args.candidateId = id_;
  args.lastLogIndex = log_.LastIndex();
  args.lastLogTerm = log_.LastTerm();

  for (int i = 0; i < peerNum_; i++) {
    if (i != id_) {
      peers_[i]->RequestVote(args);
    }
  }
}

void Raft::RequestVote(const RequestVoteArgs& args, RequestVoteReply& reply) {
  OnNewInputTerm(args.term);
  ResetTimer();

  reply.term = currentTerm_;

  if (args.term == currentTerm_ &&
      (votedFor_ == kVotedForNull || votedFor_ == args.candidateId) &&
      log_.IsUpToDate(args.lastLogIndex, args.lastLogTerm)) {
    char buf[128];
    snprintf(buf, sizeof(buf), "raft[%d] -> raft[%d]", id_, args.candidateId);
    LOG_DEBUG << buf;
    SetVotedFor(args.candidateId);
    reply.voteGranted = true;
  } else {
    reply.voteGranted = false;
  }
}

void Raft::OnRequestVoteReply(int peer, const RequestVoteArgs& args,
                              const RequestVoteReply& reply) {
  OnNewInputTerm(reply.term);

  if (role_ != kCandidate ||      // not a candidate anymore
      !reply.voteGranted ||       // vote not granted
      currentTerm_ > reply.term)  // expired vote
  {
    return;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "raft[%d] <- raft[%d]", id_, peer);
  LOG_DEBUG << buf;

  votesGot_++;
  if (votesGot_ > peerNum_ / 2) {
    ToLeader();
  }
}

void Raft::StartAppendEntries() {
  for (int i = 0; i < peerNum_; i++) {
    if (i == id_)
      continue;

    AppendEntriesArgs args;
    args.term = currentTerm_;
    args.prevLogIndex = nextIndex_[i] - 1;
    args.prevLogTerm = log_.TermAt(args.prevLogIndex);
    args.entries = log_.GetEntriesAsJson(nextIndex_[i], kMaxEntriesSendOneTime);
    args.leaderCommit = commitIndex_;
    peers_[i]->AppendEntries(args);
  }
}

void Raft::AppendEntries(const AppendEntriesArgs& args,
                         AppendEntriesReply& reply) {
  OnNewInputTerm(args.term);
  ResetTimer();

  reply.term = currentTerm_;

  if (currentTerm_ > args.term) {
    // expired heartbeat
    reply.success = false;
    return;
  } else if (role_ == kCandidate) {
    // lose leader election
    ToFollower(currentTerm_);
  } else if (role_ == kLeader) {
    char buf[128];
    snprintf(buf, sizeof(buf), "multiple leaders in term %d", currentTerm_);
    LOG_FATAL << buf;
  }

  //
  // invariant here:
  //   1. role == kFollower
  //   2. args.term == currentTerm
  //
  if (log_.Contain(args.prevLogIndex, args.prevLogTerm)) {
    log_.Overwrite(args.prevLogIndex + 1, args.entries);

    //
    // update commit index monotonically
    //
    int possibleCommit = std::min(args.leaderCommit, log_.LastIndex());
    if (commitIndex_ < possibleCommit) {
      commitIndex_ = possibleCommit;
      ApplyLog();
    }
    reply.success = true;
  } else {
    auto p = log_.LastIndexInTerm(args.prevLogIndex, args.prevLogTerm);
    reply.expectIndex = p.index;
    reply.expectTerm = p.term;
    reply.success = false;
  }
}

void Raft::OnAppendEntriesReply(int peer, const AppendEntriesArgs& args,
                                const AppendEntriesReply& reply) {
  OnNewInputTerm(reply.term);

  if (role_ != kLeader || currentTerm_ > reply.term) {
    // 1. not a leader anymore
    // 2. expired RPC(return too late)
    return;
  }

  if (!reply.success) {
    //
    // log replication failed, back nexIndex_[peer] quickly!!!
    //
    int nextIndex = nextIndex_[peer];

    if (reply.expectTerm == args.prevLogTerm) {
      assert(reply.expectIndex < args.prevLogIndex);
      nextIndex = reply.expectIndex;
    } else {
      assert(reply.expectTerm < args.prevLogTerm);
      auto p = log_.LastIndexInTerm(nextIndex, reply.expectTerm);
      nextIndex = p.index;
    }

    //
    // take care of duplicate & out-of-order & expired reply
    //
    if (nextIndex > nextIndex_[peer]) {
      nextIndex = nextIndex_[peer] - 1;
    }
    if (nextIndex <= matchIndex_[peer]) {
      char buf[128];
      snprintf(buf, sizeof(buf),
               "raft[%d] %s, nextIndex <= matchIndex_[%d], set to %d", id_,
               RoleString(), peer, matchIndex_[peer] + 1);
      LOG_DEBUG << buf;
      nextIndex = matchIndex_[peer] + 1;
    }

    nextIndex_[peer] = nextIndex;
    return;
  }

  //
  // log replication succeed
  //
  int startIndex = args.prevLogIndex + 1;
  int entryNum = static_cast<int>(args.entries.size());
  int endIndex = startIndex + entryNum - 1;

  for (int i = endIndex; i >= startIndex; i--) {

    //
    // log[i] has already replicated on peer,
    // duplicate reply takes no effects
    //
    if (i <= matchIndex_[peer])
      break;

    //
    // a leader cannot immediately conclude that a
    // entry from previous term is committed once it is
    // stored on majority of servers, so, just don't count #replica
    //
    if (log_.TermAt(i) < currentTerm_)
      break;
    assert(log_.TermAt(i) == currentTerm_);

    //
    // logs already committed
    //
    if (i <= commitIndex_)
      break;

    //
    // initial replica is 2, one for id_, one for peer
    //
    int replica = 2;
    for (int p = 0; p < peerNum_; p++) {
      if (i <= matchIndex_[p])
        replica++;
    }

    //
    // update commitIndex monotonically
    //
    if (replica > peerNum_ / 2) {
      commitIndex_ = i;
      break;
    }
  }

  ApplyLog();
  if (nextIndex_[peer] <= endIndex) {
    nextIndex_[peer] = endIndex + 1;
    matchIndex_[peer] = endIndex;
  }
}

void Raft::Tick() {
  switch (role_) {
    case kFollower:
    case kCandidate:
      TickOnElection();
      break;
    case kLeader:
      TickOnHeartbeat();
      break;
    default:
      assert(false && "bad role");
  }
}

void Raft::DebugOutput() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "raft[%d] %s, term %d, #votes %d, commit %d", id_,
           RoleString(), currentTerm_, votesGot_, commitIndex_);
  LOG_DEBUG << buf;
}

void Raft::ApplyLog() {
  assert(lastApplied_ <= commitIndex_);

  if (commitIndex_ != lastApplied_) {
    if (lastApplied_ + 1 == commitIndex_) {
      char buf[128];
      snprintf(buf, sizeof(buf), "raft[%d] %s, term %d, apply log [%d]", id_,
               RoleString(), currentTerm_, commitIndex_);
      LOG_DEBUG << buf;
    } else {
      char buf[128];
      snprintf(buf, sizeof(buf), "raft[%d] %s, term %d, apply log (%d, %d]",
               id_, RoleString(), currentTerm_, lastApplied_, commitIndex_);
      LOG_DEBUG << buf;
    }
  }

  for (int i = lastApplied_ + 1; i <= commitIndex_; i++) {
    ApplyMsg msg(i, log_.CommandAt(i));
    applyCallback_(msg);
  }
  lastApplied_ = commitIndex_;
}

void Raft::TickOnElection() {
  timeElapsed_++;
  if (timeElapsed_ >= randomizedElectionTimeout_) {
    ToCandidate();  // candidate -> candidate is OK
  }
}

void Raft::TickOnHeartbeat() {
  timeElapsed_++;
  if (timeElapsed_ >= heartbeatTimeout_) {
    StartAppendEntries();
    ResetTimer();
  }
}

void Raft::SetCurrentTerm(int term) {
  currentTerm_ = term;
  storage_.PutCurrentTerm(currentTerm_);
}

void Raft::SetVotedFor(int votedFor) {
  votedFor_ = votedFor;
  storage_.PutVotedFor(votedFor_);
}

void Raft::ToFollower(int targetTerm) {
  if (role_ != kFollower) {
    char buf[128];
    snprintf(buf, sizeof(buf), "raft[%d] %s -> follower", id_, RoleString());
    LOG_DEBUG << buf;
  }

  assert(currentTerm_ <= targetTerm);

  role_ = kFollower;
  if (currentTerm_ < targetTerm) {
    SetCurrentTerm(targetTerm);
    SetVotedFor(kVotedForNull);
    votesGot_ = 0;
  }
  ResetTimer();
}

void Raft::ToCandidate() {
  if (role_ != kCandidate) {
    char buf[128];
    snprintf(buf, sizeof(buf), "raft[%d] %s -> candidate", id_, RoleString());
    LOG_DEBUG << buf;
  }

  role_ = kCandidate;
  SetCurrentTerm(currentTerm_ + 1);
  SetVotedFor(id_);  // vote myself
  votesGot_ = 1;

  if (IsStandalone()) {
    ToLeader();
  } else {
    ResetTimer();
    StartRequestVote();
  }
}

void Raft::ToLeader() {
  char buf[128];
  snprintf(buf, sizeof(buf), "raft[%d] %s -> leader", id_, RoleString());
  LOG_DEBUG << buf;

  nextIndex_.assign(peerNum_, log_.LastIndex() + 1);
  matchIndex_.assign(peerNum_, kInitialMatchIndex);
  role_ = kLeader;
  ResetTimer();
}

void Raft::OnNewInputTerm(int term) {
  if (currentTerm_ < term) {
    ToFollower(term);
  }
}

void Raft::ResetTimer() {
  timeElapsed_ = 0;
  if (role_ != kLeader)
    randomizedElectionTimeout_ = randomGen_.Generate();
}
