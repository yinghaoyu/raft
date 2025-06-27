#include <grpcpp/grpcpp.h>
#include <iostream>
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
    std::shared_ptr<Channel> channel =
        grpc::CreateChannel(target_ip, grpc::InsecureChannelCredentials());
    stub_ = RaftService::NewStub(channel);
  }

  // 异步 RequestVote
  void AsyncRequestVote(const RequestVoteReq& req,
                        std::function<void(const RequestVoteRsp&)> cb) {
    auto* call = new RequestVoteCall;
    call->cb = cb;
    call->response_reader =
        stub_->PrepareAsyncRequestVote(&call->context, req, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status,
                                  static_cast<void*>(call));
  }

  // 异步 AppendEntries
  void AsyncAppendEntries(const AppendEntriesReq& req,
                          std::function<void(const AppendEntriesRsp&)> cb) {
    auto* call = new AppendEntriesCall;
    call->cb = cb;
    call->response_reader =
        stub_->PrepareAsyncAppendEntries(&call->context, req, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->reply, &call->status,
                                  static_cast<void*>(call));
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
        std::cout << "[RequestVote] RPC failed" << std::endl;
      }
    }
  };

  struct AppendEntriesCall : public BaseCall {
    std::function<void(const AppendEntriesRsp&)> cb;
    AppendEntriesRsp reply;
    ClientContext context;
    Status status;
    std::unique_ptr<ClientAsyncResponseReader<AppendEntriesRsp>>
        response_reader;
    void OnReply() override {
      if (status.ok()) {
        cb(reply);
      } else {
        std::cout << "[AppendEntries] RPC failed" << std::endl;
      }
    }
  };

  std::unique_ptr<RaftService::Stub> stub_;
  CompletionQueue cq_;
};

// #pragma once

// #include "message.grpc.pb.h"
// #include "message.pb.h"

// #include <grpcpp/grpcpp.h>
// #include <jsoncpp/json/json.h>
// #include <jsoncpp/json/reader.h>
// #include <jsoncpp/json/value.h>
// #include <muduo/base/noncopyable.h>
// #include <muduo/net/InetAddress.h>
// #include <condition_variable>
// #include <queue>

// using grpc::Channel;
// using grpc::ClientContext;
// using grpc::Status;

// using message::AppendEntriesReq;
// using message::AppendEntriesRsp;
// using message::RequestVoteReq;
// using message::RequestVoteRsp;

// using message::RaftService;

// class GrpcConPool {
//  public:
//   GrpcConPool(const size_t poolSize,
//               const muduo::net::InetAddress serverAddress)
//       : poolSize_(poolSize), stopped_(false) {

//     for (size_t i = 0; i < poolSize_; ++i) {
//       std::shared_ptr<Channel> channel = grpc::CreateChannel(
//           serverAddress.toIpPort(), grpc::InsecureChannelCredentials());
//       connections_.push(RaftService::NewStub(channel));
//     }
//   }

//   ~GrpcConPool() {
//     std::lock_guard<std::mutex> lock(mutex_);
//     Close();
//     while (!connections_.empty()) {
//       connections_.pop();
//     }
//   }

//   std::unique_ptr<RaftService::Stub> getConnection() {
//     std::unique_lock<std::mutex> lock(mutex_);
//     cond_.wait(lock, [this] {
//       if (stopped_) {
//         return true;
//       }
//       return !connections_.empty();
//     });
//     if (stopped_) {
//       return nullptr;
//     }
//     auto context = std::move(connections_.front());
//     connections_.pop();
//     return context;
//   }

//   void returnConnection(std::unique_ptr<RaftService::Stub> context) {
//     std::lock_guard<std::mutex> lock(mutex_);
//     connections_.push(std::move(context));
//     cond_.notify_one();
//   }

//   void Close() {
//     stopped_ = true;
//     cond_.notify_all();
//   }

//  private:
//   size_t poolSize_;
//   std::atomic<bool> stopped_;
//   std::queue<std::unique_ptr<RaftService::Stub>> connections_;
//   std::mutex mutex_;
//   std::condition_variable cond_;
// };
