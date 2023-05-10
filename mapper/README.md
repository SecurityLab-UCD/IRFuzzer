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

# Calculate coverage upper bound for given pattern lookup table
# and output it as a shadow map
./mapper -upperbound -lookup ~/isel.json -pred 4,8,13 -as-map bigger.map
./mapper -upperbound -lookup ~/isel.json -pred 8,13 -as-map smaller.map
# optionally just print coverage upper bound as percentage
./mapper -upperbound -lookup ~/isel.json -pred 4,8,13
./mapper -upperbound -lookup ~/isel.json -pred 8,13

# Show coverage stats for each generated shadow map
./mapper -stat -map bigger.map -map smaller.map -table-size $(jq '.table_size' ~/isel.json)

# Shadow map 1 MINUS shadow map 2
# (shows indices that are covered by the first map and not the second map)
./mapper -diff -map bigger.map -map smaller.map -table-size $(jq '.table_size' ~/isel.json)
# optionally output as shadow map
./mapper -diff -map bigger.map -map smaller.map -as-map diff.map \
  -table-size $(jq '.table_size' ~/isel.json)

# Shadow map 1 AND shadow map 2
# (shows indices that are covered in both maps)
./mapper -intersect -map bigger.map -map smaller.map -table-size $(jq '.table_size' ~/isel.json)
# optionally output as shadow map
./mapper -intersect -map bigger.map -map smaller.map -as-map intersect.map \
  -table-size $(jq '.table_size' ~/isel.json)
```