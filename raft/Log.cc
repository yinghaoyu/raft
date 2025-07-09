#include <leveldb/db.h>

#include "Log.h"

using namespace raft;

Log::Log(Storage* storage) : storage_(storage), firstIndex_(storage->GetFirstIndex()), lastIndex_(storage->GetLastIndex()) {
  assert(firstIndex_ <= lastIndex_);
  size_t entryNum = lastIndex_ - firstIndex_ + 1;
  log_.reserve(entryNum);
  for (auto& entry : storage->GetEntries()) {
    PutEntryFromJson(entry);
  }

  assert(entryNum == log_.size());
}

IndexAndTerm Log::LastIndexInTerm(int startIndex, int term) const {
  int index = std::min(startIndex, lastIndex_);
  for (; index >= FirstIndex(); index--) {
    if (TermAt(index) <= term)
      break;
  }
  return {index, TermAt(index)};
}

bool Log::IsUpToDate(int index, int term) const {
  int lastLogTerm_ = LastTerm();
  if (lastLogTerm_ != term)
    return lastLogTerm_ < term;
  return lastIndex_ <= index;
}

void Log::Append(int term, const Json::Value& command) {
  log_.emplace_back(term, command);
  lastIndex_++;

  auto entry = GetEntryAsJson(lastIndex_);
  storage_->PrepareEntry(lastIndex_, entry);
  storage_->PutPreparedEntries();
  storage_->PutLastIndex(lastIndex_);
}

void Log::Overwrite(int firstIndex, const Json::Value& entries) {
  assert(firstIndex <= lastIndex_ + 1);

  log_.resize(firstIndex);
  for (const Json::Value& entry : entries) {
    PutEntryFromJson(entry);
    storage_->PrepareEntry(firstIndex++, entry);
  }
  if (entries.size() > 0) {
    storage_->PutPreparedEntries();
  }
  lastIndex_ = static_cast<int>(log_.size()) - 1;
  storage_->PutLastIndex(lastIndex_);
}

Json::Value Log::GetEntriesAsJson(int firstIndex, int maxEntries) const {
  Json::Value entries(Json::arrayValue);

  int lastIndex = std::min(lastIndex_, firstIndex + maxEntries - 1);
  for (int i = firstIndex; i <= lastIndex; i++) {
    auto element = GetEntryAsJson(i);
    entries.append(element);
  }
  return entries;
}

bool Log::Contain(int index, int term) const {
  if (index > lastIndex_)
    return false;
  return log_[index].term == term;
}

Json::Value Log::GetEntryAsJson(int index) const {
  Json::Value entry(Json::objectValue);
  entry["term"] = log_[index].term;
  entry["command"] = log_[index].command;
  return entry;
}

void Log::PutEntryFromJson(const Json::Value& entry) {
  int term = entry["term"].asInt();
  auto& command = entry["command"];
  log_.emplace_back(term, command);
}
