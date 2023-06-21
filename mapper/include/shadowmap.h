#pragma once
#ifndef SHADOW_MAP_H_
#define SHADOW_MAP_H_
#include "llvm/Support/raw_ostream.h"
#include <functional>
#include <limits>
#include <string>
#include <vector>

/// @brief Read a bit vector from a file
/// @param BitSize expected size of the bit vector
/// @param FileName file to read from
/// @return the read bit vector
/// @note Aborts if file does not contain enough bits
std::vector<bool> readBitVector(size_t BitSize, const std::string &FileName);

/// @brief Convenience function to read multiple bit vectors from multiple files
/// @param BitSize expected size of the bit vectors
/// @param FileNames files to read from
/// @return vector of read bit vectors
std::vector<std::vector<bool>>
readBitVectors(size_t BitSize, const std::vector<std::string> &FileNames);

/// @brief Write a bit vector to a file
/// @param Vec bit vector to write
/// @param FileName file to write to
/// @return whether the operation succeeded
bool writeBitVector(const std::vector<bool> &Vec, const std::string &FileName);

/// @brief Do bitwise operations on multiple shadow maps.
/// @param Maps Shadow maps to do operations on
/// @param Op Operator (accumulated bit OP current bit = result)
/// @return Result of the operation
std::vector<bool> doMapOp(const std::vector<std::vector<bool>> &Maps,
                          std::function<bool(bool, bool)> Op);

class MapStatPrinter {
  typedef std::tuple<std::string, std::string, size_t, size_t> StatTy;

  std::vector<StatTy> Stats;
  size_t MaxFilenameLen = 0;
  size_t MaxTableSize = 0;
  std::string Description = "";
  size_t MaxDescLen = 0;
  size_t CoverageSum = 0;
  size_t Limit = std::numeric_limits<size_t>::max();
  size_t StatsPrinted = 0;

public:
  enum SortTy { None, Asc, Desc };

  /// @brief Create printer with default row description
  MapStatPrinter(const std::string &Description)
      : Description(Description), MaxDescLen(Description.size()) {}
  /// @brief Create printer without default row description
  MapStatPrinter() = default;

  /// @brief Get coverage of a shadow map
  /// @param Map shadow map
  /// @return number of bits covered in given shadow map
  static inline size_t getIdxCovered(const std::vector<bool> &Map) {
    return std::count(Map.begin(), Map.end(), false);
  }

  /// @brief Set default row description
  /// @param Desc row description
  /// @note Do this before adding stats.
  void setRowDescription(const std::string &Desc);

  /// @brief Print all added stats and clear the stored stats.
  /// @note Unsets any existing limit.
  void print();

  // Functions for adding stats to print

  void addFile(const std::string &Filename, const std::vector<bool> &Map);
  void addFile(const std::string &Filename, size_t Covered, size_t TableSize);
  // Directly read shadow map using given filename and record stats.
  void addFile(const std::string &Filename, size_t TableSize);
  void addStat(size_t Covered, size_t TableSize);
  void addStat(const std::string &Desc, size_t Covered, size_t TableSize);
  void addMap(const std::vector<bool> &Map);

  /// @brief Print a summary row of the statistics
  /// @param Desc description
  /// @param Covered number of covered indices
  /// @param TableSize total number of indices
  /// @param alignToDesc align the "Sum: " prefix to description column instead
  /// of file
  /// @note The printed row is not bounded by the limit
  void summarize(const std::string &Desc, size_t Covered, size_t TableSize,
                 bool alignToDesc = false);

  /// @brief Print a summary row of the statistics
  /// @param Desc description
  /// @param Map shadow map to print coverage of
  /// @param alignToDesc align the "Sum: " prefix to description column instead
  /// of file
  /// @note The printed row is not bounded by the limit
  void summarize(const std::string &Desc, const std::vector<bool> &Map,
                 bool alignToDesc = false);

  /// @brief Print the sum of the statistics
  /// @param alignToDesc align the "Sum: " prefix to description column instead
  /// of file
  /// @note The printed row is not bounded by the limit
  void sum(bool alignToDesc = true);

  /// @brief Sort the stat entries in ascending order by coverage
  void asc();

  /// @brief Sort the stat entries in descending order by coverage
  void desc();

  /// @brief Sort the stat entries
  /// @param S sort order
  void sort(SortTy S);

  /// @brief Set maximum entries allowed. Entries beyond the limit will not be
  /// added.
  /// @param L maximum number of entries allowed
  /// @note Summary entries do not count towards the limit.
  void limit(size_t L);

  /// @brief Check if the entry limit has been reached
  /// @return true if printer cannot print any more entries.
  bool atLimit() const;

private:
  std::string format(const StatTy &Stat) const;
  void addStat(const std::string &Filename, const std::string &Desc,
               size_t Covered, size_t TableSize);
};

#endif // SHADOW_MAP_H_