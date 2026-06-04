#ifndef VTA_CONFIG
#define VTA_CONFIG

#pragma once

#include <cstdint>

namespace vta_config {

// ================================
// Transaction layout
// ================================
constexpr int NUM_BINS_PER_TRANSACTION = 64;

constexpr int LAYER_POS = 0;
constexpr int PC_POS = 1;
constexpr int INSTRUCTION_POS = 2;
constexpr int MODULE_POS = 3;
constexpr int POP_PREV_POS = 4;
constexpr int POP_NEXT_POS = 5;
constexpr int PUSH_PREV_POS = 6;
constexpr int PUSH_NEXT_POS = 7;
constexpr int L2G_QUEUE_POS = 8;
constexpr int G2L_QUEUE_POS = 9;
constexpr int S2G_QUEUE_POS = 10;
constexpr int G2S_QUEUE_POS = 11;
constexpr int ABSTRACT_POS = 12;
constexpr int GTB_START_TIME_POS = 13;
constexpr int GTB_END_TIME_POS = 14;
constexpr int TOTAL_TIME_POS = 15;
constexpr int DEPENDENT_ON_PC_POS = 16;
constexpr int DEP_FROM_LOAD_MODULE_POS = 17;
constexpr int DEP_FROM_COMPUTE_MODULE_POS = 18;
constexpr int DEP_FROM_STORE_MODULE_POS = 19;
constexpr int SCHEDULE_TIME_POS = 20;
constexpr int DRAM_ADDRESS_POS = 21;
constexpr int SRAM_ADDRESS_POS = 22;
constexpr int MEMORY_TYPE_POS = 23;
constexpr int TRIGGER_INSTANT_HEX_POS = 24;
constexpr int TRIGGER_INSTANT_DEC_POS = 25;
constexpr int TRANSACTION_INSTANT_HEX_POS = 26;
constexpr int DDR_TRANSACTION_SI_POS = 27;
constexpr int DDR_TRANSACTION_EI_POS = 28;
constexpr int WAIT_CYCLES_POS = 29;
constexpr int Y_SIZE_POS = 30;
constexpr int X_SIZE_POS = 31;
constexpr int STRIDE_POS = 32;
constexpr int Y0_PAD_POS = 33;
constexpr int Y1_PAD_POS = 34;
constexpr int X0_PAD_POS = 35;
constexpr int X1_PAD_POS = 36;
constexpr int RESET_OUT_POS = 37;
constexpr int RANGE_0_POS = 38;
constexpr int RANGE_1_POS = 39;

// GEMM
constexpr int GEMM_OUTER_ITER_POS = 40;
constexpr int GEMM_OUTER_WGT_POS = 41;
constexpr int GEMM_OUTER_INP_POS = 42;
constexpr int GEMM_OUTER_ACC_POS = 43;
constexpr int GEMM_INNER_ITER_POS = 44;
constexpr int GEMM_INNER_WGT_POS = 45;
constexpr int GEMM_INNER_INP_POS = 46;
constexpr int GEMM_INNER_ACC_POS = 47;

// ALU
constexpr int ALU_OUTER_ITER_POS = 48;
constexpr int ALU_OUTER_DST_POS = 49;
constexpr int ALU_OUTER_SRC_POS = 50;
constexpr int ALU_INNER_ITER_POS = 51;
constexpr int ALU_INNER_DST_POS = 52;
constexpr int ALU_INNER_SRC_POS = 53;

constexpr int CACHE_POS = 54;

// ================================
// Opcode definitions
// ================================
constexpr int OPCODE_WIDTH = 3;

enum class Opcode : uint8_t {
    LOAD   = 0,
    STORE  = 1,
    GEMM   = 2,
    FINISH = 3,
    ALU    = 4
};

// ================================
// Dependency flags
// ================================
constexpr int DEP_FLAGS_WIDTH = 4;

// ================================
// Memory types
// ================================
constexpr int MEMORY_TYPE_WIDTH = 3;

enum class MemoryID : uint8_t {
    UOP      = 0,
    WGT      = 1,
    INP      = 2,
    ACC      = 3,
    OUT      = 4,
    ACC_8BIT = 5
};

// ================================
// Width definitions
// ================================
constexpr int SRAM_BASE_WIDTH = 16;
constexpr int DRAM_BASE_WIDTH = 32;

constexpr int Y_SIZE_WIDTH = 16;
constexpr int X_SIZE_WIDTH = 16;
constexpr int X_STRIDE_WIDTH = 16;

constexpr int Y_PAD_0_WIDTH = 4;
constexpr int Y_PAD_1_WIDTH = 4;
constexpr int X_PAD_0_WIDTH = 4;
constexpr int X_PAD_1_WIDTH = 4;

constexpr int RESET_WIDTH = 1;
constexpr int UOP_BGN_WIDTH = 13;
constexpr int UOP_END_WIDTH = 14;
constexpr int ITER_OUT_WIDTH = 14;
constexpr int ITER_IN_WIDTH = 14;

constexpr int DST_FAC_OUT_WIDTH = 11;
constexpr int DST_FAC_IN_WIDTH = 11;

constexpr int GSRC_FAC_OUT_WIDTH = 11;
constexpr int GSRC_FAC_IN_WIDTH = 11;

constexpr int ASRC_FAC_OUT_WIDTH = 11;
constexpr int ASRC_FAC_IN_WIDTH = 11;

constexpr int WGT_FAC_OUT_WIDTH = 10;
constexpr int WGT_FAC_IN_WIDTH = 10;

// ================================
// ALU
// ================================
constexpr int ALU_OPCODE_WIDTH = 3;

enum class AluOpcode : uint8_t {
    MIN = 0,
    MAX = 1,
    ADD = 2,
    SHR = 3,
    MUL = 4
};

// ================================
// Immediate / UOP
// ================================
constexpr int USE_IMM_WIDTH = 1;
constexpr int IMM_WIDTH = 16;

constexpr int UOP_DST_WIDTH = 11;
constexpr int UOP_SRC_WIDTH = 11;
constexpr int UOP_WGT_WIDTH = 10;

} // namespace vta

#endif