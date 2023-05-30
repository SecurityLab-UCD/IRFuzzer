#include "commandline.h"

namespace llvm {

cl::OptionCategory AnalysisCategory("Analysis Options");

// ----------------------------------------------------------------
// analyze subcommand

cl::SubCommand AnalyzeCmd("analyze",
                          "Analyze coverage loss in experimental shadow map");

cl::opt<bool> AnVerbosity("v", cl::desc("Increase verbosity"), cl::init(false),
                          cl::sub(AnalyzeCmd), cl::cat(AnalysisCategory),
                          cl::ValueDisallowed);

cl::opt<std::string> AnLookupFile(cl::Positional, cl::desc("<lookup-table>"),
                                  cl::Required, cl::cat(AnalysisCategory),
                                  cl::sub(AnalyzeCmd));

cl::opt<std::string> AnMapFile(cl::Positional, cl::desc("<map>"), cl::Required,
                               cl::cat(AnalysisCategory), cl::sub(AnalyzeCmd));

cl::opt<size_t> AnMaxEntries("l", cl::desc("Limit blame list entries printed"),
                             cl::value_desc("entries"),
                             cl::init(std::numeric_limits<size_t>::max()),
                             cl::sub(AnalyzeCmd), cl::cat(AnalysisCategory));

cl::opt<std::string>
    AnPatOutFile("pat", cl::desc("Output uncovered patterns sorted by loss"),
                 cl::sub(AnalyzeCmd), cl::cat(AnalysisCategory));

cl::opt<bool> AnPatUseLossPerPattern(
    "loss-per-pattern",
    cl::desc("Divide blamee loss by number of blamers (patterns) in --pat"),
    cl::sub(AnalyzeCmd), cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// upperbound subcommand

cl::SubCommand
    UBCmd("upperbound",
          "Calculate matcher table coverage upper bound given true predicates");

cl::opt<bool> UBVerbosity("v", cl::desc("Increase verbosity"), cl::init(false),
                          cl::sub(UBCmd), cl::cat(AnalysisCategory),
                          cl::ValueDisallowed);

cl::opt<std::string> UBLookupFile(cl::Positional, cl::desc("<lookup-table>"),
                                  cl::Required, cl::cat(AnalysisCategory),
                                  cl::sub(UBCmd));

cl::list<std::string> UBTruePredicates(cl::Positional,
                                       cl::desc("[true-pred-name-or-idx...]"),
                                       cl::ZeroOrMore, cl::sub(UBCmd),
                                       cl::cat(AnalysisCategory));

cl::opt<std::string>
    UBPatPredStr("p", cl::desc("Manually set pattern predicate values"),
                 cl::init(""), cl::sub(UBCmd), cl::cat(AnalysisCategory));

cl::opt<std::string> UBOutputFile("o", cl::desc("Generate shadow map output"),
                                  cl::Optional, cl::value_desc("outfile"),
                                  cl::sub(UBCmd), cl::cat(AnalysisCategory));

cl::opt<bool>
    UBPredCaseSensitive("s", cl::desc("Make predicate name case sensitive"),
                        cl::init(false), cl::sub(UBCmd),
                        cl::cat(AnalysisCategory));

cl::opt<bool> UBShowBlameList("b", cl::desc("Show matcher coverage blame list"),
                              cl::init(false), cl::sub(UBCmd),
                              cl::cat(AnalysisCategory));

cl::opt<size_t> UBMaxBlameEntries("l",
                                  cl::desc("Limit blame list entries printed"),
                                  cl::value_desc("entries"), cl::sub(UBCmd),
                                  cl::init(std::numeric_limits<size_t>::max()),
                                  cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// intersect subcommand

cl::SubCommand IntersectCmd("intersect", "Calculate shadow map intersection");

cl::opt<bool> IntVerbosity("v", cl::desc("Increase verbosity"), cl::init(false),
                           cl::sub(IntersectCmd), cl::cat(AnalysisCategory),
                           cl::ValueDisallowed);

cl::opt<size_t> IntTableSize(cl::Positional, cl::desc("<table-size>"),
                             cl::Required, cl::sub(IntersectCmd),
                             cl::cat(AnalysisCategory));

cl::list<std::string> IntFiles(cl::Positional, cl::desc("<maps...>"),
                               cl::OneOrMore, cl::sub(IntersectCmd),
                               cl::cat(AnalysisCategory));

cl::opt<std::string> IntOutputFile("o", cl::desc("Generate shadow map output"),
                                   cl::Optional, cl::value_desc("outfile"),
                                   cl::sub(IntersectCmd),
                                   cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// diff subcommand

cl::SubCommand DiffCmd("diff", "Calculate shadow map difference");

cl::opt<bool> DiffVerbosity("v", cl::desc("Increase verbosity"),
                            cl::init(false), cl::sub(DiffCmd),
                            cl::cat(AnalysisCategory), cl::ValueDisallowed);

cl::opt<size_t> DiffTableSize(cl::Positional, cl::desc("<table-size>"),
                              cl::Required, cl::sub(DiffCmd),
                              cl::cat(AnalysisCategory));

cl::list<std::string> DiffFiles(cl::Positional, cl::desc("<maps...>"),
                                cl::OneOrMore, cl::sub(DiffCmd),
                                cl::cat(AnalysisCategory));

cl::opt<std::string> DiffOutputFile("o", cl::desc("Generate shadow map output"),
                                    cl::Optional, cl::value_desc("outfile"),
                                    cl::sub(DiffCmd),
                                    cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// union subcommand

cl::SubCommand UnionCmd("union", "Calculate shadow map union");

cl::opt<bool> UnionVerbosity("v", cl::desc("Increase verbosity"),
                             cl::init(false), cl::sub(UnionCmd),
                             cl::cat(AnalysisCategory), cl::ValueDisallowed);

cl::opt<size_t> UnionTableSize(cl::Positional, cl::desc("<table-size>"),
                               cl::Required, cl::sub(UnionCmd),
                               cl::cat(AnalysisCategory));

cl::list<std::string> UnionFiles(cl::Positional, cl::desc("<maps...>"),
                                 cl::OneOrMore, cl::sub(UnionCmd),
                                 cl::cat(AnalysisCategory));

cl::opt<std::string> UnionOutputFile("o",
                                     cl::desc("Generate shadow map output"),
                                     cl::Optional, cl::value_desc("outfile"),
                                     cl::sub(UnionCmd),
                                     cl::cat(AnalysisCategory));

// ----------------------------------------------------------------
// stat subcommand

cl::SubCommand StatCmd("stat", "Show statistics of shadow map(s)");

cl::opt<size_t> StatTableSize(cl::Positional, cl::desc("<table-size>"),
                              cl::Required, cl::sub(StatCmd),
                              cl::cat(AnalysisCategory));

cl::list<std::string> StatFiles(cl::Positional, cl::desc("<maps...>"),
                                cl::OneOrMore, cl::sub(StatCmd),
                                cl::cat(AnalysisCategory));

cl::opt<MapStatPrinter::SortTy>
    StatSort("sort", cl::desc("Sort by covered indices"),
             cl::init(MapStatPrinter::None),
             cl::values(clEnumValN(MapStatPrinter::None, "none", "Do not sort"),
                        clEnumValN(MapStatPrinter::Asc, "asc",
                                   "Sort in ascending order"),
                        clEnumValN(MapStatPrinter::Desc, "desc",
                                   "Sort in descending order")),
             cl::sub(StatCmd), cl::cat(AnalysisCategory));

} // end namespace llvm