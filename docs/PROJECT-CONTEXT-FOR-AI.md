# VTA Simulator — Complete Project Context

> **Purpose of this document.** This is a self-contained briefing for an AI assistant (or a new
> team member) that has *no prior context* about this project. It explains the goal, the two
> codebases involved, every important file, the architecture, the instruction format, the compute
> module, the memory system being integrated, and the agreed plan. Read it top to bottom and you
> will have the full picture needed to help with the integration and testing work.

---

## 0. TL;DR (read this first)

- This is a university (Uni Siegen) embedded-systems project (**PEP**) re-implementing the **Apache
  TVM VTA** (Versatile Tensor Accelerator) as a **SystemC** simulator.
- The project is based on the paper **"Time-Triggered Inference on FPGAs" (Bebawy et al., Uni
  Siegen, IEEE ETFA 2024)** and the original VTA report (arXiv:1807.04188).
- There are **two codebases**:
  1. **`VTA-Simulator-main/`** — the *real, whole project*. Has the fetcher, instruction model,
     all three modules (Load, Compute, Store), and the FIFO interconnect. **It had no memory.**
  2. **`AXI-Full-Memory-main/`** — a *teammate's contribution*: just the **memory** (a 64 KB AXI
     slave + interconnect). Its `load/compute/store` module files are throwaway test dummies.
- **The task:** plug a real memory into our simulator so the modules can read/write actual data,
  then fill in the functional computation (GEMM / ALU) in the Compute module.
- **Two architectural decisions already made by the team:**
  1. Build the **conventional VTA** (with dependency queues) **now**; the *time-triggered* VTA
     (TT-VTA, dispatcher-based) is **future work**.
  2. **Use FIFOs** for data movement (the project is already entirely FIFO-based), **not** the
     teammate's clock-driven AXI burst protocol.
- **Hard constraints:** (a) do **not** add anything to `.h` files unless unavoidable — the
  professor said everything needed is already declared; (b) the only function that needs the actual
  computation is `ComputeModule::dependencies_received()`.

---

## 1. Background: what VTA is and what this project does

**VTA (Versatile Tensor Accelerator)** is an open-source deep-learning accelerator from Apache TVM,
originally written in Vivado HLS C++ (`vta.cc`) for FPGAs. It is a **decoupled access-execute**
design with four modules that run concurrently and synchronize through FIFO queues + shared SRAM:

- **Fetch** — reads the instruction stream from DRAM, decodes the opcode, routes each instruction
  to the Load, Compute, or Store command queue.
- **Load** — DMA-copies input activations and weights from DRAM into on-chip INP/WGT SRAM.
- **Compute** — the **GEMM core** (matrix multiply) + **Tensor ALU** (elementwise ops: add, max,
  min, shift). Also loads micro-ops (uops) and bias/accumulator data from DRAM.
- **Store** — DMA-copies results from on-chip OUT SRAM back to DRAM.

**The paper's contribution (TT-VTA):** the conventional VTA is *event-triggered* — modules wait on
dependency-token queues, which makes timing non-deterministic (bad for safety-critical systems).
The paper replaces the dependency queues with a **Time-Triggered Dispatcher** driven by a Global
Time Base, using **DRAM Locks** to grant memory access in scheduled time windows. Our project
implements the **conventional** version first; TT-VTA is the future extension.

**What our simulator is:** a SystemC model that executes the VTA instruction stream and (now) will
perform the actual tensor math. Historically it functioned more as a timing/schedule generator; the
current work is to make it functionally compute real data read from a real memory.

### Key VTA hardware reference (default configuration)

| Concept | Value | Notes |
|---|---|---|
| Instruction width | **128 bits** | stored as two 64-bit words (`sc_int<64>` ×2) in this project |
| Micro-op (uop) width | **32 bits** | packs three tile indices: dst, src, wgt |
| `VTA_BATCH` | 1 | batch dimension |
| `VTA_BLOCK_IN` | **16** | input channels per tile (the GEMM "K" / dot-product length) |
| `VTA_BLOCK_OUT` | **16** | output channels per tile |
| `INP_WIDTH` / `WGT_WIDTH` / `OUT_WIDTH` | **8 bits** (int8) | activations, weights, outputs |
| `ACC_WIDTH` | **32 bits** (int32) | accumulator |
| Opcodes | LOAD=0, STORE=1, GEMM=2, FINISH=3, ALU=4 | 3-bit field |
| Memory IDs | UOP=0, WGT=1, INP=2, ACC=3, OUT=4, ACC_8BIT=5 | which SRAM a LOAD/STORE targets |
| ALU opcodes | MIN=0, MAX=1, ADD=2, SHR=3, MUL=4 | 3-bit field |
| Dependency queues | l2g, g2l, s2g, g2s | one boolean token per direction |

> Note: the real VTA AXI bus is 64-bit by default (configurable); the "512" sometimes seen is
> `VTA_ACC_MATRIX_WIDTH` (32 bits × 16 channels), **not** the bus width. The teammate's memory uses
> a 32-bit data word.

The GEMM tensor intrinsic per micro-op: `(1×16) input tile × (16×16) weight tile → (1×16)
accumulator tile`, with the inner reduction summing over the 16 input channels.

---

## 2. The two codebases and their true relationship

```
E:\Uni Siegen\PEP\
├── VTA-Simulator-main\        ← THE REAL PROJECT (our work). Has everything except memory.
├── AXI-Full-Memory-main\      ← Teammate's MEMORY ONLY. Its module files are test dummies.
├── vta.cc, vta.h              ← Reference: original Apache TVM HLS C++ design (source of truth)
└── Paper\                     ← The research papers this project is based on
    ├── Time-Triggered_Inference_on_FPGAs (1).pdf   (THE key paper, same author as our code)
    ├── Superscalar_Time-Triggered_Versatile-Tensor_Accelerator (1).pdf
    ├── VTA.pdf
    └── 1807.04188v3.pdf       (original VTA technical report)
```

**Critical clarification:** Earlier confusion treated the AXI codebase as "the system we plug into."
That is backwards. **Our `VTA-Simulator-main` is the whole project.** The teammate built only the
**memory** (`axi_lite_slave.h`) plus an interconnect; her `load_module.h` / `compute_module.h` /
`store_module.h` are *dummy masters she wrote only to test her memory* and are discarded. We take
her memory and connect it into our project.

---

## 3. Full file inventory of `VTA-Simulator-main`

### Top level
- **`main.cpp`** — entry point. Parses the model CSV, builds the encoded instruction map, constructs
  the `VTA` top module, runs `sc_start()`.
- **`CMakeLists.txt`, `CMakePresets.json`, `build.cmd`** — build setup (CMake + vcpkg; SystemC dep).
- **`README.md`** — project documentation.
- **`baseline.txt`** — reference output for comparison.

### `config/`
- **`config.json`** — runtime configuration (selected model, platform timing parameters, etc.).

### `data/models/`
- **`resnet_18.csv`, `resnet_34.csv`, `resnet_50.csv`, `resnet_101.csv`** — the actual instruction
  streams, one row per instruction, exported from TVM/VTA running these ResNet models. This is the
  *real* instruction source. `resnet_101.csv` = 103 layers, the main test workload.

### `include/` (headers) and `src/` (implementations) — paired by module

| Area | Header (`include/`) | Source (`src/`) | Role |
|---|---|---|---|
| **Config** | `config/vta_config.h` | — | All VTA constants: opcodes, memory IDs, ALU opcodes, bit-field widths, transaction layout positions. **Single source of truth for constants.** |
| | `config/config.h` | `config/config.cpp`, `config/config_json.cpp` | Loads `config.json`; `PlatformConfig` singleton (timing params like `load_cycles`). |
| | `config/files_manager.h` | — | File path helpers. |
| **Instruction** | `instruction/instruction.h` | `instruction/instruction.cpp` | The `Instruction` class: decode/encode the 128-bit word; all getters. **Central data structure.** |
| | `instruction/parser.h` | `instruction/parser.cpp` | Parses the model CSV into encoded instruction tuples. |
| **Type** | `type/type.h` | `type/type.cpp` | Field-position constants and enums (`InstrType`), transaction layout. |
| | `type/instruction_schedule.h` | — | Schedule data type (timing). |
| **Simulation** | `simulation/vta.h` | `simulation/vta.cpp` | **Top-level module.** Instantiates ARM, Fetcher, 3 instruction FIFOs, Load/Compute/Store, 4 dependency FIFOs; wires all `valid/ready/data/end` signals. |
| | `simulation/module.h` | `simulation/module.cpp` | **`Module` base class** for Load/Compute/Store. Defines the instruction-execution lifecycle (events + pure-virtual hooks). |
| | `simulation/compute_module.h` | `simulation/compute_module.cpp` | **`ComputeModule`** — GEMM/ALU. The file we modify. |
| | `simulation/load_module.h` | `simulation/load_module.cpp` | `LoadModule` — currently timing only (`finish.notify(latency())`). |
| | `simulation/store_module.h` | `simulation/store_module.cpp` | `StoreModule` — currently timing only. |
| | `simulation/fetcher.h` | `simulation/fetcher.cpp` | `Fetcher` — reads encoded instructions, pushes to the per-module instruction FIFOs via handshake. |
| | `simulation/queue.h` | `simulation/queue.cpp` | **`Queue`** — the FIFO primitive (see §4.1). Used for both instruction and dependency queues. |
| | `simulation/arm.h` | `simulation/arm.cpp` | `ARM` — models the host CPU that kicks off the fetcher and receives the final trigger. |
| **Communication** | `communication/communication.h`, `communication/message.h` | `communication/communication.cpp` | ZeroMQ-based messaging (used for external schedule communication; not central to compute). |
| **Utilities** | `utilities/utilities.h` | `utilities/utilities.cpp` | Helpers: `int_to_hex`, `concatStringsModern`, etc. |

### `archive/` (old/experimental — not built)
- `include_queues/` + `src_queues/` — earlier standalone implementations of the four dependency
  queues: **`l2c_queue`, `c2l_queue`, `s2c_queue`, `c2s_queue`** (load↔compute, store↔compute).
  Superseded by the generic `Queue` class but useful to understand naming.
- `fir.*`, `tb.*` — unrelated experiments.

---

## 4. Architecture: a FIFO-based, event-driven SystemC simulator

### 4.1 The FIFO primitive — `Queue` (`include/simulation/queue.h`, `src/simulation/queue.cpp`)

The `Queue` class is the project's universal channel. It is a **FIFO** wrapping a
`std::queue<sc_int<64>>` (capacity 128) behind a 4-wire handshake on each side:

```
input side:   in_vld  (in),  in_rdy  (out), in_data  (in 64-bit),  in_end  (in)
output side:  out_vld (out), out_rdy (in),  out_data (out 64-bit), out_end (out)
```

- `vld`/`rdy` = valid/ready handshake (transfer happens when both are high).
- `data` = one 64-bit payload word.
- `end` = marks the last word of a multi-word item (an instruction is 128 bits = **two** 64-bit
  words, so `instruction_queue=true` queues pair words together).

**This is the FIFO the supervisor means by "go with FIFO."** Every data path in the system already
uses this interface. New memory-data movement should use the same `valid/ready/data/end` style — not
the teammate's clock-driven AXI `AWVALID/WVALID/WREADY/...` burst protocol.

### 4.2 The `Module` base class (`include/simulation/module.h`)

Load, Compute, and Store all inherit from `Module`. It provides:

- **Instruction-input FIFO port:** `i_queue_vld / i_queue_rdy / i_queue_data / i_queue_end` —
  receives instructions from the Fetcher's per-module instruction FIFO.
- **`Instruction* current`** — the instruction currently being executed.
- **A lifecycle driven by `sc_event`s and pure-virtual hooks** (each module implements them):

| Event | Hook (pure virtual) | Meaning |
|---|---|---|
| `fetch` | `fetch_instruction()` (concrete) | pull next instruction from the instruction FIFO |
| `check_dep` | `check_dependencies()` | does this instruction need to *receive* dependency tokens first? |
| `receive_dep` | `receive_dependencies()` | tokens arrived; gate to proceed |
| `dep_received` | **`dependencies_received()`** | **DO THE WORK** (compute / load / store), then `finish.notify(latency())` |
| `finish` | `finalize_instruction()` | instruction done; handle FINISH specially |
| `push_dep` | `push_dependencies()` | send dependency tokens to downstream modules |
| `dep_pushed` | `dependencies_pushed()` | tokens sent; clean up and fetch next |

- **`virtual sc_time latency() = 0`** — each module returns the simulated execution time of `current`.

**The single most important fact for the compute work:** the *actual functional computation* belongs
in **`dependencies_received()`**. Everything else (fetch, dependency handshakes, finalize) is already
implemented and must not be disturbed.

### 4.3 The Compute module ports (`include/simulation/compute_module.h`)

`ComputeModule : Module` adds the four dependency-FIFO endpoints + a trigger out:

| Port group | Direction | Connects to | VTA meaning |
|---|---|---|---|
| `pull_prev_*` | in | `l2c_queue` | receive token from **Load** (load→compute, "l2g") |
| `push_prev_*` | out | `c2l_queue` | send token to **Load** (compute→load, "g2l") |
| `pull_next_*` | in | `s2c_queue` | receive token from **Store** (store→compute, "s2g") |
| `push_next_*` | out | `c2s_queue` | send token to **Store** (compute→store, "g2s") |
| `out_trig` | out | ARM | signals layer/run completion |

It declares the private state and event handlers for the handshakes (all implemented in the `.cpp`).
**No memory arrays are declared in this header** (correct — buffers live in the `.cpp` only, per the
no-`.h`-edits rule).

### 4.4 The top-level wiring (`include/simulation/vta.h`, `src/simulation/vta.cpp`)

The `VTA` module instantiates and solders together:

- `ARM *arm` — host CPU model.
- `Fetcher *fetcher` — instruction dispatcher.
- **Three instruction FIFOs:** `l_instructions_queue`, `c_instructions_queue`,
  `s_instructions_queue` (Fetcher → each module).
- **Three modules:** `LoadModule *load`, `ComputeModule *compute`, `StoreModule *store`.
- **Four dependency FIFOs:** `l2c_queue`, `c2l_queue`, `s2c_queue`, `c2s_queue`.

All connections are `sc_signal<bool>` / `sc_signal<sc_int<64>>` for the `vld/rdy/data/end` wires.
This is the diagram from the paper (Fig. 1, "Conventional VTA") rendered in SystemC.

### 4.5 Data flow today

```
ARM → triggers Fetcher
Fetcher → reads encoded instructions → pushes (vld/rdy/data/end) into the 3 instruction FIFOs
Each Module → fetch_instruction() pulls from its instruction FIFO → reconstructs Instruction
            → check/receive dependency tokens via the l2c/c2l/s2c/c2s FIFOs
            → dependencies_received()  ← (Load/Store: timing only; Compute: SHOULD compute)
            → finish.notify(latency()) → finalize → push dependency tokens → fetch next
```

**What is missing:** there is no memory, so `inp_mem`/`wgt_mem`/`uop_mem`/etc. have no real data and
the compute math has had nothing to operate on. That is exactly what the memory integration fixes.

---

## 5. The instruction model (`instruction.cpp` / `instruction.h`)

Every instruction is **128 bits = two `sc_int<64>` words** (`part_1`, `part_2`). The
`Instruction(const std::tuple<sc_int<64>, sc_int<64>>&)` constructor decodes them MSB-first.

### Common header (all instruction types)
- bits [63:61] **opcode** (3 bits) → LOAD/STORE/GEMM/FINISH/ALU
- next 4 bits: **dependency flags** `pop_prev, pop_next, push_prev, push_next`

### LOAD / STORE layout (`VTAMemInsn`)
Decoded fields: `memory_type` (3 bits → UOP/WGT/INP/ACC/OUT), `sram` (16-bit SRAM base, stored as hex
string), `dram` (32-bit DRAM base, hex string), then in `part_2`: `y_size` (16), `x_size` (16),
`stride` (16), `y0_pad`/`y1_pad`/`x0_pad`/`x1_pad` (4 each). **This is where the "where is the data"
information comes from** — the data location and shape are carried by the instruction.

### GEMM / FINISH layout (`VTAGemInsn`)
`reset_out` (1), `range_0` = uop_bgn (13), `range_1` = uop_end (14), `gemm_outer_loop_iter` (14),
`gemm_inner_loop_iter` (14); in `part_2`: `gemm_outer_loop_acc` (dst factor out, 11),
`gemm_inner_loop_acc` (11), `gemm_outer_loop_inp` (src factor out, 11), `gemm_inner_loop_inp` (11),
`gemm_outer_loop_wgt` (10), `gemm_inner_loop_wgt` (10).

### ALU layout (`VTAAluInsn`)
`reset_out` (1), `range_0` (13), `range_1` (14), and **(IMPORTANT BUG) the loop iteration counts are
stored into `gemm_outer_loop_iter` / `gemm_inner_loop_iter`**, *not* the `alu_*` fields. In `part_2`:
`alu_outer_loop_dst` (11), `alu_inner_loop_dst` (11), `alu_outer_loop_src` (11),
`alu_inner_loop_src` (11), and `alu_opcode` (3) → the instruction `name` is set from the opcode.

> **Decoder bug to work around in Compute:** `get_outer_loop_iter()`/`get_inner_loop_iter()` for ALU
> return `alu_outer_loop_iter`/`alu_inner_loop_iter`, which are never populated by the binary decoder
> (they stay at their default `-1`). The decoder actually wrote the values into
> `gemm_outer_loop_iter`/`gemm_inner_loop_iter`. **Therefore, in the Compute ALU path, use
> `get_gemm_outer_loop_iter()` and `get_gemm_inner_loop_iter()` — not `get_outer_loop_iter()`.**

### Static maps (public, defined in `instruction.cpp`)
- `Instruction::instruction_type` : name string → `Opcode`
- `Instruction::memory_id` : name string → `MemoryID`
- `Instruction::alu_opcode` : name string → `AluOpcode` (use this for clean ALU opcode lookup)

### The 10 instruction names the Compute module sees
`LOAD UOP`, `LOAD ACC`, `GEMM`, `ALU - add`, `ALU - add imm`, `ALU - max imm`, `ALU - min imm`,
`ALU - shr`, `NOP-COMPUTE-STAGE`, `FINISH`.

> Note: `use_imm` and the 16-bit `imm` value are **not** decoded by the binary constructor. Detect
> immediate ALU ops via `name.find("imm") != npos` and assume `imm = 0` (correct for ResNet ReLU =
> `max(x,0)`).

---

## 6. The Compute module in depth (`src/simulation/compute_module.cpp`)

### Already implemented (do not change)
- **`latency()`** — returns simulated execution time for all 10 instruction types (LOAD ACC, GEMM,
  ALU, NOP, FINISH, etc.). Timing is already correct regardless of what computation runs.
- **`check_dependencies()` / `receive_dependencies()`** — gate on the `pull_prev`/`pull_next` tokens.
- **`finalize_instruction()`** — on `FINISH`, prints "FINISH LAYER N" and resets state; else fires
  `push_dep`.
- **`push_dependencies()` / `dependencies_pushed()`** — send `push_prev`/`push_next` tokens, then
  fetch the next instruction.
- All the `*_handler()` methods for the FIFO handshakes.

### The one function to implement: `dependencies_received()`
Currently a stub: `finish.notify(latency());`. It must, based on `current->get_name()`:

- **`LOAD UOP`** → copy `x_size` 32-bit uop words from DRAM (`dram` base) into `uop_mem` (`sram`
  base). `std::memcpy(&uop_mem[sram], &dram_uops[dram], x_size*sizeof(uint32_t))`.
- **`LOAD ACC`** → 2D strided copy (with optional padding) of int32 biases from DRAM into `acc_mem`.
- **`GEMM`** → triple nested loop (outer × inner × uop-range). For each uop: decode dst/src/wgt tile
  indices (mask+shift), add loop offsets, then for each output channel `oc` (0..15) sum
  `inp_mem[src][ic] * wgt_mem[wgt][oc*16+ic]` over `ic` (0..15), accumulate into `acc_mem[dst][oc]`,
  honor `reset_out`, and write the int8-truncated result into `out_mem[dst][oc]`.
- **`ALU - *`** → triple nested loop; per element `oc`: read `src_0 = acc_mem[dst][oc]`, `src_1 =
  use_imm ? imm : acc_mem[src][oc]`, apply MAX/MIN/ADD/SHR, write back to `acc_mem` and int8-truncate
  to `out_mem`. **Use `get_gemm_outer/inner_loop_iter()` for the loop bounds (see decoder bug).**
- **`NOP-COMPUTE-STAGE` / `FINISH`** → no computation.
- **Always end with** `finish.notify(latency());`.

### Constants and buffers (defined at top of the `.cpp`, NOT in any `.h`)
```cpp
constexpr int VTA_BLOCK_IN = 16, VTA_BLOCK_OUT = 16;
constexpr int UOP_BUFF_DEPTH = (1 << vta_config::UOP_DST_WIDTH);   // 2048
constexpr int ACC_BUFF_DEPTH = (1 << vta_config::UOP_DST_WIDTH);   // 2048
constexpr int INP_BUFF_DEPTH = (1 << vta_config::UOP_SRC_WIDTH);   // 2048
constexpr int WGT_BUFF_DEPTH = (1 << vta_config::UOP_WGT_WIDTH);   // 1024
constexpr uint32_t DST_MASK = (1u << vta_config::UOP_DST_WIDTH) - 1u; // 0x7FF
constexpr uint32_t SRC_MASK = (1u << vta_config::UOP_SRC_WIDTH) - 1u; // 0x7FF
constexpr uint32_t WGT_MASK = (1u << vta_config::UOP_WGT_WIDTH) - 1u; // 0x3FF

static uint32_t uop_mem[UOP_BUFF_DEPTH];                 // private to Compute
static int32_t  acc_mem[ACC_BUFF_DEPTH][VTA_BLOCK_OUT];  // private to Compute
int8_t inp_mem[INP_BUFF_DEPTH][VTA_BLOCK_IN];            // shared (filled by Load)
int8_t wgt_mem[WGT_BUFF_DEPTH][VTA_BLOCK_OUT*VTA_BLOCK_IN];
int8_t out_mem[ACC_BUFF_DEPTH][VTA_BLOCK_OUT];           // shared (read by Store)
```
The micro-op 32-bit layout: bits `[10:0]`=dst, `[21:11]`=src, `[31:22]`=wgt.

This matches the original `gemm()` and `alu()` functions in `vta.cc` (the HLS reference), with the
`bus_T = ap_uint<512>` packing replaced by plain unpacked element arrays (no AXI bus in the math
itself).

---

## 7. The memory system being integrated (`AXI-Full-Memory-main`)

A teammate built a verified AXI4 memory subsystem. **We use the memory; we discard her dummy module
templates.** Files:

- **`axi_lite_slave.h`** — the **64 KB main memory** (`sc_uint<8> memory_array[65536]`), byte
  addressable, with read and write state machines. Stores 4 bytes per 32-bit word (little-endian:
  bits [7:0] at addr+0 … [31:24] at addr+3). Has **dynamic 2D config pins** `CFG_WIDTH`,
  `CFG_STRIDE` (default 16 and 32): after writing `CFG_WIDTH` bytes in a row, it auto-jumps by
  `CFG_STRIDE` to the next row — i.e., the hardware handles 2D tensor padding so software sends data
  linearly. Power-on fills memory with random bytes (simulates real silicon).
- **`axi_interconnect.h`** — round-robin **arbiter** for 3 masters (M0/M1/M2). Includes `AWLOCK` /
  `ARLOCK` pins = the paper's **DRAM Lock** for time-triggered preemption (future TT-VTA use). For
  conventional VTA, the standard round-robin path is what matters.
- **`vta_system.h`** — top-level "motherboard": instantiates arbiter + memory, exposes `m0_`/`m1_`/
  `m2_` trace wires for Load/Compute/Store. Has commented-out "expansion sockets" for the real
  modules.
- **`main.cpp`** — her standalone test harness (drives the dummy masters). Not used in our project.
- **`load_module.h` / `compute_module.h` / `store_module.h`** — **dummy test masters** (each a single
  `drive_test()` SC_THREAD writing signature bytes 0xAA/0xBB/0xCC). **Discarded** — our real modules
  replace them.

### Her integration guide (verbatim intent)
1. Work in `vta_system.h`: `#include` your module, instantiate it, wire its AXI pins to the `m1_`
   traces (Compute = M1).
2. Don't modify `axi_interconnect.h`, `axi_lite_slave.h`, `main.cpp`.
3. The memory handles 2D padding via `CFG_WIDTH`/`CFG_STRIDE` — send data linearly.
4. Arbitration is automatic — just assert `AWVALID`/`ARVALID` when ready.
5. Keep the AXI valid/ready handshake skeleton; replace dummy payloads with real data.

### The reconciliation decision (IMPORTANT)
Her interface is **clock-driven AXI bursts**. Our project is **event-driven FIFO** (the `Queue`
class). The team chose to **go with FIFO**: data movement uses the existing `valid/ready/data/end`
FIFO interface, consistent with the rest of the simulator and with real VTA's `hls::stream` FIFOs.
The practical implication is that her AXI memory is either (a) wrapped behind a FIFO adapter, or (b)
re-expressed with a FIFO-style access port, so the modules talk to memory the same way they talk to
each other. The flat 64 KB array and its 2D width/stride behavior remain the storage model.

---

## 8. The integration task, restated

1. **Give the simulator a memory.** Add the 64 KB store (from `axi_lite_slave`) as the DRAM backing,
   accessed through a **FIFO** `valid/ready/data/end` interface (not raw AXI), consistent with the
   existing `Queue`-based design.
2. **Define a memory map** of the 64 KB for the five regions Compute needs (uop, acc/bias, inp, wgt,
   out). Note the constraint: in the teammate's memory there is one global `CFG_WIDTH`/`CFG_STRIDE`,
   so the layout / packing convention must be agreed across Load, Compute, Store.
3. **Define byte packing** on the 32-bit data word: input tile = 16 int8 = 4 words; accumulator tile
   = 16 int32 = 16 words; weight tile = 256 int8 = 64 words. Producer (Load) and consumer (Compute)
   must agree, and it must match `axi_lite_slave`'s 4-bytes-per-word little-endian storage.
4. **Fill `dependencies_received()`** in Compute with the GEMM/ALU/LOAD-UOP/LOAD-ACC logic (§6),
   reading operands from the buffers (now backed by memory) and writing results back.
5. **Instruction source:** the Fetcher + model CSV already provides real instructions. For *initial
   testing*, use a tiny hand-written instruction sequence + dummy data (see §9).

---

## 9. Recommended testing strategy (staged)

**Do not debug "is the math right?" and "is the data path right?" at the same time.**

- **Stage 1 — prove the math, no memory, no bus.** Inject tiny known values directly into
  `uop_mem`/`inp_mem`/`wgt_mem`, run a single GEMM with `outer=inner=1`, uop range `[0,1)`, and check
  by hand. Example: `inp_mem[0][ic]=1`, `wgt_mem[0][oc*16+ic]=1` → expect every `acc_mem[0][oc]=16`
  and `out_mem[0][oc]=16`. Then `inp=2,wgt=3` → expect 96. Then ALU `max imm 0` on
  `[-5,+7,...]` → expect `[0,7,...]` (ReLU). Drive this either by hand-building one `Instruction` in
  a small test `main`, or with a 4–5 line test CSV.
- **Stage 2 — prove the data path.** Replace hand-injected values with reads from the memory over the
  FIFO interface. Same instruction, same expected answer.
- **Stage 3 — real workload.** Let the Fetcher stream a full model CSV (start small: `resnet_18.csv`)
  end-to-end and compare against `baseline.txt`.

**Best instruction source for now:** a small hand-written deterministic sequence for Stages 1–2 (tiny
and verifiable by hand), graduating to the real model CSV for Stage 3. Do not hand-type ResNet.

---

## 10. Constraints and conventions (must follow)

- **No `.h` edits** unless truly unavoidable — the professor said everything needed is already
  declared. All new code (constants, buffers, logic) goes in the `.cpp`.
- **Only `dependencies_received()`** needs the functional computation; the rest of the lifecycle is
  done.
- **Conventional VTA now, TT-VTA later** — keep the dependency-queue model; do not remove it for the
  dispatcher yet.
- **FIFO for data movement** — use the existing `valid/ready/data/end` style, not clock-driven AXI.
- **Git commits:** never add a `Co-Authored-By` trailer.

---

## 11. Open questions to resolve with the team

1. **Memory map + packing:** exact byte offsets in the 64 KB for uop/acc/inp/wgt/out, and the packing
   on the 32-bit word — given a single global `CFG_WIDTH`/`CFG_STRIDE`.
2. **FIFO ↔ memory adapter:** how to present the 64 KB store through a FIFO `valid/ready/data/end`
   port (wrap the AXI slave, or add a FIFO access method).
3. **Who fills `inp_mem`/`wgt_mem`:** in real VTA the Load module DMAs them; in this shared-memory
   model, confirm Load writes them to memory and Compute reads them back (extra round trip) vs. Load
   writing Compute's buffers directly.
4. **Stage-1 harness location:** new `test_main.cpp` vs. a tiny test CSV under `data/models/`.

---

## 12. Reference: the original HLS source (`vta.cc`, `vta.h`)

Located at `E:\Uni Siegen\PEP\vta.cc` and `vta.h` — the **authoritative behavior** to match. Key
functions: `fetch()`, `load()`, `compute()`, `store()`, `gemm()`, `alu()`, `load_2d()`,
`load_pad_2d()`, `reset_mem()`, `read_tensor()`, `write_tensor()`. In `vta.cc`, `uop_mem` and
`acc_mem` are `static` inside `compute()` (private on-chip SRAM); `inp_mem`/`wgt_mem`/`out_mem` are
declared in `vta()` and passed to all stages (shared). The four `hls::stream<bool>` dependency queues
(`l2g`, `g2l`, `s2g`, `g2s`) are the FIFOs our `Queue` class corresponds to. Our SystemC compute math
mirrors `gemm()`/`alu()` exactly, dropping the `bus_T = ap_uint<512>` AXI packing in favor of plain
unpacked element arrays.

---

*End of context document.*
