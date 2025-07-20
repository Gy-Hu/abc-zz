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
# Basic usage with default threshold (0.9)
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster # Recommanded to try ./ZZ/Bip/bip.exe -input=xxx/hwmcc13/multi/6s281.aig ,cluster -threshold=0.2 

# Use higher threshold for stricter grouping (fewer, higher-quality clusters)
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster -threshold=0.95

# Use lower threshold for more grouping (more clusters with lower similarity)
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster -threshold=0.8
```

### Parameters

- `-threshold=<float>`: Affinity threshold for grouping properties (0.0-1.0, default: 0.9)

### Algorithm Overview

The clustering algorithm implements the FMCAD-19 paper and automatically determines the optimal number of clusters based on structural similarity:

1. **Level-1 Grouping**: Merges properties with identical COI (100% similarity)
2. **Level-2 Grouping**: Groups properties based on heavy-weight strongly connected components
3. **Level-3 Grouping**: Groups properties based on Hamming distance with configurable threshold
4. **Quality Analysis**: Provides intra/inter-cluster similarity metrics

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

### Quick Build

```bash
# Create build directory and configure
mkdir build
cd build
cmake ..

# Build with parallel compilation
cmake --build . --parallel 4
```