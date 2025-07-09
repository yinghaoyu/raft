#include <grpcpp/grpcpp.h>
#include <muduo/base/Logging.h>
#include <memory>
#include <string>
#include <thread>

#include "message.grpc.pb.h"

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using message::AppendEntriesReq;
using message::AppendEntriesRsp;
using message::RaftService;
using message::RequestVoteReq;
using message::RequestVoteRsp;

class RaftAsyncGrpcClient {
 public:
  explicit RaftAsyncGrpcClient(std::string target_ip) {
    std::shared_ptr<Channel> channel = grpc::CreateChannel(target_ip, grpc::InsecureChannelCredentials());
    stub_ = RaftService::NewStub(channel);
  }

  // 异步 RequestVote
  void AsyncRequestVote(const RequestVoteReq& req, std::function<void(const RequestVoteRsp&)> cb) {
    auto* call = new RequestVoteCall;
    call->cb = cb;
    call->response_reader = stub_->PrepareAsyncRequestVote(&call->context, req, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, static_cast<void*>(call));
  }

  // 异步 AppendEntries
  void AsyncAppendEntries(const AppendEntriesReq& req, std::function<void(const AppendEntriesRsp&)> cb) {
    auto* call = new AppendEntriesCall;
    call->cb = cb;
    call->response_reader = stub_->PrepareAsyncAppendEntries(&call->context, req, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status, static_cast<void*>(call));
  }

  // 处理异步回调
  void AsyncCompleteRpc() {
    void* got_tag;
    bool ok = false;
    while (cq_.Next(&got_tag, &ok)) {
      if (!ok)
        continue;
      BaseCall* base = static_cast<BaseCall*>(got_tag);
      base->OnReply();
      delete base;
    }
  }

 private:
  // 基类用于多态
  struct BaseCall {
    virtual ~BaseCall() = default;
    virtual void OnReply() = 0;
  };

  struct RequestVoteCall : public BaseCall {
    std::function<void(const RequestVoteRsp&)> cb;
    RequestVoteRsp reply;
    ClientContext context;
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<RequestVoteRsp>> response_reader;
    void OnReply() override {
      if (status.ok()) {
        cb(reply);
      } else {
        LOG_DEBUG << "[RequestVote] RPC failed";
      }
    }
  };

  struct AppendEntriesCall : public BaseCall {
    std::function<void(const AppendEntriesRsp&)> cb;
    AppendEntriesRsp reply;
    ClientContext context;
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<AppendEntriesRsp>> response_reader;
    void OnReply() override {
      if (status.ok()) {
        cb(reply);
      } else {
        LOG_DEBUG << "[AppendEntries] RPC failed";
      }
    }
  };

  std::unique_ptr<RaftService::Stub> stub_;
  CompletionQueue cq_;
};
