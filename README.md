ABC-ZZ
======

ABC-ZZ is a C++ framework for sequential synthesis and verification.

## FMCAD-19 Property Clustering Feature

This fork implements the property clustering algorithm from the FMCAD 2019 paper:
**"Boosting Verification Scalability via Structural Grouping and Semantic Partitioning of Properties"**
by Rohit Dureja, Jason Baumgartner, Alexander Ivrii, Robert Kanzelman, and Kristin Y. Rozier.

### Usage

The property clustering functionality is available through the `cluster` command in the `bip` tool:

```bash
# Basic usage - cluster properties into 2 groups
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster -n=2

# Cluster into 4 groups
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster -n=4
```

### Parameters

- `-n=<int>`: Target number of clusters (default: 4)

### Algorithm Overview

The clustering algorithm computes support bitvectors for each property's cone-of-influence and uses:

1. **Level-1 Grouping**: Merges properties with identical COI
2. **Robust Agglomerative Clustering**: Jaccard similarity-based merging with guaranteed convergence to target clusters
3. **Semantic Partitioning**: Simple size-based splitting when needed to reach target clusters
4. **Quality Analysis**: Provides intra/inter-cluster similarity metrics and separation ratios

The implementation leverages ABC-ZZ's existing infrastructure for netlist traversal, COI computation, and bitvector operations.

### Using Clustered Properties

After clustering, you can verify each group separately using the `-prop` parameter:

```bash
# Verify first cluster (from example above)
./build/ZZ/Bip/bip.exe -input=your_file.aig -prop=[0,1,2,3,30,27,24,21,18,15,12,9,6] ,pdr

# Verify second cluster
./build/ZZ/Bip/bip.exe -input=your_file.aig -prop=[4,29,26,23,20,17,14,11,8,5] ,pdr

# Verify third cluster
./build/ZZ/Bip/bip.exe -input=your_file.aig -prop=[31,28,25,22,19,16,13,10,7] ,pdr
```



## Build Instructions

The ZZ framework uses CMake to build the system.

### Quick Build (Recommended)

```bash
# Create build directory and configure
mkdir build
cd build
cmake ..

# Build with parallel compilation
cmake --build . --parallel 4
```