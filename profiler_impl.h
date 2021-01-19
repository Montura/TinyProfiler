#pragma once

#include "profiler.h"

class MiniProfiler::Impl {
  struct StkTrace {
    uint64_t cnt = 0;
    uint64_t arr[63]{};
  };

  //main thread
  std::unique_ptr<std::thread> profThread;
  //shared
  std::atomic_bool shouldExit;
  //profiling thread
  std::vector<StkTrace> traces;

  static uint64_t getThreadId();
  static std::string getSymbolName(const StkTrace& trace, int64_t i) ;
  void profileFunc(uint64_t mainThreadId);

  struct Comparator {
    static bool compare(std::pair<std::string, size_t> const& a, std::pair<std::string, size_t> const& b) {
      return a.second < b.second;
    }
  };

  std::vector<std::map<std::string, size_t>> trie;
  std::vector<int> samples;

  void dumpSamples(size_t u, int indent);

public:
  Impl();
  ~Impl();
};


