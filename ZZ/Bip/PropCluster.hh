#ifndef ZZ__Bip__PropCluster_hh
#define ZZ__Bip__PropCluster_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void clusterProperties(NetlistRef N, uint n_clusters, uint n_pivots, uint seq_depth, /*out*/Vec<Vec<uint> >& clusters);

// FMCAD-19: Support bitvector and affinity computation functions
void computeSupportBitvectors(NetlistRef N, uint n_pivots, /*out*/Vec<Vec<uint64> >& bitvectors);
double computeAffinity(const Vec<uint64>& bv1, const Vec<uint64>& bv2);
uint hammingDistance(const Vec<uint64>& bv1, const Vec<uint64>& bv2);

// FMCAD-19: Three-level grouping algorithms from the paper
void groupingLevel1(const Vec<Vec<uint64> >& bitvectors, /*out*/Vec<Vec<uint> >& groups);
void groupingLevel2(NetlistRef N, const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold);
void groupingLevel3(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold);

// FMCAD-19: Helper functions for grouping
void mergeClosestGroups(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups);
double computeClusterQuality(const Vec<Vec<uint64> >& bitvectors, const Vec<uint>& group);

// FMCAD-19: Semantic partitioning using localization
void semanticPartitioning(NetlistRef N, Vec<Vec<uint> >& groups, uint bmc_limit);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
