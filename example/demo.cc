

#include <assert.h>
#include <muduo/base/Logging.h>
#include <chrono>

#include "raft/Node.h"

using namespace std::chrono_literals;

void usage() {
  printf("usage: ./raft id address1 address2...");
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  if (argc < 3)
    usage();

  muduo::Logger::setLogLevel(muduo::Logger::DEBUG);
  muduo::Logger::setFlushInterval(1);

  int id = std::stoi(argv[1]);

  std::vector<muduo::net::InetAddress> peerAddresses;

  if (id + 2 >= argc) {
    usage();
  }

  for (int i = 2; i < argc; i++) {
    peerAddresses.emplace_back(std::stoi(argv[i]));
  }

  raft::Config config;
  config.id = id;
  config.storagePath = "./raft." + std::to_string(id);
  config.heartbeatTimeout = 1;
  config.electionTimeout = 5;
  config.timeUnit = 100ms;
  config.serverAddress = peerAddresses[id];
  config.peerAddresses = peerAddresses;
  config.applyCallback = [](const raft::ApplyMsg& msg) {
    assert(msg.command.asString() == "raft example");
  };
  config.snapshotCallback = [](const Json::Value& snapshot) {
    LOG_FATAL << "not implemented yet";
  };

  raft::Node raftNode(config);

  muduo::net::EventLoop loop;
  loop.runEvery(1, [&]() {
    auto ret = raftNode.GetState();
    if (ret.isLeader) {
      raftNode.Propose(Json::Value("raft example"));
    }
  });

  raftNode.Start();
  LOG_DEBUG << "raftNode start finish";
  loop.loop();

  return 0;
}