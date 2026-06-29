// Unit tests for the Dictionary engine's fuzzy lookup (Dictionary::findSimilar),
// run against the en-es fixture (test/dictionaries/en-es), whose index holds a
// few hundred ordinary English headwords — a realistic corpus for fuzzy matching.
// Dictionary.cpp reads the index / offset-table files via the HalStorage host stub.
//
// findSimilar(word, maxResults, cachePath) resolves the dictionary location by
// reading "<cachePath>/dictionary.bin", whose contents are the dictionary file
// stem (folder + base name, no extension). Each test writes such a dictionary.bin
// into a temp cache dir pointing at the fixture stem.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "util/Dictionary.h"

namespace {

const std::string kFixtureStem = std::string(DICT_FIXTURE_DIR) + "/en-es/en-es";

// Write a dictionary.bin pointing at the fixture stem into a fresh temp cache dir
// (under GoogleTest's managed temp directory) and return that dir path (suitable
// as findSimilar's cachePath argument).
std::string cacheDirPointingAtFixture() {
  const std::string cacheDir = testing::TempDir() + "dict_engine_with_dict";
  std::filesystem::create_directories(cacheDir);
  std::ofstream bin(cacheDir + "/dictionary.bin", std::ios::binary | std::ios::trunc);
  bin << kFixtureStem;
  return cacheDir;
}

// A temp cache dir with no dictionary.bin (forces the global fallback, which is
// absent on the host).
std::string emptyCacheDir() {
  const std::string cacheDir = testing::TempDir() + "dict_engine_empty";
  std::filesystem::create_directories(cacheDir);
  std::error_code ec;
  std::filesystem::remove(cacheDir + "/dictionary.bin", ec);
  return cacheDir;
}

bool contains(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

}  // namespace

TEST(DictFindSimilar, ReturnsExactHeadwordForSingleTypo) {
  const std::string cache = cacheDirPointingAtFixture();
  // "aple" is one insertion away from the headword "apple".
  const auto results = Dictionary::findSimilar("aple", 6, cache.c_str());
  EXPECT_TRUE(contains(results, "apple")) << "expected 'apple' among suggestions";
}

TEST(DictFindSimilar, ReturnsMultipleNearbyHeadwords) {
  const std::string cache = cacheDirPointingAtFixture();
  // "lood" is within edit distance 2 of the _oo_ cluster: food/good (1),
  // book/cook/door (2).
  const auto results = Dictionary::findSimilar("lood", 6, cache.c_str());
  EXPECT_TRUE(contains(results, "food"));
  EXPECT_TRUE(contains(results, "good"));
  EXPECT_TRUE(contains(results, "book"));
  EXPECT_TRUE(contains(results, "cook"));
}

TEST(DictFindSimilar, RespectsMaxResults) {
  const std::string cache = cacheDirPointingAtFixture();
  const auto results = Dictionary::findSimilar("lood", 2, cache.c_str());
  EXPECT_LE(results.size(), 2u);
}

TEST(DictFindSimilar, SortsClosestFirst) {
  const std::string cache = cacheDirPointingAtFixture();
  // food/good are distance 1 from "lood"; book/cook/door are distance 2. The
  // closest match must come first.
  const auto results = Dictionary::findSimilar("lood", 6, cache.c_str());
  ASSERT_FALSE(results.empty());
  EXPECT_TRUE(results.front() == "food" || results.front() == "good")
      << "closest match should sort first, got: " << results.front();
}

TEST(DictFindSimilar, ReturnsEmptyForFarWord) {
  const std::string cache = cacheDirPointingAtFixture();
  const auto results = Dictionary::findSimilar("zzzzzzzz", 6, cache.c_str());
  EXPECT_TRUE(results.empty());
}

TEST(DictFindSimilar, ReturnsEmptyWhenNoDictionary) {
  const std::string cache = emptyCacheDir();
  const auto results = Dictionary::findSimilar("grace", 6, cache.c_str());
  EXPECT_TRUE(results.empty());
}
