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
void clusterProperties(NetlistRef N, uint n_clusters, uint n_pivots, uint seq_depth, /*out*/Vec<Vec<uint> >& clusters);

// Core support bitvector and affinity computation functions
void computeSupportBitvectors(NetlistRef N, uint n_pivots, /*out*/Vec<Vec<uint64> >& bitvectors, /*out*/uint& total_vars);
double computeAffinity(const Vec<uint64>& bv1, const Vec<uint64>& bv2);
double computeClusterAffinity(const Vec<Vec<uint64> >& bitvectors, const Vec<uint>& cluster1, const Vec<uint>& cluster2);
uint hammingDistance(const Vec<uint64>& bv1, const Vec<uint64>& bv2);

// Level-1 grouping: identical COI merging (Section III.A)
void groupingLevel1(const Vec<Vec<uint64> >& bitvectors, /*out*/Vec<Vec<uint> >& groups);

// Legacy functions - Level-2 and Level-3 are now integrated into robust clustering
// void groupingLevel2(NetlistRef N, const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold);
// void groupingLevel3(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold, uint actual_bits);

// Robust agglomerative clustering - main algorithm (Section III)
void robustAgglomerativeClustering(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& clusters, uint target_clusters);
void displayClusterQualityMetrics(const Vec<Vec<uint64> >& bitvectors, const Vec<Vec<uint> >& clusters);

// Legacy helper functions - kept for compatibility
void mergeClosestGroups(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups);
double computeClusterQuality(const Vec<Vec<uint64> >& bitvectors, const Vec<uint>& group);

// Semantic partitioning using localization (Section IV)
void semanticPartitioning(NetlistRef N, Vec<Vec<uint> >& groups, uint target_clusters, uint bmc_limit);

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
