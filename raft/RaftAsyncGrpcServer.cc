// #include "RaftServiceImpl.h"
// #include "Struct.h"

// #include <jsoncpp/json/json.h>
// #include <jsoncpp/json/reader.h>
// #include <jsoncpp/json/value.h>

// RaftServiceImpl::RaftServiceImpl() {}

// Status RaftServiceImpl::RequestVote(ServerContext* context,
//                                     const RequestVoteReq* request,
//                                     RequestVoteRsp* response) {
//   raft::RequestVoteArgs args;
//   args.term = request->term();
//   args.candidateId = request->candidateid();
//   args.lastLogIndex = request->lastlogindex();
//   args.lastLogTerm = request->lastlogterm();

//   doRequestVote_(args, [=](const raft::RequestVoteReply& reply) {
//     response->set_term(reply.term);
//     response->set_votegranted(reply.voteGranted);
//   });

//   return Status::OK;
// }

// Status RaftServiceImpl::AppendEntries(ServerContext* context,
//                                       const AppendEntriesReq* request,
//                                       AppendEntriesRsp* response) {
//   return Status::OK;
// }