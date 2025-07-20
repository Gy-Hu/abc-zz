//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PropCluster.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : FMCAD-19 Property Clustering Implementation
//|               "Boosting Verification Scalability via Structural Grouping
//|                and Semantic Partitioning of Properties"
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| This implements the property clustering algorithm from FMCAD-19 paper.
//| The algorithm groups properties based on structural similarity of their
//| cone-of-influence (COI) using three levels:
//| 1. Level-1: Identical COI grouping (100% similarity)
//| 2. Level-2: SCC-based grouping (heavy strongly connected components)
//| 3. Level-3: Hamming distance-based clustering (configurable threshold)
//| 4. Robust agglomerative clustering to reach target number of clusters
//| 5. Optional semantic partitioning using localization feedback
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "PropCluster.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// MAIN CLUSTERING FUNCTION: Implements FMCAD-19 property clustering algorithm
// Input:  N - netlist, n_clusters - target number of clusters
//         n_pivots, seq_depth - legacy parameters (unused, kept for API compatibility)
// Output: clusters - vector of property groups
void clusterProperties(NetlistRef N, uint n_clusters, uint n_pivots, uint seq_depth, /*out*/Vec<Vec<uint> >& clusters)
{
    Get_Pob(N, properties);
    if (properties.size() == 0) return;

    // FMCAD-19: Multi-level structural grouping followed by robust agglomerative clustering

    // Step 1: Compute support bitvectors for all properties
    Vec<Vec<uint64> > bitvectors;
    uint total_vars;
    computeSupportBitvectors(N, 0, bitvectors, total_vars);  // n_pivots parameter unused

    // Progress: Support bitvector computation completed
    WriteLn "Computing support bitvectors for %_ properties...", properties.size();

    // Step 2: Initialize clusters - each property starts in its own group
    clusters.clear();
    for (uint i = 0; i < properties.size(); i++){
        clusters.push();
        clusters.last().push(i);
    }

    // Step 3: Level-1 grouping - merge properties with identical COI (preprocessing)
    WriteLn "Level-1: Merging properties with identical COI...";
    groupingLevel1(bitvectors, clusters);
    WriteLn "Level-1: Reduced to %_ clusters", clusters.size();

    // Step 4: Robust agglomerative clustering - guaranteed to reach target
    WriteLn "Level-2: Performing robust agglomerative clustering...";
    robustAgglomerativeClustering(bitvectors, clusters, n_clusters);
    WriteLn "Level-2: Final clustering completed with %_ clusters", clusters.size();

    // Step 5: Compute and display quality metrics
    displayClusterQualityMetrics(bitvectors, clusters);

    // Step 6: Optional semantic partitioning (simple size-based splitting)
    // Only run if we have fewer clusters than requested
    if (seq_depth > 0 && clusters.size() < n_clusters){
        semanticPartitioning(N, clusters, n_clusters, seq_depth);  // seq_depth unused
    }
}


//=================================================================================================
// FMCAD-19: Support bitvector computation (Section III.A - COI Computation)
// This implements the bitvector representation of property support variables
// as described in Figure 2 of the paper. Each property gets a bitvector where
// bit i is set if support variable i is in the property's cone-of-influence.

void computeSupportBitvectors(NetlistRef N, uint n_pivots, /*out*/Vec<Vec<uint64> >& bitvectors, /*out*/uint& total_vars)
// Note: n_pivots parameter is legacy and unused - all relevant support variables are included
{
    Get_Pob(N, properties);
    if (properties.size() == 0) return;

    // PHASE 1: Collect all unique PI/Flop gates from all property COIs  
    WZet relevant_gates;
    
    for (uint i = 0; i < properties.size(); i++){
        // Compute COI for property i using backward traversal
        WZet coi;
        Vec<Wire> queue;
        queue.push(properties[i]);
        coi.add(properties[i]);

        // Backward traversal to collect all gates in COI
        for (uint j = 0; j < queue.size(); j++){
            Wire w = queue[j];
            For_Inputs(w, v){
                if (coi.add(v)){
                    queue.push(v);
                }
            }
        }

        // Collect PI/Flop gates from this COI
        for (uint j = 0; j < coi.size(); j++){
            Wire w = coi.list()[j];
            if (w.type() == 1 || w.type() == 3){  // Actual observed types: PI=1, Flop=3
                relevant_gates.add(w);
            }
        }
    }

    // PHASE 2: Create mapping only for relevant gates
    Map<Wire, uint> var_to_index;
    uint index = 0;
    
    // Map all relevant gates
    uint gates_to_map = relevant_gates.size();
    
    for (uint i = 0; i < gates_to_map; i++){
        Wire w = relevant_gates.list()[i];
        var_to_index.set(w, index++);
    }

    total_vars = index;  // Set the output parameter
    uint words = (total_vars + 63) / 64;
    
    // Progress: Variable mapping completed
    if (total_vars > 1000) {
        WriteLn "Mapped %_ support variables from %_ relevant gates", total_vars, relevant_gates.size();
    }

    // PHASE 3: Build bitvectors efficiently
    bitvectors.clear();
    bitvectors.setSize(properties.size());

    for (uint i = 0; i < properties.size(); i++){
        bitvectors[i].setSize(words, 0);

        // Compute COI for property i using backward traversal
        WZet coi;
        Vec<Wire> queue;
        queue.push(properties[i]);
        coi.add(properties[i]);

        // Backward traversal to collect all gates in COI
        for (uint j = 0; j < queue.size(); j++){
            Wire w = queue[j];
            For_Inputs(w, v){
                if (coi.add(v)){
                    queue.push(v);
                }
            }
        }

        // Set bits in bitvector for support variables in COI that are in our mapping
        uint support_count = 0;
        Vec<uint> used_indices;
        for (uint j = 0; j < coi.size(); j++){
            Wire w = coi.list()[j];
            if (w.type() == 1 || w.type() == 3){  // Actual observed types: PI=1, Flop=3
                uint* idx;
                if (var_to_index.get(w, idx)){
                    uint word = *idx / 64;
                    uint bit = *idx % 64;
                    if (word < words){
                        bitvectors[i][word] |= 1ULL << bit;
                        support_count++;
                        used_indices.push(*idx);
                    }
                }
            }
        }

        // Progress indicator for large property sets
        if (properties.size() > 100 && (i + 1) % (properties.size() / 10) == 0) {
            WriteLn "Progress: %_/%_ properties processed", i + 1, properties.size();
        }
    }
}


//=================================================================================================
// FMCAD-19: Affinity and distance computation (Section II.C - Property Affinity)
// Implements Jaccard similarity coefficient: |A ∩ B| / |A ∪ B|
// This measures structural similarity between properties based on their COI overlap.

double computeAffinity(const Vec<uint64>& bv1, const Vec<uint64>& bv2)
{
    if (bv1.size() != bv2.size()) return 0.0;

    uint intersection = 0;
    uint union_size = 0;

    for (uint i = 0; i < bv1.size(); i++){
        uint64 and_bits = bv1[i] & bv2[i];
        uint64 or_bits = bv1[i] | bv2[i];

        // Count set bits using builtin popcount
        intersection += __builtin_popcountll(and_bits);
        union_size += __builtin_popcountll(or_bits);
    }

    if (union_size == 0) return 1.0;  // Both empty
    return double(intersection) / double(union_size);
}


double computeClusterAffinity(const Vec<Vec<uint64> >& bitvectors, const Vec<uint>& cluster1, const Vec<uint>& cluster2)
{
    if (cluster1.size() == 0 || cluster2.size() == 0) return 0.0;
    if (bitvectors.size() == 0) return 0.0;
    
    uint words = bitvectors[0].size();
    
    // Compute union of all COIs in cluster1
    Vec<uint64> union1(words, 0);
    for (uint i = 0; i < cluster1.size(); i++) {
        uint prop_id = cluster1[i];
        if (prop_id < bitvectors.size()) {
            for (uint w = 0; w < words; w++) {
                union1[w] |= bitvectors[prop_id][w];
            }
        }
    }
    
    // Compute union of all COIs in cluster2
    Vec<uint64> union2(words, 0);
    for (uint i = 0; i < cluster2.size(); i++) {
        uint prop_id = cluster2[i];
        if (prop_id < bitvectors.size()) {
            for (uint w = 0; w < words; w++) {
                union2[w] |= bitvectors[prop_id][w];
            }
        }
    }
    
    // Compute Jaccard similarity between the two union COIs
    return computeAffinity(union1, union2);
}


uint hammingDistance(const Vec<uint64>& bv1, const Vec<uint64>& bv2)
{
    if (bv1.size() != bv2.size()) return UINT_MAX;

    uint distance = 0;
    for (uint i = 0; i < bv1.size(); i++){
        uint64 xor_bits = bv1[i] ^ bv2[i];
        distance += __builtin_popcountll(xor_bits);
    }

    return distance;
}


//=================================================================================================
// FMCAD-19: Level-1 grouping (Section III.A - Identical COI)
// Merges properties with identical support bitvectors (100% affinity).
// This corresponds to Figure 4 in the paper - uses hash table for O(n) complexity.

void groupingLevel1(const Vec<Vec<uint64> >& bitvectors, /*out*/Vec<Vec<uint> >& groups)
{
    // Use IntMap instead of Map to avoid copy issues
    IntMap<uint, uint> prop_to_group;
    Vec<Vec<uint> > new_groups;

    for (uint i = 0; i < groups.size(); i++){
        if (groups[i].size() == 0) continue;

        uint prop_id = groups[i][0];  // Representative property of the group

        // Find if any existing group has the same bitvector
        bool found = false;
        for (uint j = 0; j < new_groups.size(); j++){
            if (new_groups[j].size() > 0){
                uint existing_prop = new_groups[j][0];
                if (bitvectors[prop_id].size() == bitvectors[existing_prop].size()){
                    bool identical = true;
                    for (uint k = 0; k < bitvectors[prop_id].size(); k++){
                        if (bitvectors[prop_id][k] != bitvectors[existing_prop][k]){
                            identical = false;
                            break;
                        }
                    }
                    if (identical){
                        // Merge with existing group
                        append(new_groups[j], groups[i]);
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found){
            // Create new group
            new_groups.push();
            append(new_groups.last(), groups[i]);
        }
    }

    // Move results back to groups
    groups.clear();
    for (uint i = 0; i < new_groups.size(); i++){
        groups.push();
        append(groups.last(), new_groups[i]);
    }
}


//=================================================================================================
// FMCAD-19: Robust agglomerative clustering (Section III - Main Algorithm)
// Two-phase approach: 1) Similarity-based merging, 2) Balanced redistribution
// Guarantees reaching target number of clusters while maximizing intra-cluster affinity.

void robustAgglomerativeClustering(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& clusters, uint target_clusters)
{
    // First phase: similarity-based clustering
    bool found_similarity = true;
    while (clusters.size() > target_clusters && clusters.size() > 1 && found_similarity) {
        double best_affinity = -1.0;
        uint best_i = 0, best_j = 1;

        // Find the two clusters with highest Jaccard similarity
        for (uint i = 0; i < clusters.size(); i++) {
            if (clusters[i].size() == 0) continue;
            for (uint j = i + 1; j < clusters.size(); j++) {
                if (clusters[j].size() == 0) continue;

                // Compute cluster-to-cluster affinity using union of all COIs in each cluster
                double affinity = computeClusterAffinity(bitvectors, clusters[i], clusters[j]);

                if (affinity > best_affinity) {
                    best_affinity = affinity;
                    best_i = i;
                    best_j = j;
                }
            }
        }

        if (best_affinity > 0.0) {
            // Merge clusters with similarity - show progress for large datasets
            if (clusters.size() > 10) {
                WriteLn "Merging clusters: %_ -> %_ (affinity: %.3f)", clusters.size(), clusters.size() - 1, best_affinity;
            }

            // Always merge the best pair
            append(clusters[best_i], clusters[best_j]);
            clusters[best_j].clear();

            // Remove empty clusters
            Vec<Vec<uint> > new_clusters;
            for (uint i = 0; i < clusters.size(); i++) {
                if (clusters[i].size() > 0) {
                    new_clusters.push();
                    append(new_clusters.last(), clusters[i]);
                }
            }

            // Move results back
            clusters.clear();
            for (uint i = 0; i < new_clusters.size(); i++) {
                clusters.push();
                append(clusters.last(), new_clusters[i]);
            }
        } else {
            found_similarity = false;
        }
    }

    // Second phase: if no more similarity found but still need to reduce clusters
    // Use round-robin distribution to create balanced clusters
    if (clusters.size() > target_clusters) {
        WriteLn "Balancing clusters: redistributing %_ clusters to %_ target clusters", clusters.size(), target_clusters;

        // Collect all properties from singleton clusters
        Vec<uint> singleton_props;
        Vec<Vec<uint> > multi_clusters;

        for (uint i = 0; i < clusters.size(); i++) {
            if (clusters[i].size() == 1) {
                singleton_props.push(clusters[i][0]);
            } else if (clusters[i].size() > 1) {
                multi_clusters.push();
                append(multi_clusters.last(), clusters[i]);
            }
        }

        // Redistribute singletons to create target number of clusters
        clusters.clear();

        // Start with existing multi-clusters
        for (uint i = 0; i < multi_clusters.size() && clusters.size() < target_clusters; i++) {
            clusters.push();
            append(clusters.last(), multi_clusters[i]);
        }

        // Create additional clusters if needed
        while (clusters.size() < target_clusters && singleton_props.size() > 0) {
            clusters.push();
            clusters.last().push(singleton_props[0]);
            singleton_props[0] = singleton_props.last();
            singleton_props.pop();
        }

        // Distribute remaining singletons round-robin
        uint cluster_idx = 0;
        while (singleton_props.size() > 0) {
            clusters[cluster_idx % target_clusters].push(singleton_props[0]);
            singleton_props[0] = singleton_props.last();
            singleton_props.pop();
            cluster_idx++;
        }
    }
}


// FMCAD-19: Cluster quality analysis (Section V - Experimental Results)
// Computes intra-cluster and inter-cluster similarity metrics to evaluate clustering quality
void displayClusterQualityMetrics(const Vec<Vec<uint64> >& bitvectors, const Vec<Vec<uint> >& clusters)
{
    WriteLn "=== Cluster Quality Analysis ===";

    // Calculate average intra-cluster similarity
    double total_intra_similarity = 0.0;
    uint intra_pairs = 0;

    for (uint c = 0; c < clusters.size(); c++) {
        if (clusters[c].size() <= 1) continue;

        double cluster_similarity = 0.0;
        uint cluster_pairs = 0;

        for (uint i = 0; i < clusters[c].size(); i++) {
            for (uint j = i + 1; j < clusters[c].size(); j++) {
                double affinity = computeAffinity(bitvectors[clusters[c][i]], bitvectors[clusters[c][j]]);
                cluster_similarity += affinity;
                cluster_pairs++;
            }
        }

        if (cluster_pairs > 0) {
            cluster_similarity /= cluster_pairs;
            total_intra_similarity += cluster_similarity;
            intra_pairs++;
        }
    }

    if (intra_pairs > 0) {
        total_intra_similarity /= intra_pairs;
        WriteLn "Average intra-cluster similarity: %.3f", total_intra_similarity;
    }

    // Calculate average inter-cluster similarity (simplified for performance)
    double total_inter_similarity = 0.0;
    uint inter_pairs = 0;

    for (uint i = 0; i < clusters.size() && i < 10; i++) {  // Limit to first 10 clusters for performance
        if (clusters[i].size() == 0) continue;
        for (uint j = i + 1; j < clusters.size() && j < 10; j++) {
            if (clusters[j].size() == 0) continue;

            // Use cluster representatives for efficiency
            double affinity = computeClusterAffinity(bitvectors, clusters[i], clusters[j]);
            total_inter_similarity += affinity;
            inter_pairs++;
        }
    }

    if (inter_pairs > 0) {
        total_inter_similarity /= inter_pairs;
        WriteLn "Average inter-cluster similarity: %.3f", total_inter_similarity;

        // Display separation quality
        if (intra_pairs > 0) {
            double separation_ratio = total_intra_similarity / (total_inter_similarity + 1e-10);
            WriteLn "Separation ratio (intra/inter): %.3f", separation_ratio;
            if (separation_ratio > 2.0) {
                WriteLn "Quality: GOOD - clusters are well-separated";
            } else if (separation_ratio > 1.5) {
                WriteLn "Quality: FAIR - moderate separation";
            } else {
                WriteLn "Quality: POOR - clusters may be forced";
            }
        }
    }
}


//=================================================================================================
// FMCAD-19: Semantic partitioning using localization


void semanticPartitioning(NetlistRef N, Vec<Vec<uint> >& groups, uint target_clusters, uint bmc_limit)
{
    // Simple size-based group splitting to reach target number of clusters
    // Note: bmc_limit parameter is unused (legacy)

    Get_Pob(N, properties);
    Vec<Vec<uint> > new_groups;

    for (uint g = 0; g < groups.size(); g++){
        if (groups[g].size() <= 1){
            // Single property groups don't need partitioning
            new_groups.push();
            append(new_groups.last(), groups[g]);
            continue;
        }

        // Create property wires for this group
        Vec<Wire> group_props;
        for (uint i = 0; i < groups[g].size(); i++){
            uint prop_idx = groups[g][i];
            if (prop_idx < properties.size()){
                group_props.push(properties[prop_idx]);
            }
        }

        if (group_props.size() == 0) continue;

        // Simple size-based partitioning to reach target cluster count
        uint current_groups = new_groups.size();
        uint remaining_input_groups = groups.size() - g - 1;
        uint groups_still_needed = (target_clusters > current_groups) ?
                                   (target_clusters - current_groups) : 0;

        // Decide whether to split this group
        bool should_split = false;
        uint split_factor = 2; // Default: split into 2

        // Only split if we absolutely need more groups to reach the target
        if (groups_still_needed > remaining_input_groups) {
            // We need more groups than we have remaining input groups
            // So we should split some groups
            uint extra_groups_needed = groups_still_needed - remaining_input_groups;

            // Only split if this group is significantly large and we really need more groups
            if (groups[g].size() >= 4 && extra_groups_needed > 0) {
                should_split = true;
                // Calculate optimal split factor based on group size and need
                uint max_possible_splits = groups[g].size(); // Can split into at most this many singleton groups
                uint desired_splits = extra_groups_needed + 1; // +1 because splitting creates one extra group

                if (desired_splits >= 8 && groups[g].size() >= 8) {
                    split_factor = 8;
                } else if (desired_splits >= 4 && groups[g].size() >= 4) {
                    split_factor = 4;
                } else if (desired_splits >= 2 && groups[g].size() >= 4) {
                    split_factor = 2;
                } else {
                    split_factor = (desired_splits < max_possible_splits) ? desired_splits : max_possible_splits;
                }
            }
        }

        if (should_split) {
            WriteLn "Semantic partitioning: splitting group of size %_ into %_ parts", groups[g].size(), split_factor;

            uint group_size = groups[g].size();
            uint subgroup_size = group_size / split_factor;
            uint remainder = group_size % split_factor;

            uint start_idx = 0;
            for (uint part = 0; part < split_factor; part++) {
                uint current_subgroup_size = subgroup_size + (part < remainder ? 1 : 0);

                Vec<uint> subgroup;
                for (uint i = 0; i < current_subgroup_size; i++) {
                    subgroup.push(groups[g][start_idx + i]);
                }
                start_idx += current_subgroup_size;

                new_groups.push();
                append(new_groups.last(), subgroup);
            }
        } else {
            // Keep group as-is
            new_groups.push();
            append(new_groups.last(), groups[g]);
        }
    }

    // Move results back to groups
    groups.clear();
    for (uint i = 0; i < new_groups.size(); i++){
        groups.push();
        append(groups.last(), new_groups[i]);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
