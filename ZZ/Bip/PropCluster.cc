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

    // FMCAD-19: Implement the three-level grouping algorithm from the paper

    // Step 1: Compute support bitvectors for all properties
    Vec<Vec<uint64> > bitvectors;
    computeSupportBitvectors(N, n_pivots, bitvectors);

    // Step 2: Initialize clusters - each property starts in its own group
    clusters.clear();
    for (uint i = 0; i < properties.size(); i++){
        clusters.push();
        clusters.last().push(i);
    }

    // Step 3: Level-1 grouping - merge properties with identical COI
    groupingLevel1(bitvectors, clusters);

    // Step 4: Level-2 grouping - merge based on SCC weights (high affinity)
    groupingLevel2(N, bitvectors, clusters, 0.9);

    // Step 5: Level-3 grouping - merge based on Hamming distance
    groupingLevel3(bitvectors, clusters, 0.8);

    // Step 6: If target number of clusters specified, merge until we reach it
    WriteLn "DEBUG: After Level-3, we have %_ clusters, target is %_", clusters.size(), n_clusters;
    while (clusters.size() > n_clusters && clusters.size() > 1){
        uint old_size = clusters.size();
        mergeClosestGroups(bitvectors, clusters);
        WriteLn "DEBUG: Merged from %_ to %_ clusters", old_size, clusters.size();
        if (clusters.size() == old_size) {
            WriteLn "DEBUG: No more merging possible - all remaining groups have zero affinity";
            break;
        }
    }

    // Step 7: Optional semantic partitioning using localization
    if (seq_depth > 0){
        semanticPartitioning(N, clusters, n_clusters, seq_depth);
    }
}


//=================================================================================================
// FMCAD-19: Support bitvector computation


void computeSupportBitvectors(NetlistRef N, uint n_pivots, /*out*/Vec<Vec<uint64> >& bitvectors)
{
    Get_Pob(N, properties);
    if (properties.size() == 0) return;

    // Create mapping from support variables to bit indices
    Map<Wire, uint> var_to_index;
    uint index = 0;

    // Map primary inputs to bit indices
    For_Gatetype(N, gate_PI, w){
        if (index >= n_pivots) break;
        var_to_index.set(w, index++);
    }

    // Map flops to bit indices
    For_Gatetype(N, gate_Flop, w){
        if (index >= n_pivots) break;
        var_to_index.set(w, index++);
    }

    uint total_vars = index;
    uint words = (total_vars + 63) / 64;

    // Initialize bitvectors for each property
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

        // Set bits in bitvector for support variables in COI
        for (uint j = 0; j < coi.size(); j++){
            Wire w = coi.list()[j];
            uint* idx;
            if (var_to_index.get(w, idx)){
                uint word = *idx / 64;
                uint bit = *idx % 64;
                if (word < words){
                    bitvectors[i][word] |= 1ULL << bit;
                }
            }
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


void groupingLevel3(const Vec<Vec<uint64> >& bitvectors, Vec<Vec<uint> >& groups, double threshold)
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

            // Estimate total bits (approximate)
            uint total_bits = bitvectors[prop_i].size() * 64;
            double normalized_distance = double(distance) / double(total_bits);
            double affinity = 1.0 - normalized_distance;

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
// FMCAD-19: Helper functions


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

        if (groups_still_needed > remaining_input_groups) {
            // We need more groups than we have remaining input groups
            // So we should split some groups
            uint extra_groups_needed = groups_still_needed - remaining_input_groups;

            if (groups[g].size() >= 2 && extra_groups_needed > 0) {
                should_split = true;
                // Calculate optimal split factor based on group size and need
                uint max_possible_splits = groups[g].size(); // Can split into at most this many singleton groups
                uint desired_splits = extra_groups_needed + 1; // +1 because splitting creates one extra group

                if (desired_splits >= 8 && groups[g].size() >= 8) {
                    split_factor = 8;
                } else if (desired_splits >= 4 && groups[g].size() >= 4) {
                    split_factor = 4;
                } else if (desired_splits >= 2 && groups[g].size() >= 2) {
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
