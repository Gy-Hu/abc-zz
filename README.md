ABC-ZZ
======

ABC-ZZ is a C++ framework for sequential synthesis and verification.

![.github/workflows/build.yml](https://github.com/berkeley-abc/abc-zz/workflows/.github/workflows/build.yml/badge.svg)

## FMCAD-19 Property Clustering Feature

This fork implements the three-level property grouping algorithm from the FMCAD 2019 paper:
**"Boosting Verification Scalability via Structural Grouping and Semantic Partitioning of Properties"**
by Rohit Dureja, Jason Baumgartner, Alexander Ivrii, Robert Kanzelman, and Kristin Y. Rozier.

### Usage

The property clustering functionality is available through the `cluster` command in the `bip` tool:

```bash
# Basic usage - cluster properties into 2 groups
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster -n=2

# Advanced usage with custom parameters
./build/ZZ/Bip/bip.exe -input=your_file.aig ,cluster -n=4 -pivots=512 -seq=3
```

### Parameters

- `-n=<int>`: Target number of clusters (default: 4)
- `-pivots=<int>`: Number of state variables to track in support computation (default: 256)
- `-seq=<int>`: Sequential depth of support analysis (default: 5)

### Algorithm Overview

The implementation includes:

1. **Level-1 Grouping**: Merges properties with identical Cone of Influence (COI)
2. **Level-2 Grouping**: Merges groups based on high affinity using Jaccard similarity
3. **Level-3 Grouping**: Merges groups based on Hamming distance of support vectors
4. **Semantic Partitioning**: Uses localization-based refinement for large groups
5. **Quality Analysis**: Provides detailed clustering statistics

### Example Output

```
=== FMCAD-19 Property Clustering Results ===
Total properties: 32
Number of clusters: 2
Target clusters: 2
Pivots used: 256
Sequential depth: 5

Cluster 0: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15] (size: 16)
Cluster 1: [16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31] (size: 16)

=== Cluster Quality Analysis ===
Properties clustered: 32
Singleton clusters: 0
Largest cluster size: 16
Average cluster size: 16.00
```

### Using Clustered Properties

After clustering, you can verify each group separately using the `-prop` parameter:

```bash
# Verify first cluster
./build/ZZ/Bip/bip.exe -input=your_file.aig -prop=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15] ,pdr

# Verify second cluster
./build/ZZ/Bip/bip.exe -input=your_file.aig -prop=[16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31] ,pdr
```

### Implementation Details

The clustering algorithm computes support bitvectors for each property and uses three levels of grouping:
- **Structural similarity** (identical support sets)
- **High affinity** (Jaccard similarity ≥ 0.9)
- **Moderate affinity** (Hamming distance-based similarity ≥ 0.8)

The implementation leverages ABC-ZZ's existing infrastructure including:
- Netlist traversal and COI computation
- Localization abstraction framework
- Multi-property BMC support

## Build Instructions

The ZZ framework uses CMake to build the system.

- Create a new directory 'build' under the abc-zz main directory

  mkdir build
  cd build

- Run CMake

  cmake ..

- Build the target you are interested in (e.g. bip.exe)

  make bip.exe (builds the bip executable)
  make Bip (build the Bip library and its dependencies)
  make Bip-pic (build the Bip library and its dependencies with -fPIC)
  make Bip-exe (build all exectuables in module Bip)

  make zz_all (builds all libraries and executables)
  make zz_pic (build all libraries with -fPIC)
  make zz_static (build all static libraries)


Build ZZ requires the following:

- CMake version 3.8 or above
- Python 2.7s (or later)
- Developer header files and libraries for 'zlib' (e.g. zlib1g-dev package on Ubuntu)

Recommended:

- GNU Readline developer header files and libraries (e.g. readline-dev on Ubuntu)
- libpng developer header files and libraries (e.g. libpng12-dev on Ubuntu)


## Windows

Building on Windows is more complicated. The simplest way is to use `vcpkg` to gather the dependencies.

### Vcpkg

Clone the repository

    git clone https://github.com/Microsoft/vcpkg

Change into the `vcpkg` directory, and bootstrap

    cd vcpkg
    ./bootstrap-vcpkg.bat

Install the relevant pacakges

    ./vcpkg.exe install dirent:x64-windows-static-md zlib:x64-windows-static-md

Create a build directory 

    cd ../
    mkdir build
    cd build

Configure

    cmake \
        -G "Visual Studio 15 2017 Win64" \
        -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=x64-windows-static-md \
        \
        .. 

Build

    cmake --build .

