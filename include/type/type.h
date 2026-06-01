///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// License Agreement ///////////////////////////////////////////////
// Module: Time-Triggered Schedule Generator for VTA (TTVTA-Simulator)
// file: type.h
// Developer: Yosab Bebawy
// Date: 01.05.2023
// Contact data: yosab.bebawy@uni-siegen.de
// distribution Rights: only reserved for Yosab Bebawy (@Bebawy)
// Copyrights © reserved for Mr. Bebawy
// License: This program is NOT free software. 
//          - You can NOT redistribute it and/or modify it without the agreement of Mr. Bebawy.
//          - This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
//            without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
////////////////////////////////// End of License Agreement ///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef TYPE_H
#define TYPE_H 

#include <fstream>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm> 
#include "utilities.h"

// instrcuction parameters //
#define layerPos 0
#define pcPos 1
#define instNamePos 2
#define moduleNamePos 3
#define pop_prevPos 4
#define pop_nextPos 5
#define push_prevPos 6
#define push_nextPos 7
#define l2g_queuePos 8
#define g2l_queuePos 9
#define s2g_queuePos 10
#define g2s_queuePos 11
#define abstractPos 12
#define gtb_start_timePos 13
#define gtb_end_timePos 14
#define total_ticksPos 15
#define dependent_on_pcPos 16
#define dramPos 21
#define sramPos 22
#define y_sizePos 30
#define x_sizePos 31
#define stridePos 32
#define y0_padPos 33
#define y1_padPos 34
#define x0_padPos 35
#define x1_padPos 36
#define reset_out_pos 37
#define range_0Pos 38
#define range_1Pos 39
#define gemm_outer_loop_iterPos 40
#define gemm_inner_loop_iterPos 44
#define alu_outer_loop_iterPos 48
#define alu_inner_loop_iterPos 51
#define cachePos 54

// ttvta schedule configutartion //
#define nextentry_size 4
#define phase_size 8
#define id_size 1
#define branch_size 1
//#define reset 0
const int reset = 0;

typedef std::tuple<std::vector<std::vector<bool>>, std::vector<std::vector<int>>, std::vector<int>> MemoryDependencies;

struct Edge {
    public:
    std::string start;
    std::string end;

    Edge(std::string start, std::string end)
    {
        this->start = start;
        this->end = end;
    }
};

struct Node {
    public:
    std::string layer;
    std::string pc;
    std::string inst_name;
    std::string module;
    bool pop_prev;
    bool pop_next;
    bool push_prev;
    bool push_next;
    std::string dram;
    bool seq_dep;
};

struct MemoryAddress {
    public:
    std::string start;
    int x;
    int y;
    int stride;
    int word_size;
    int first_element;
    int last_element;
    int nr_of_hits;
    std::string dram_address_first_apperance;
    std::string dram_address_last_apperance;
    std::vector<std::pair<int, int>> batches;
    char type;
    
    MemoryAddress(std::string start, int x, int y, int stride, int word_size, char type = ' ', bool detailed = false) {
        this->start = start;
        this->x = x;
        this->y = y;
        this->stride = stride;
        this->word_size = word_size;
        this->nr_of_hits = 0;
        this->dram_address_first_apperance = "";
        this->dram_address_last_apperance = "";
        this->type = type;

        if (detailed) {
            int batch_size_in_byte = x * word_size;
            int stride_size_in_byte = stride * word_size;
            
            this->first_element = string_to_uint64(start);
            this->last_element = this->first_element + (y - 1) * stride_size_in_byte + batch_size_in_byte;

            for (int i = 0; i < y; i++) {
                int s = this->first_element + i * stride_size_in_byte;
                int e = s + batch_size_in_byte;
                this->batches.push_back({s, e});
            }
        }
    }

    int get_size() const {
        return x * y * word_size;
    }

    void hit(std::string apperance) {
        this->nr_of_hits++;
        if (this->dram_address_first_apperance == "")
            this->dram_address_first_apperance = apperance;
        this->dram_address_last_apperance = apperance;
    } 



    std::vector<std::pair<int, int>> execlude_intersection_intervals(MemoryAddress other_address)
    {
        std::vector<std::pair<int, int>> intersection_intervals;

        if (std::max(this->first_element, other_address.first_element) < std::min(this->last_element, other_address.last_element)) {
            int i = 0, j = 0;

            while (i < this->batches.size() && j < other_address.batches.size()) {
                std::pair<int, int> b1 = this->batches[i];
                std::pair<int, int> b2 = other_address.batches[j];

                int s1 = b1.first;
                int e1 = b1.second;

                int s2 = b2.first;
                int e2 = b2.second;

                int L = std::max(s1, s2);
                int R = std::min(e1, e2);

                if (L < R) { // valid intersection
                    intersection_intervals.push_back({L, R});
                    // execlude the intersection from the current interval
                    if (s1 == L && e1 == R) {
                        this->batches.erase(this->batches.begin() + i);
                        i--;
                    } else if (s1 < L && e1 > R) {
                        this->batches[i].second = L;
                        this->batches.insert(this->batches.begin() + i + 1, {R, e1});
                    } else if (s1 < L) {
                        this->batches[i].second = L;
                    } else if (e1 > R) {
                        this->batches[i].first = R;
                    } else {
                        std::cout<<"--------------------------";
                    }
                }

                // move the interval with the smaller endpoint
                if (e1 < e2) i++;
                else j++;
            }
        }
        if (intersection_intervals.size() > 0) {
            std::vector<std::pair<int, int>>::iterator it = this->batches.begin();
            while (it < this->batches.end()) {
                if (it->second <= it->first) {
                    this->batches.erase(it);
                } else {
                    it++;
                }
            }
        }
        
        return intersection_intervals;
    }
};

struct InstructionSechedule {
    public:
    long pc;
    long next_pc;
    long long start;
    long long end;
    
    InstructionSechedule(long pc, long next_pc, long long start, long long end) {
        this->pc = pc;
        this->next_pc = next_pc;
        this->start = start;
        this->end = end;
    }
};

struct Task {
    int id;
    long long duration;
    int priority;  // higher means schedule earlier
    std::vector<int> successors;
    int indegree = 0;
};

struct Schedule {
    int processor;
    double start, finish;
};

struct Scheduled {
    std::string processor;
    double start;
    double finish;
};

// Comparator for priority queue (max-heap based on priority)
struct ComparePriority {
    bool operator()(const Task* a, const Task* b) {
        return a->priority < b->priority;
    }
};


enum Granularity {
    layer,
    block
};

// constexpr Granularity granularityFromString(std::string_view str) {
//     if (str == "layer")     return Granularity::layer;
//     else                    return Granularity::block;
// }

#endif