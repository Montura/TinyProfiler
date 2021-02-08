#include <string>
#include <iostream>
#include <algorithm>

#include "../include/trie.h"

Trie::Trie() : trie(1), samples(1) {}

void Trie::insert(const std::vector<std::string>& path) {
  size_t v = 0;
  samples[v] = 1;
  for (const auto& str : path) {
    if (trie[v].edges.count(str) == 0) {
      trie[v].edges[str] = trie.size();
//      printf("v = %zu, samples[v] = %d, trie[v][str] = %zu, str = %s\n",
//             v, samples[v], trie[v].edges[str], str.c_str());
      trie.emplace_back();
      samples.push_back(0);
    }
    v = trie[v].edges[str];
    samples[v]++;
  }
}

void Trie::dumpSamples(size_t u, uint32_t indentCount, uint64_t totalTracesCount) {
  std::vector<std::pair<std::string, size_t>> sons(trie[u].edges.begin(), trie[u].edges.end());

  std::sort(sons.begin(), sons.end(), SamplesComparator::compare);

  for (const auto& pKV : sons) {
    size_t v = pKV.second;
    double frac = (samples[v] + 0.0) / totalTracesCount;
    if (frac < 0.02) {
      continue;
    }

    printf("%*s%4.1lf%%  : %s\n", indentCount, "", frac * 100.0, pKV.first.c_str());

    dumpSamples(v, indentCount + 1, totalTracesCount);
  }
}

