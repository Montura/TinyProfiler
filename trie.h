#pragma once

#include <map>
#include <vector>

class Trie {
  struct Vertex {
    std::map<std::string, size_t> edges;
  };

  std::vector<Vertex> trie;
  std::vector<int> samples;

public:
  explicit Trie();

  void dumpSamples(size_t u, uint32_t indentCount, uint64_t i);

  struct SamplesComparator {
    static bool compare(std::pair<std::string, size_t> const& a, std::pair<std::string, size_t> const& b) {
      return a.second < b.second;
    }
  };

  void insert(const std::vector<std::string>& path);
};