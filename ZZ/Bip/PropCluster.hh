#ifndef ZZ__Bip__PropCluster_hh
#define ZZ__Bip__PropCluster_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// FMCAD-19 Property Clustering Implementation
// "Boosting Verification Scalability via Structural Grouping and Semantic Partitioning of Properties"
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm

// Main clustering function - implements the complete FMCAD-19 algorithm
// Uses affinity threshold to automatically determine optimal number of clusters
void clusterProperties(NetlistRef N, double affinity_threshold, /*out*/Vec<Vec<uint> >& clusters);

// Core support bitvector and affinity computation functions
void computeSupportBitvectors(NetlistRef N, /*out*/Vec<Vec<uint64> >& bitvectors, /*out*/uint& total_vars);
double computeAffinity(const Vec<uint64>& bv1, const Vec<uint64>& bv2, uint total_vars);
double computeClusterAffinity(const Vec<Vec<uint64> >& bitvectors, const Vec<uint>& cluster1, const Vec<uint>& cluster2, uint total_vars);
uint hammingDistance(const Vec<uint64>& bv1, const Vec<uint64>& bv2);

// Level-1 grouping: identical COI merging (Section III.A)
void groupingLevel1(const Vec<Vec<uint64> >& bitvectors, /*out*/Vec<Vec<uint> >& groups);

// FMCAD-19 Level-2 grouping (Section III.B - Heavy-weight SCCs)
void groupingLevel2(NetlistRef N, const Vec<Vec<uint64> >& bitvectors, /*out*/Vec<Vec<uint> >& groups, double affinity_threshold, uint total_vars);

// FMCAD-19 Level-3 grouping (Section III.C - Hamming distance)
void groupingLevel3(const Vec<Vec<uint64> >& bitvectors, /*out*/Vec<Vec<uint> >& groups, double affinity_threshold, uint total_vars);

// Quality metrics and display
void displayClusterQualityMetrics(const Vec<Vec<uint64> >& bitvectors, const Vec<Vec<uint> >& clusters, uint total_vars);

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
