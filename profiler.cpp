#include "profiler.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <string>
#include <functional>

#include "profiler_impl.h"

MiniProfiler::MiniProfiler() {
  impl = std::make_unique<Impl>();
}

MiniProfiler::~MiniProfiler() {
  impl.reset();
}

MiniProfiler::Impl::Impl() : samples(1), trie(1) {
  uint64_t threadId = getThreadId();
  profThread = std::make_unique<std::thread>([this, threadId]() {
    profileFunc(threadId);
  });
  shouldExit = false;
  //*profThread = std::thread(...);
}

MiniProfiler::Impl::~Impl() {
  shouldExit.store(true);
  profThread->join();
  profThread.reset();

  for (const auto& trace : traces) {
    std::vector<std::string> path;
    for (int64_t i = trace.cnt - 1; i >= 0; i--) {
      std::string symbolName = getSymbolName(trace, i);
      path.emplace_back(symbolName);
    }
    size_t v = 0;
    samples[v]++;
    for (const auto& s : path) {
      if (trie[v].count(s) == 0) {
        trie[v][s] = trie.size();
        trie.emplace_back();
        samples.push_back(0);
      }
      v = trie[v][s];
      samples[v]++;
    }
  }

  dumpSamples(0, 0);
}

void MiniProfiler::Impl::dumpSamples(size_t u, int indent) {
  std::vector<std::pair<std::string, size_t>> sons(trie[u].begin(), trie[u].end());

  sort(sons.begin(), sons.end(), Comparator::compare);

  for (const auto& pKV : sons) {
    size_t v = pKV.second;
    double frac = (samples[v] + 0.0) / traces.size();
    if (frac < 0.03)
      continue;

    for (int i = 0; i < indent; i++)
      printf("  ");
    printf("%4.1lf%%  : %s\n", frac * 100.0, pKV.first.c_str());

    dumpSamples(v, indent + 1);
  }
}

#ifdef _WINDOWS

  #include <windows.h>
  #include <dbghelp.h>

  #pragma comment(lib, "dbgHelp.lib")

//.h --- for compiler
//.lib/.a --- for linker

  uint64_t MiniProfiler::Impl::getThreadId() {
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    return GetCurrentThreadId();
  }

  void MiniProfiler::Impl::profileFunc(uint64_t mainThreadId) {
    HANDLE mainThread = OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD) mainThreadId);

    while (!shouldExit.load()) {
      //wait 1 ms
      Sleep(1);

      StkTrace trace = {0};
      SuspendThread(mainThread);
      {
        //CaptureStackBackTrace -- this thread  (see e.g. Heapy)

        CONTEXT context;
        context.ContextFlags = CONTEXT_ALL;
        GetThreadContext(mainThread, &context);

        STACKFRAME64 stackFrame = {0};
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrPC.Offset = context.Rip;
        stackFrame.AddrStack.Offset = context.Rsp;
        stackFrame.AddrFrame.Offset = context.Rbp;

        for (int frame = 0; frame < 63; frame++) {
          BOOL ok = StackWalk64(
              IMAGE_FILE_MACHINE_AMD64,
              GetCurrentProcess(),
              mainThread,
              &stackFrame,
              &context,
              NULL,
              SymFunctionTableAccess64,
              SymGetModuleBase64,
              NULL
          );
          if (!ok)
            break;

          trace.arr[trace.cnt++] = stackFrame.AddrPC.Offset;
        }
      }
      ResumeThread(mainThread);

      traces.push_back(trace);
    }

    CloseHandle(mainThread);
  }

  std::string MiniProfiler::Impl::getSymbolName(const StkTrace& trace, int64_t i) {
    union {
      SYMBOL_INFO symbol;
      char trash[sizeof(SYMBOL_INFO) + 1024];
    };
    symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.MaxNameLen = 1024;
    DWORD64 displ;
    BOOL ok = SymFromAddr(GetCurrentProcess(), trace.arr[i], &displ, &symbol);
    return std::string(symbol.Name);
  }

__declspec( dllexport ) MiniProfiler* CreateMiniProfiler() {
  return new MiniProfiler();
}

__declspec( dllexport ) void DestroyMiniProfiler(MiniProfiler* pC) {
  delete pC;
  pC = nullptr;
}

#elif __linux__

#include <unistd.h>
#include <csignal>

static void cb_sig(int signal) {
  switch(signal) {
    case SIGUSR1:
      pause();
      break;
    case SIGUSR2:
      break;
  }
}

uint64_t MiniProfiler::Impl::getThreadId() {
//  sigemptyset(&act.sa_mask);
//  act.sa_flags = 0;
//  act.sa_handler = cb_sig;

  return pthread_self();
}

void MiniProfiler::Impl::profileFunc(uint64_t mainThreadId) {

  while (!shouldExit.load()) {
    //wait 1 ms
    sleep(1);

    StkTrace trace = {0};
    pthread_detach(mainThreadId);
    // /* Send signal SIGNO to the given thread. */
    pthread_kill(mainThreadId, SIGSTOP);
//    pthread_kill(mainThreadId, SIGUSR1);
    {
      pthread_attr_t attr;
      pthread_getattr_np(mainThreadId, &attr);

      //CaptureStackBackTrace -- this thread  (see e.g. Heapy)
      void* stackAddr = nullptr;
      size_t stackSize = 0;
      const int rc = pthread_attr_getstack(&attr, &stackAddr, &stackSize);
      if (rc) {
        printf("Can't get thread stack, tID = %lld", mainThreadId);
      }
      printf("Thread %ld: stack size = %li bytes \n", mainThreadId, stackSize);
//      CONTEXT context;
//      context.ContextFlags = CONTEXT_ALL;
//      GetThreadContext(mainThread, &context);
//
//      STACKFRAME64 stackFrame = {0};
//      stackFrame.AddrPC.Mode = AddrModeFlat;
//      stackFrame.AddrStack.Mode = AddrModeFlat;
//      stackFrame.AddrFrame.Mode = AddrModeFlat;
//      stackFrame.AddrPC.Offset = context.Rip;
//      stackFrame.AddrStack.Offset = context.Rsp;
//      stackFrame.AddrFrame.Offset = context.Rbp;

      for (int frame = 0; frame < 63; frame++) {
//        BOOL ok = StackWalk64(
//            IMAGE_FILE_MACHINE_AMD64,
//            GetCurrentProcess(),
//            mainThread,
//            &stackFrame,
//            &context,
//            NULL,
//            SymFunctionTableAccess64,
//            SymGetModuleBase64,
//            NULL
//        );
//        if (!ok)
//          break;

//        trace.arr[trace.cnt++] = stackFrame.AddrPC.Offset;
      }
    }
//    pthread_kill(mainThreadId, SIGUSR2);
    pthread_kill(mainThreadId, SIGCONT);
    traces.push_back(trace);
  }
}

std::string MiniProfiler::Impl::getSymbolName(const StkTrace &trace, int64_t i) {
  return "";
}

#elif __APPLE__
  uint64_t MiniProfiler::Impl::getThreadId() {
    return 0;
  }

  void MiniProfiler::Impl::profileFunc(uint64_t mainThreadId) {}

  std::string MiniProfiler::Impl::getSymbolName(const StkTrace& trace, int64_t i) {
    return "";
  }
#else

#endif