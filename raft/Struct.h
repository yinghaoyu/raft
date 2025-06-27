#ifndef RAFT_STRUCT_H
#define RAFT_STRUCT_H

#include <jsoncpp/json/value.h>

namespace raft {

struct RequestVoteArgs {
  int term = -1;
  int candidateId = -1;
  int lastLogIndex = -1;
  int lastLogTerm = -1;
};

struct RequestVoteReply {
  int term = -1;
  bool voteGranted = false;
};

struct AppendEntriesArgs {
  int term = -1;
  int prevLogIndex = -1;
  int prevLogTerm = -1;
  Json::Value entries;
  int leaderCommit = -1;
};

struct AppendEntriesReply {
  int term = -1;
  bool success = false;
  int expectIndex = -1;
  int expectTerm = -1;
};

struct ProposeResult {
  int expectIndex = -1;
  int currentTerm = -1;
  bool isLeader = false;
};

struct RaftState {
  int currentTerm = -1;
  bool isLeader = false;
};

struct IndexAndTerm {
  int index;
  int term;
};

struct ApplyMsg {
  ApplyMsg(int index_, const Json::Value& command_)
      : index(index_), command(command_) {}

  int index;
  Json::Value command;
};

}  // namespace raft

#endif  // RAFT_STRUCT_H
