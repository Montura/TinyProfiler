#pragma once

#include <array>
#include "profiler.h"
#include "trie.h"

class MiniProfiler::Impl {
  struct StkTrace {
    uint64_t len = 0;
    std::array<uint64_t, 63> frames;
  };

  //main thread
  std::unique_ptr<std::thread> profThread;
  //shared
  std::atomic_bool shouldExit;
  //profiling thread
  std::vector<StkTrace> traces;

  Trie trie;

  static uint64_t getThreadId();
  static std::string getSymbolName(const StkTrace& trace, int64_t i) ;
  void profileFunc(uint64_t mainThreadId);

public:
  Impl();
  ~Impl();
};


