
#ifndef RAFT_CALLBACK_H
#define RAFT_CALLBACK_H

#include <jsoncpp/json/value.h>
#include <functional>

namespace raft {

struct RequestVoteArgs;
struct RequestVoteReply;

struct AppendEntriesArgs;
struct AppendEntriesReply;

struct ApplyMsg;

typedef std::function<void(const RequestVoteReply&)> RequestVoteDoneCallback;
typedef std::function<void(const AppendEntriesReply&)> AppendEntriesDoneCallback;
typedef std::function<void(const RequestVoteArgs&, const RequestVoteDoneCallback&)> DoRequestVoteCallback;
typedef std::function<void(const AppendEntriesArgs&, const AppendEntriesDoneCallback&)> DoAppendEntriesCallback;
typedef std::function<void(int, const RequestVoteArgs&, const RequestVoteReply&)> RequestVoteReplyCallback;
typedef std::function<void(int, const AppendEntriesArgs&, const AppendEntriesReply&)> AppendEntriesReplyCallback;
typedef std::function<void(const ApplyMsg&)> ApplyCallback;
typedef std::function<void(const Json::Value&)> SnapshotCallback;

}  // namespace raft

#endif  // RAFT_CALLBACK_H
