add_library(raft STATIC
        Raft.cc Raft.h
        RaftPeer.cc RaftPeer.h
        RaftAsyncGrpcServer.h RaftAsyncGrpcServer.cc
        message.grpc.pb.h message.grpc.pb.cc
        message.pb.h message.pb.cc
        Log.h Log.cc
        Storage.cc Storage.h
        Node.cc Node.h
        Config.h Struct.h Random.h Callback.h)
target_link_libraries(raft jsoncpp leveldb muduo_base muduo_net protobuf gRPC::grpc++_reflection gRPC::grpc++)