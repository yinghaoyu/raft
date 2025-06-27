#ifndef RAFT_LOG_H
#define RAFT_LOG_H

#include <jsoncpp/json/value.h>
#include <memory>
#include <vector>

#include <muduo/base/noncopyable.h>
#include "Storage.h"
#include "Struct.h"

namespace raft {

class Log : muduo::noncopyable {
 public:
  explicit Log(Storage* storage);

  int FirstIndex() const { return firstIndex_; }

  int FirstTerm() const { return log_[firstIndex_].term; }

  int LastIndex() const { return lastIndex_; }

  int LastTerm() const { return log_[lastIndex_].term; }

  int TermAt(int index) const { return log_[index].term; }

  const Json::Value& CommandAt(int index) const { return log_[index].command; }

  IndexAndTerm LastIndexInTerm(int startIndex, int term) const;

  bool IsUpToDate(int index, int term) const;

  void Append(int term, const Json::Value& command);

  void Overwrite(int firstIndex, const Json::Value& entries);

  Json::Value GetEntriesAsJson(int firstIndex, int maxEntries) const;

  bool Contain(int index, int term) const;

 private:
  Json::Value GetEntryAsJson(int index) const;

  void PutEntryFromJson(const Json::Value& entry);

  struct Entry {
    Entry(int term_, const Json::Value& command_)
        : term(term_), command(command_) {}

    Entry() : term(0), command(Json::nullValue) {}

    int term;
    Json::Value command;  // from raft user or raft peer
  };

  Storage* storage_;
  int firstIndex_;
  int lastIndex_;
  std::vector<Entry> log_;
};

}  // namespace raft

#endif  // RAFT_LOG_H
