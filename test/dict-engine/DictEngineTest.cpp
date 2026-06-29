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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "util/Dictionary.h"

namespace {

const std::string kFixtureStem = std::string(DICT_FIXTURE_DIR) + "/en-es/en-es";
// 1500 synthetic "fuzzy_word_NNNNN" headwords with a real .idx.oft (~47 pages),
// so lookups for words far past offset 0 exercise the OFT page-boundary binary
// search in Dictionary::findSimilar (a 6-page dict like en-es always clamps the
// scan window back to offset 0).
const std::string kFuzzyStem = std::string(DICT_FIXTURE_DIR) + "/fuzzy-oft/fuzzy-oft";

// Write a dictionary.bin pointing at `stem` into a fresh temp cache dir (named
// after `tag`, under GoogleTest's managed temp directory) and return that dir
// path (suitable as findSimilar's cachePath argument).
std::string cacheDirPointingAt(const std::string& stem, const std::string& tag) {
  const std::string cacheDir = testing::TempDir() + tag;
  std::filesystem::create_directories(cacheDir);
  std::ofstream bin(cacheDir + "/dictionary.bin", std::ios::binary | std::ios::trunc);
  bin << stem;
  return cacheDir;
}

std::string cacheDirPointingAtFixture() { return cacheDirPointingAt(kFixtureStem, "dict_engine_with_dict"); }

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

// The OFT binary search snaps the scan window back to the page boundary that
// contains scanStart. For a word deep in a multi-page dictionary, scanStart is
// well past offset 0, so the candidates findSimilar returns are exactly those
// inside the ±7-page window around the target's page. We assert every suggestion
// is numerically clustered around the target headword: a broken snap (e.g.
// landing on the wrong page, or always offset 0) would scan a different region
// and return words from elsewhere in the index, failing this band check.

// Parse the trailing NNNNN of a "fuzzy_word_NNNNN" headword (-1 if malformed).
int fuzzyOrdinal(const std::string& w) {
  const std::string prefix = "fuzzy_word_";
  if (w.size() <= prefix.size() || w.compare(0, prefix.size(), prefix) != 0) return -1;
  return std::atoi(w.c_str() + prefix.size());
}

void expectSuggestionsClusteredNear(int target) {
  const std::string cache = cacheDirPointingAt(kFuzzyStem, "dict_engine_fuzzy");
  char query[32];
  std::snprintf(query, sizeof(query), "fuzzy_word_%05d", target);
  const auto results = Dictionary::findSimilar(query, 6, cache.c_str());
  ASSERT_FALSE(results.empty()) << "expected suggestions near " << query;
  // Window is ~±7 pages of 32 entries (~±224 ordinals) around the target page;
  // ±400 is a generous bound that still excludes a wrong-region (e.g. offset-0) scan.
  for (const auto& r : results) {
    const int ord = fuzzyOrdinal(r);
    ASSERT_GE(ord, 0) << "unexpected headword form: " << r;
    EXPECT_LE(std::abs(ord - target), 400) << "suggestion " << r << " is outside the scan window around " << target;
  }
}

TEST(DictFindSimilarOft, SuggestionsClusteredDeepInMultiPageDict) {
  expectSuggestionsClusteredNear(1400);  // page ~43 of ~47
}

TEST(DictFindSimilarOft, SuggestionsClusteredMidMultiPageDict) {
  expectSuggestionsClusteredNear(750);  // page ~23 of ~47
}

// getStemVariants strips inflectional/derivational suffixes (and a few prefixes)
// to produce candidate base forms. These tests pin the behavior so the
// allocation-reduction refactor (reusable scratch buffer + inline dedup) stays
// equivalent to the previous substr/concat implementation.

TEST(DictStemVariants, TooShortYieldsNothing) { EXPECT_TRUE(Dictionary::getStemVariants("go").empty()); }

TEST(DictStemVariants, RegularPlural) { EXPECT_TRUE(contains(Dictionary::getStemVariants("cats"), "cat")); }

TEST(DictStemVariants, IesPlural) {
  const auto v = Dictionary::getStemVariants("parties");
  EXPECT_TRUE(contains(v, "party"));
}

TEST(DictStemVariants, PastTense) { EXPECT_TRUE(contains(Dictionary::getStemVariants("walked"), "walk")); }

TEST(DictStemVariants, ProgressiveWithDoubledConsonant) {
  const auto v = Dictionary::getStemVariants("running");
  EXPECT_TRUE(contains(v, "run"));
}

TEST(DictStemVariants, PrefixRemoval) { EXPECT_TRUE(contains(Dictionary::getStemVariants("unhappy"), "happy")); }

TEST(DictStemVariants, NoDuplicates) {
  const auto v = Dictionary::getStemVariants("blesses");
  std::vector<std::string> sorted = v;
  std::sort(sorted.begin(), sorted.end());
  EXPECT_EQ(std::adjacent_find(sorted.begin(), sorted.end()), sorted.end())
      << "getStemVariants must not return duplicate variants";
}
