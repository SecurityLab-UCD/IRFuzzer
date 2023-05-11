# Mapper
Mapper is a tool for analyzing matcher table coverage and manipulating shadow
maps.

## Examples
```shell
# In Security Lab LLVM source tree (with llvm-tblgen -pattern-lookup feature)

# Generate pattern lookup table
./build/bin/llvm-tblgen -gen-dag-isel -pattern-lookup ~/isel.json -omit-comments \
  -I ./llvm/lib/Target/X86 \
  -I./build/include \
  -I./llvm/include \
  -I ./llvm/lib/Target \
  ./llvm/lib/Target/X86/X86.td -o ~/isel.inc -d /dev/nul

# In mapper build directory

# Calculate coverage upper bound for given pattern lookup table (and optionally true predicates)
# and output it as a shadow map
./mapper upperbound ~/isel.json 4 8 13 -o bigger.map
./mapper upperbound ~/isel.json 8 13 -o smaller.map
# optionally just print coverage upper bound as percentage
./mapper upperbound ~/isel.json 4 8 13
./mapper upperbound ~/isel.json 8 13
# Note that predicate names (or a mix of indices and names) are also supported
# (case-insensitive by default, use -s to override)
./mapper upperbound ~/isel.json in64bitmode hasfp16

# Get table size from pattern lookup table
tablesize=$(jq '.table_size' ~/isel.json)

# Show coverage stats for each generated shadow map
./mapper stat $tablesize bigger.map smaller.map

# Shadow map 1 MINUS shadow map 2
# (shows indices that are covered by the first map and not the second map)
./mapper diff $tablesize bigger.map smaller.map
# optionally output as shadow map
./mapper diff $tablesize bigger.map smaller.map -o diff.map

# Shadow map 1 AND shadow map 2
# (shows indices that are covered in both maps)
./mapper intersect $tablesize bigger.map smaller.map
# optionally output as shadow map
./mapper intersect $tablesize bigger.map smaller.map -o intersect.map
```