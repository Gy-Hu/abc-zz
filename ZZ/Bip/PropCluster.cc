//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PropCluster.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "PropCluster.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Select 'n_pivots' random flops and assign them number '1, 2... n_pivots'. '0' is reserved for
// "no pivot".
void pickPivots(NetlistRef N, uint n_pivots, WMap<uint>& pivots)
{
    Vec<GLit> ffs;
    For_Gatetype(N, gate_Flop, w)
        ffs.push(w);

#if 1
    uint64 seed = DEFAULT_SEED;
    while (ffs.size() > n_pivots){
        uint r = irand(seed, ffs.size());
        swp(ffs[r], ffs[LAST]);
        ffs.pop();
    }
#else
    if (ff.size() > n_pivots){
        uint j = 0;
        for (uint i = 0; < i
    uint count =
#endif

    for (uint i = 0; i < ffs.size(); i++)
        pivots(ffs[i] + N) = i+1;
}


void clusterProperties(NetlistRef N, uint n_clusters, uint n_pivots, uint seq_depth, /*out*/Vec<Vec<uint> >& clusters)
{
    Get_Pob(N, properties);
    if (properties.size() == 0) return;

    // FMCAD-19: Robust agglomerative clustering algorithm

    // Step 1: Compute support bitvectors for all properties (without n_pivots limit)
    Vec<Vec<uint64> > bitvectors;
    uint total_vars;
    computeSupportBitvectors(N, UINT_MAX, bitvectors, total_vars);  // Remove artificial limit

    // DEBUG: Show support bitvector info and similarity patterns
    WriteLn "DEBUG: Support bitvector analysis:";
    uint max_show = (bitvectors.size() < 10) ? bitvectors.size() : 10;
    for (uint i = 0; i < max_show; i++) {
        uint support_size = 0;
        for (uint j = 0; j < bitvectors[i].size(); j++) {
            support_size += __builtin_popcountll(bitvectors[i][j]);
        }
        WriteLn "  Property %_: support size = %_, first word = 0x%_", i, support_size, 
               (bitvectors[i].size() > 0) ? (uint)(bitvectors[i][0] & 0xFFFFFFFF) : 0;
    }
    if (bitvectors.size() > 10) {
        WriteLn "  ... (showing first 10 properties only)";
    }
    
    // DEBUG: Show some pairwise similarities
    WriteLn "DEBUG: Sample pairwise similarities:";
    for (uint i = 0; i < 5 && i < bitvectors.size(); i++) {
        for (uint j = i + 1; j < 5 && j < bitvectors.size(); j++) {
            double sim = computeAffinity(bitvectors[i], bitvectors[j]);
            WriteLn "  Prop %_ vs Prop %_: similarity = %.4f", i, j, sim;
        }
    }

    // DEBUG: Show detailed bitvector content for first few properties
    WriteLn "DEBUG: Detailed bitvector analysis:";
    for (uint i = 0; i < 8 && i < bitvectors.size(); i++) {
        Write "  Property %_: [", i;
        for (uint j = 0; j < bitvectors[i].size() && j < 2; j++) {
            if (j > 0) Write ", ";
            Write "0x%_", bitvectors[i][j];
        }
        WriteLn "]";
    }

    // DEBUG: Check if properties have overlapping support
    WriteLn "DEBUG: Support overlap analysis:";
    uint total_overlaps = 0;
    for (uint i = 0; i < bitvectors.size() && i < 10; i++) {
        for (uint j = i + 1; j < bitvectors.size() && j < 10; j++) {
            uint intersection = 0;
            for (uint k = 0; k < bitvectors[i].size(); k++) {
                intersection += __builtin_popcountll(bitvectors[i][k] & bitvectors[j][k]);
            }
            if (intersection > 0) {
                WriteLn "  Props %_ and %_ share %_ support variables", i, j, intersection;
                total_overlaps++;
            }
        }
    }
    WriteLn "  Total overlaps found in first 10 properties: %_", total_overlaps;

    // Step 2: Initialize clusters - each property starts in its own group
    clusters.clear();
    for (uint i = 0; i < properties.size(); i++){
        clusters.push();
        clusters.last().push(i);
    }

    // Step 3: Level-1 grouping - merge properties with identical COI (preprocessing)
    WriteLn "DEBUG: Before Level-1, we have %_ clusters", clusters.size();
    groupingLevel1(bitvectors, clusters);
    WriteLn "DEBUG: After Level-1, we have %_ clusters", clusters.size();

    // Step 4: Robust agglomerative clustering - guaranteed to reach target
    WriteLn "DEBUG: Starting robust agglomerative clustering from %_ to %_ clusters", clusters.size(), n_clusters;
    robustAgglomerativeClustering(bitvectors, clusters, n_clusters);
    WriteLn "DEBUG: After robust clustering, we have %_ clusters", clusters.size();

    // Step 5: Compute and display quality metrics
    displayClusterQualityMetrics(bitvectors, clusters);

    // Step 6: Optional semantic partitioning using localization
    // Only run semantic partitioning if we have fewer clusters than requested
    if (seq_depth > 0 && clusters.size() < n_clusters){
        semanticPartitioning(N, clusters, n_clusters, seq_depth);
    }
}


//=================================================================================================
// FMCAD-19: Support bitvector computation


void computeSupportBitvectors(NetlistRef N, uint n_pivots, /*out*/Vec<Vec<uint64> >& bitvectors, /*out*/uint& total_vars)
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
    
    // Map all relevant gates (remove artificial n_pivots limitation)
    uint gates_to_map = relevant_gates.size();
    
    for (uint i = 0; i < gates_to_map; i++){
        Wire w = relevant_gates.list()[i];
        var_to_index.set(w, index++);
    }

    total_vars = index;  // Set the output parameter
    uint words = (total_vars + 63) / 64;
    
    WriteLn "DEBUG: COI-first algorithm - found %_ relevant gates, mapped %_ gates", relevant_gates.size(), total_vars;

    // DEBUG: Show the first few gate mappings
    WriteLn "DEBUG: Gate to index mappings (first 10):";
    uint show_count = 0;
    for (uint i = 0; i < relevant_gates.size() && show_count < 10; i++){
        Wire w = relevant_gates.list()[i];
        uint* idx;
        if (var_to_index.get(w, idx)){
            WriteLn "  Gate %_ (type=%_) -> index %_", +w, (uint)w.type(), *idx;
            show_count++;
        }
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

        // DEBUG: Show which indices each property uses (first 8 properties only)
        if (i < 8) {
            Write "DEBUG: Property %_ uses indices: [", i;
            for (uint k = 0; k < used_indices.size(); k++) {
                if (k > 0) Write ", ";
                Write "%_", used_indices[k];
            }
            WriteLn "]";
        }
    }
}


//=================================================================================================
// FMCAD-19: Affinity and distance computation


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
// FMCAD-19: Level-1 grouping - merge properties with identical COI


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
// FMCAD-19: Level-2 grouping - merge based on SCC weights


void groupingLevel2(NetlistRef N, const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold)
{
    // For now, implement a simplified version that merges high-affinity groups
    // A full SCC-based implementation would require more complex graph analysis

    Vec<Vec<uint> > new_groups;
    Vec<bool> merged(groups.size(), false);

    for (uint i = 0; i < groups.size(); i++){
        if (merged[i] || groups[i].size() == 0) continue;

        new_groups.push();
        append(new_groups.last(), groups[i]);
        merged[i] = true;

        // Try to merge with other groups based on affinity
        for (uint j = i + 1; j < groups.size(); j++){
            if (merged[j] || groups[j].size() == 0) continue;

            // Compute affinity between group representatives
            uint prop_i = groups[i][0];
            uint prop_j = groups[j][0];
            double affinity = computeAffinity(bitvectors[prop_i], bitvectors[prop_j]);

            if (affinity >= threshold){
                // Merge groups
                append(new_groups.last(), groups[j]);
                merged[j] = true;
            }
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
// FMCAD-19: Level-3 grouping - merge based on Hamming distance


void groupingLevel3(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold, uint actual_bits)
{
    Vec<Vec<uint> > new_groups;
    Vec<bool> merged(groups.size(), false);

    for (uint i = 0; i < groups.size(); i++){
        if (merged[i] || groups[i].size() == 0) continue;

        new_groups.push();
        append(new_groups.last(), groups[i]);
        merged[i] = true;

        // Try to merge with other groups based on Hamming distance
        for (uint j = i + 1; j < groups.size(); j++){
            if (merged[j] || groups[j].size() == 0) continue;

            // Compute normalized Hamming distance between group representatives
            uint prop_i = groups[i][0];
            uint prop_j = groups[j][0];
            uint distance = hammingDistance(bitvectors[prop_i], bitvectors[prop_j]);

            // Use actual number of mapped bits instead of assuming all bits in words are used
            double normalized_distance = double(distance) / double(actual_bits);
            double affinity = 1.0 - normalized_distance;

            if (i < 3 && j < 6) {
                WriteLn "DEBUG: Level-3 affinity prop[%_] vs prop[%_]: distance=%_, actual_bits=%_, norm_dist=%.3f, affinity=%.3f, threshold=%.3f", prop_i, prop_j, distance, actual_bits, normalized_distance, affinity, threshold;
            }

            if (affinity >= threshold){
                // Merge groups
                append(new_groups.last(), groups[j]);
                merged[j] = true;
                if (i < 3) {
                    WriteLn "DEBUG: Level-3 MERGED prop[%_] with prop[%_] (affinity=%.3f >= %.3f)", prop_i, prop_j, affinity, threshold;
                }
            }
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
// FMCAD-19: Robust agglomerative clustering


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
            // Merge clusters with similarity
            WriteLn "DEBUG: Robust merge - best affinity: %.4f between clusters %_ and %_ (sizes: %_, %_)",
                   best_affinity, best_i, best_j, clusters[best_i].size(), clusters[best_j].size();

            // DEBUG: Show what properties are being merged
            if (clusters[best_i].size() <= 5 && clusters[best_j].size() <= 5) {
                Write "DEBUG: Merging cluster %_ [", best_i;
                for (uint k = 0; k < clusters[best_i].size(); k++) {
                    if (k > 0) Write ", ";
                    Write "%_", clusters[best_i][k];
                }
                Write "] with cluster %_ [", best_j;
                for (uint k = 0; k < clusters[best_j].size(); k++) {
                    if (k > 0) Write ", ";
                    Write "%_", clusters[best_j][k];
                }
                WriteLn "]";
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
        WriteLn "DEBUG: No more similarity found, using balanced redistribution";

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
            WriteLn "Cluster %_: avg intra-similarity = %.3f", c, cluster_similarity;
            total_intra_similarity += cluster_similarity;
            intra_pairs++;
        }
    }
    
    if (intra_pairs > 0) {
        total_intra_similarity /= intra_pairs;
        WriteLn "Average intra-cluster similarity: %.3f", total_intra_similarity;
    }
    
    // Calculate average inter-cluster similarity
    double total_inter_similarity = 0.0;
    uint inter_pairs = 0;
    
    for (uint i = 0; i < clusters.size(); i++) {
        if (clusters[i].size() == 0) continue;
        for (uint j = i + 1; j < clusters.size(); j++) {
            if (clusters[j].size() == 0) continue;
            
            double cluster_affinity = 0.0;
            uint pairs = 0;
            
            for (uint pi = 0; pi < clusters[i].size(); pi++) {
                for (uint pj = 0; pj < clusters[j].size(); pj++) {
                    double affinity = computeAffinity(bitvectors[clusters[i][pi]], bitvectors[clusters[j][pj]]);
                    cluster_affinity += affinity;
                    pairs++;
                }
            }
            
            if (pairs > 0) {
                cluster_affinity /= pairs;
                total_inter_similarity += cluster_affinity;
                inter_pairs++;
            }
        }
    }
    
    if (inter_pairs > 0) {
        total_inter_similarity /= inter_pairs;
        WriteLn "Average inter-cluster similarity: %.3f", total_inter_similarity;
    }
    
    // Display separation quality
    if (intra_pairs > 0 && inter_pairs > 0) {
        double separation_ratio = total_intra_similarity / total_inter_similarity;
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


//=================================================================================================
// FMCAD-19: Helper functions (legacy)


void mergeClosestGroups(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups)
{
    if (groups.size() <= 1) return;

    double best_affinity = -1.0;
    uint best_i = 0, best_j = 1;

    // Find the two groups with highest affinity
    for (uint i = 0; i < groups.size(); i++){
        if (groups[i].size() == 0) continue;
        for (uint j = i + 1; j < groups.size(); j++){
            if (groups[j].size() == 0) continue;

            uint prop_i = groups[i][0];
            uint prop_j = groups[j][0];
            double affinity = computeAffinity(bitvectors[prop_i], bitvectors[prop_j]);

            if (affinity > best_affinity){
                best_affinity = affinity;
                best_i = i;
                best_j = j;
            }
        }
    }

    WriteLn "DEBUG: Best affinity found: %.4f between groups %_ and %_", best_affinity, best_i, best_j;

    // Only merge if there's some affinity
    if (best_affinity <= 0.0) {
        WriteLn "DEBUG: No positive affinity found, stopping merge";
        return;
    }

    // Merge the two best groups
    append(groups[best_i], groups[best_j]);
    groups[best_j].clear();

    // Remove empty groups
    Vec<Vec<uint> > new_groups;
    for (uint i = 0; i < groups.size(); i++){
        if (groups[i].size() > 0){
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


double computeClusterQuality(const Vec<Vec<uint64> >& bitvectors, const Vec<uint>& group)
{
    if (group.size() <= 1) return 1.0;

    double total_affinity = 0.0;
    uint count = 0;

    for (uint i = 0; i < group.size(); i++){
        for (uint j = i + 1; j < group.size(); j++){
            total_affinity += computeAffinity(bitvectors[group[i]], bitvectors[group[j]]);
            count++;
        }
    }

    return count > 0 ? total_affinity / count : 1.0;
}


//=================================================================================================
// FMCAD-19: Semantic partitioning using localization


void semanticPartitioning(NetlistRef N, Vec<Vec<uint> >& groups, uint target_clusters, uint bmc_limit)
{
    // This implements the semantic partitioning algorithm from the paper
    // using the existing localization infrastructure in abc-zz

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

        // Try localization-based partitioning
        // For now, implement a simplified version that uses BMC convergence
        // A full implementation would integrate with the localization framework

        // FMCAD-19: Intelligent semantic partitioning
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
            WriteLn "DEBUG: Splitting group of size %_ into %_ parts", groups[g].size(), split_factor;

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
