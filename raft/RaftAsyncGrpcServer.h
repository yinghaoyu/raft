#include <grpcpp/grpcpp.h>
#include <muduo/base/Logging.h>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include "Callback.h"
#include "Struct.h"
#include "message.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using message::AppendEntriesReq;
using message::AppendEntriesRsp;
using message::RaftService;
using message::RequestVoteReq;
using message::RequestVoteRsp;

// 通用基类，使用多态
class BaseCallData {
 public:
  virtual void Proceed() = 0;
  virtual ~BaseCallData() = default;
};

class RequestVoteCallData : public BaseCallData {
 public:
  using AsyncService = message::RaftService::AsyncService;
  RequestVoteCallData(raft::DoRequestVoteCallback& cb, AsyncService* service, ServerCompletionQueue* cq)
      : doRequestVote_(cb), service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
    Proceed();
  }

  void Proceed() override {
    if (status_ == CREATE) {
      status_ = PROCESS;
      service_->RequestRequestVote(&ctx_, &request_, &responder_, cq_, cq_, this);
    } else if (status_ == PROCESS) {
      new RequestVoteCallData(doRequestVote_, service_, cq_);

      raft::RequestVoteArgs args;
      args.term = request_.term();
      args.candidateId = request_.candidateid();
      args.lastLogIndex = request_.lastlogindex();
      args.lastLogTerm = request_.lastlogterm();

      doRequestVote_(args, [=](const raft::RequestVoteReply& reply) {
        reply_.set_term(reply.term);
        reply_.set_votegranted(reply.voteGranted);
        status_ = FINISH;
        responder_.Finish(reply_, Status::OK, this);
      });

    } else {
      delete this;
    }
  }

 private:
  raft::DoRequestVoteCallback doRequestVote_;

  AsyncService* service_;
  ServerCompletionQueue* cq_;
  ServerContext ctx_;
  message::RequestVoteReq request_;
  message::RequestVoteRsp reply_;
  ServerAsyncResponseWriter<message::RequestVoteRsp> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;
};

class AppendEntriesCallData : public BaseCallData {
 public:
  using AsyncService = message::RaftService::AsyncService;
  AppendEntriesCallData(raft::DoAppendEntriesCallback& cb, AsyncService* service, ServerCompletionQueue* cq)
      : doAppendEntries_(cb), service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
    Proceed();
  }

  void Proceed() override {
    if (status_ == CREATE) {
      status_ = PROCESS;
      service_->RequestAppendEntries(&ctx_, &request_, &responder_, cq_, cq_, this);
    } else if (status_ == PROCESS) {
      new AppendEntriesCallData(doAppendEntries_, service_, cq_);

      raft::AppendEntriesArgs args;
      args.term = request_.term();
      args.prevLogIndex = request_.prevlogindex();
      args.prevLogTerm = request_.prevlogterm();
      args.entries = Json::arrayValue;

      for (const auto& entry : request_.entries()) {
        Json::Value obj(Json::objectValue);
        obj["term"] = entry.term();
        obj["command"] = entry.command();
        args.entries.append(obj);
      }

      args.leaderCommit = request_.leadercommit();

      doAppendEntries_(args, [=](const raft::AppendEntriesReply& reply) {
        reply_.set_term(reply.term);
        reply_.set_success(reply.success);
        reply_.set_expectindex(reply.expectIndex);
        reply_.set_expectterm(reply.expectTerm);
        status_ = FINISH;
        responder_.Finish(reply_, Status::OK, this);
      });

    } else {
      delete this;
    }
  }

 private:
  raft::DoAppendEntriesCallback doAppendEntries_;

  AsyncService* service_;
  ServerCompletionQueue* cq_;
  ServerContext ctx_;
  message::AppendEntriesReq request_;
  message::AppendEntriesRsp reply_;
  ServerAsyncResponseWriter<message::AppendEntriesRsp> responder_;
  enum CallStatus { CREATE, PROCESS, FINISH };
  CallStatus status_;
};

class RaftAsyncGrpcServer final {
 public:
  ~RaftAsyncGrpcServer() {
    server_->Shutdown();
    cq_->Shutdown();
  }

  void Run(std::string server_address) {
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service_);
    cq_ = builder.AddCompletionQueue();
    server_ = builder.BuildAndStart();
    LOG_DEBUG << "gRPC Server listening on " << server_address;

    HandleRpcs();
  }

  void SetDoRequestVoteCallback(const raft::DoRequestVoteCallback& cb) { doRequestVote_ = cb; }

  void SetDoAppendEntriesCallback(const raft::DoAppendEntriesCallback& cb) { doAppendEntries_ = cb; }

 private:
  void HandleRpcs() {
    // 启动所有接口的监听
    new RequestVoteCallData(doRequestVote_, &service_, cq_.get());
    new AppendEntriesCallData(doAppendEntries_, &service_, cq_.get());
    void* tag;
    bool ok;
    while (cq_->Next(&tag, &ok)) {
      if (ok) {
        static_cast<BaseCallData*>(tag)->Proceed();
      } else {
        delete static_cast<BaseCallData*>(tag);
      }
    }
  }

  raft::DoRequestVoteCallback doRequestVote_;
  raft::DoAppendEntriesCallback doAppendEntries_;

  std::unique_ptr<ServerCompletionQueue> cq_;
  message::RaftService::AsyncService service_;
  std::unique_ptr<Server> server_;
};
