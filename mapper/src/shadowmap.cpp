#include "shadowmap.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <climits>
#include <fstream>
#include <sstream>

std::vector<bool> readShadowMap(size_t MapBitSize,
                                const std::string &FileName) {
  std::ifstream ShadowMapIfs(FileName, std::ios::binary);
  std::vector<bool> ShadowMap;
  ShadowMap.reserve(MapBitSize);

  char Data;
  // This size check lets us know later if there are more data in the shadow map
  while (ShadowMap.size() < MapBitSize && ShadowMapIfs.read(&Data, 1)) {
    // Check size to ensure we are pushing the right number of bits. We can't
    // know if the table size incorrectly includes padding bits at the end.
    for (int I = 0; I < CHAR_BIT && ShadowMap.size() < MapBitSize; I++) {
      // NOTE: 1 bit in shadow map means MT index not covered; reading as is
      // here
      ShadowMap.push_back(((Data >> I) & 1) != 0);
    }
  }
  if (ShadowMapIfs.read(&Data, 1)) {
    std::stringstream Message;
    Message << "Expected map size of " << MapBitSize << ", but " << FileName
            << " has more.\n";
    llvm::report_fatal_error(Message.str());
  }
  if (ShadowMap.size() != MapBitSize) {
    std::stringstream Message;
    Message << "Expected map size of " << MapBitSize << ", but got "
            << ShadowMap.size() << " in " << FileName << ".\n";
    llvm::report_fatal_error(Message.str());
  }
  return ShadowMap;
}

std::vector<std::vector<bool>>
readShadowMaps(size_t MapBitSize, const std::vector<std::string> &FileNames) {
  std::vector<std::vector<bool>> Result;
  for (const auto &FileName : FileNames) {
    Result.push_back(readShadowMap(MapBitSize, FileName));
  }
  return Result;
}

bool writeShadowMap(const std::vector<bool> &Map, const std::string &FileName) {
  std::ofstream ShadowMapOfs(FileName, std::ios::binary);
  if (!ShadowMapOfs)
    return false;
  unsigned char ByteBuffer = 0;
  for (size_t I = 0; I < Map.size(); I++) {
    if (I % 8 == 0 && I) {
      ShadowMapOfs << ByteBuffer;
      ByteBuffer = 0;
    }
    if (!Map[I])
      continue;
    ByteBuffer |= 1 << (7 - (I % 8));
  }
  if (Map.size() % 8 != 0) {
    ShadowMapOfs << ByteBuffer;
  }
  return ShadowMapOfs.good();
}

std::vector<bool> doMapOp(const std::vector<std::vector<bool>> &Maps,
                          std::function<bool(bool, bool)> Op) {
  if (Maps.empty())
    return std::vector<bool>();
  std::vector<bool> ResultMap(Maps[0]);
  for (size_t M = 1; M < Maps.size(); M++) {
    for (size_t I = 0; I < Maps[0].size(); I++) {
      ResultMap[I] = Op(ResultMap[I], Maps[M][I]);
    }
  }
  return ResultMap;
}

std::string MapStatPrinter::format(const MapStatPrinter::StatTy &Stat) const {
  const auto &[Filename, Desc, Covered, TableSize] = Stat;
  std::stringstream Result;
  Result << std::setw(MaxFilenameLen) << Filename;
  if (Filename.size())
    Result << ": ";
  else if (MaxFilenameLen)
    Result << "  ";

  Result << std::setw(MaxDescLen) << Desc;
  if (Desc.size())
    Result << ": ";
  else if (MaxDescLen)
    Result << "  ";

  size_t IdxWidth = std::to_string(MaxTableSize).size();
  Result << std::setw(IdxWidth) << Covered << " out of ";
  Result << std::setw(IdxWidth) << TableSize;

  double Coverage = Covered;
  Coverage /= TableSize;
  Coverage *= 100;

  Result << " (" << std::to_string(Coverage) << "%)";
  return Result.str();
}

void MapStatPrinter::print() const {
  for (const StatTy &Stat : Stats) {
    llvm::outs() << format(Stat) << '\n';
  }
}

void MapStatPrinter::addFile(const std::string &Filename,
                             const std::vector<bool> &Map) {
  addStat(Filename, Description, getIdxCovered(Map), Map.size());
}

void MapStatPrinter::addFile(const std::string &Filename, size_t Covered,
                             size_t TableSize) {
  addStat(Filename, Description, Covered, TableSize);
}

void MapStatPrinter::addFile(const std::string &Filename, size_t TableSize) {
  std::vector<bool> Map = readShadowMap(TableSize, Filename);
  addFile(Filename, getIdxCovered(Map), TableSize);
}

void MapStatPrinter::addStat(size_t Covered, size_t TableSize) {
  addStat("", Description, Covered, TableSize);
}

void MapStatPrinter::addStat(const std::string &Desc, size_t Covered,
                             size_t TableSize) {
  addStat("", Desc, Covered, TableSize);
}

void MapStatPrinter::addMap(const std::vector<bool> &Map) {
  addStat("", Description, getIdxCovered(Map), Map.size());
}

void MapStatPrinter::summarize(const std::string &Desc, size_t Covered,
                               size_t TableSize, bool alignToDesc) {
  if (Limit < std::numeric_limits<size_t>::max())
    Limit++;
  if (alignToDesc) {
    addStat("", Desc, Covered, TableSize);
  } else {
    addStat(Desc, "", Covered, TableSize);
  }
}

void MapStatPrinter::summarize(const std::string &Desc,
                               const std::vector<bool> &Map, bool alignToDesc) {
  size_t Covered = getIdxCovered(Map);
  summarize(Desc, Covered, Map.size(), alignToDesc);
}

void MapStatPrinter::asc() {
  std::sort(Stats.begin(), Stats.end(), [](const auto &a, const auto &b) {
    return std::get<2>(a) < std::get<2>(b);
  });
}

void MapStatPrinter::desc() {
  std::sort(Stats.begin(), Stats.end(), [](const auto &a, const auto &b) {
    return std::get<2>(a) > std::get<2>(b);
  });
}

void MapStatPrinter::sort(SortTy S) {
  switch (S) {
  default:
    break;
  case Asc:
    asc();
    break;
  case Desc:
    desc();
    break;
  }
}

void MapStatPrinter::limit(size_t L) { Limit = L; }
bool MapStatPrinter::atLimit() const { return Limit == 0; }

void MapStatPrinter::addStat(const std::string &Filename,
                             const std::string &Desc, size_t Covered,
                             size_t TableSize) {
  if (Limit == 0)
    return;
  Limit--;
  MaxTableSize = std::max(MaxTableSize, TableSize);
  MaxFilenameLen = std::max(MaxFilenameLen, Filename.size());
  MaxDescLen = std::max(MaxDescLen, Desc.size());
  Stats.push_back(std::tuple(Filename, Desc, Covered, TableSize));
}