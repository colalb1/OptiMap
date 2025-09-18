# OptiMap
Cache-aware C++ map optimized for speed and efficiency.

### Prerequisites
- CMake 4.0 or higher
- A C++23 compatible compiler

## Benchmarks
Here are the performance and memory usage benchmarks for OptiMap compared to `std::unordered_map` and `ankerl::unordered_dense::map`. `std::unordered_map` and `ankerl::unordered_dense::map` use various hash functions while OptiMap uses its own proprietary hash function.

<table>
<tr>
<td valign="top">
<details>
<summary><strong>Performance Results</strong></summary>
<br>
<em>Speed of various operations. Lower is better.</em>
<div align="center">

**Copy Performance**
<br>
<img src="plots/Copy_performance.png" width="80%">

**CtorDtorEmptyMap Performance**
<br>
<img src="plots/CtorDtorEmptyMap_performance.png" width="80%">

**CtorDtorSingleEntryMap Performance**
<br>
<img src="plots/CtorDtorSingleEntryMap_performance.png" width="80%">

**InsertHugeInt Performance**
<br>
<img src="plots/InsertHugeInt_performance.png" width="80%">

**IterateIntegers Performance**
<br>
<img src="plots/IterateIntegers_performance.png" width="80%">

**RandomDistinct2 Performance**
<br>
<img src="plots/RandomDistinct2_performance.png" width="80%">

**RandomFind 200 Performance**
<br>
<img src="plots/RandomFind_200_performance.png" width="80%">

**RandomFind 2000 Performance**
<br>
<img src="plots/RandomFind_2000_performance.png" width="80%">

**RandomFind 500000 Performance**
<br>
<img src="plots/RandomFind_500000_performance.png" width="80%">

**RandomFindString 1000000 Performance**
<br>
<img src="plots/RandomFindString_1000000_performance.png" width="80%">

**RandomFindString Performance**
<br>
<img src="plots/RandomFindString_performance.png" width="80%">

**RandomInsertErase Performance**
<br>
<img src="plots/RandomInsertErase_performance.png" width="80%">

**RandomInsertEraseStrings Performance**
<br>
<img src="plots/RandomInsertEraseStrings_performance.png" width="80%">

</div>
</details>
</td>
<td valign="top">
<details>
<summary><strong>Memory Usage</strong></summary>
<br>
<em>Memory consumption for various operations. Lower is better.</em>
<div align="center">

**Copy Memory**
<br>
<img src="plots/Copy_memory.png" width="80%">

**CtorDtorEmptyMap Memory**
<br>
<img src="plots/CtorDtorEmptyMap_memory.png" width="80%">

**CtorDtorSingleEntryMap Memory**
<br>
<img src="plots/CtorDtorSingleEntryMap_memory.png" width="80%">

**InsertHugeInt Memory**
<br>
<img src="plots/InsertHugeInt_memory.png" width="80%">

**IterateIntegers Memory**
<br>
<img src="plots/IterateIntegers_memory.png" width="80%">

**RandomDistinct2 Memory**
<br>
<img src="plots/RandomDistinct2_memory.png" width="80%">

**RandomFind 200 Memory**
<br>
<img src="plots/RandomFind_200_memory.png" width="80%">

**RandomFind 2000 Memory**
<br>
<img src="plots/RandomFind_2000_memory.png" width="80%">

**RandomFind 500000 Memory**
<br>
<img src="plots/RandomFind_500000_memory.png" width="80%">

**RandomFindString 1000000 Memory**
<br>
<img src="plots/RandomFindString_1000000_memory.png" width="80%">

**RandomFindString Memory**
<br>
<img src="plots/RandomFindString_memory.png" width="80%">

**RandomInsertErase Memory**
<br>
<img src="plots/RandomInsertErase_memory.png" width="80%">

**RandomInsertEraseStrings Memory**
<br>
<img src="plots/RandomInsertEraseStrings_memory.png" width="80%">

</div>
</details>
</td>
</tr>
</table>

### Running Benchmarks

```bash
# Build the project
mkdir build && cd build
cmake ..
make

# Run benchmarks
./OptiMapBenchmarks
```
