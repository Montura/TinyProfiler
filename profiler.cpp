#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <functional>

#include "profiler.h"
#include "profiler_impl.h"

MiniProfiler::MiniProfiler() {
  impl = std::make_unique<Impl>();
}

MiniProfiler::~MiniProfiler() {
  impl.reset();
}

MiniProfiler::Impl::Impl() {
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
    for (int64_t i = trace.len - 1; i >= 0; --i) {
      std::string symbolName = getSymbolName(trace, i);
      path.emplace_back(symbolName);
    }
    trie.insert(path);
  }

  trie.dumpSamples(0, 0, traces.size());
}

#ifdef _WINDOWS
  #include <windows.h>
  #include <dbghelp.h>

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
      // Dont' use heap during between suspending and resuming thread! Deadlock is possible!
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
          if (!ok) {
            break;
          }

          trace.frames[trace.len++] = stackFrame.AddrPC.Offset;
        }
      }
      ResumeThread(mainThread);

      traces.push_back(trace);
    }

    CloseHandle(mainThread);
  }

  std::string MiniProfiler::Impl::getSymbolName(const StkTrace& trace, int64_t i) {
    const uint32_t charBufferLen = 1024;
    union {
      SYMBOL_INFO symbol;
      char symbolInfoName[sizeof(SYMBOL_INFO) + charBufferLen];
    };
    symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.MaxNameLen = charBufferLen;
    DWORD64 displ;
    BOOL ok = SymFromAddr(GetCurrentProcess(), trace.frames[i], &displ, &symbol);
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

  #include <unistd.h>
  #include <mach/thread_act.h>

  uint64_t MiniProfiler::Impl::getThreadId() {
    return reinterpret_cast<uint64_t>(pthread_self());
  }

  void MiniProfiler::Impl::profileFunc(uint64_t mainThreadId) {
    while (!shouldExit.load()) {
      //wait 1 ms
      sleep(1);

      StkTrace trace = {0};
      kern_return_t threadSuspend = thread_suspend(mainThreadId);
      {

        void* addr = pthread_get_stackaddr_np((pthread_t) mainThreadId);
        size_t size = pthread_get_stacksize_np((pthread_t) mainThreadId);
        printf("addr=%p size=%zx\n", addr, size);

//        thread_get
        pthread_attr_t attr;
        pthread_attr_getstack(&attr, &addr, &size);



      }
      kern_return_t threadResume = thread_resume(mainThreadId);

      traces.push_back(trace);
    }
}

  std::string MiniProfiler::Impl::getSymbolName(const StkTrace& trace, int64_t i) {
    return "";
  }
#else

#endif