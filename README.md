# VTA-Simulator

A SystemC-based performance simulator for the **VTA (Versatile Tensor Accelerator)** —
a programmable deep-learning accelerator from the Apache TVM project.

---

## What This Project Does

This simulator **does not run a neural network mathematically**.
Instead, it models the *hardware timing and pipeline behaviour* of the VTA accelerator:
given a neural network broken down into VTA instructions, it answers the question:

> **"How many nanoseconds would the VTA hardware take to process each layer of this network?"**

It does this by simulating the fetch → load → compute → store hardware pipeline in
SystemC, accounting for instruction dependencies, pipeline parallelism, memory latency,
and cache behaviour — all configurable without recompiling.

---

## Background: What is VTA?

VTA stands for **Versatile Tensor Accelerator**. It is an open, programmable hardware
accelerator designed to run deep learning workloads (like ResNet image classifiers)
efficiently. VTA is part of the **Apache TVM** deep learning compiler stack.

The original VTA hardware design (`vta.h` / `vta.cc`) is written in **Vivado HLS C++**
(using `ap_int`, `hls::stream`, `#pragma HLS` — Xilinx-specific synthesis constructs)
and targets FPGAs. This project re-expresses that same hardware behaviour in
**SystemC** so it can be simulated on a normal PC to study its performance.

The four hardware stages in real VTA map directly to four SystemC modules here:

| VTA hardware stage | HLS function in `vta.cc` | SystemC module in this project |
|---|---|---|
| Fetch | `fetch()` | `Fetcher` |
| Load | `load()` | `LoadModule` |
| Compute (GEMM/ALU) | `compute()` | `ComputeModule` |
| Store | `store()` | `StoreModule` |

---

## How the Simulator Works (Step by Step)

### 1. Read the configuration
`main.cpp` reads `config/config.json` which controls which neural network model to
simulate and all hardware parameters (cache, pipeline counts, latency values).

### 2. Parse the model
`Parser` reads a model CSV file (e.g. `data/models/resnet_101.csv`).
Each **row in the CSV is one VTA instruction** — it encodes the operation type
(Load / Compute / Store), the memory addresses, loop iteration counts, and
dependency flags (`pop_prev`, `pop_next`, `push_prev`, `push_next`).
Instructions are grouped by layer and encoded into 64-bit words.

### 3. Build the hardware in SystemC
`VTA` (the top module, in `src/simulation/vta.cpp`) creates all the modules and
connects them with `sc_signal` wires — like soldering a circuit board in software.

### 4. Run the simulation
`sc_start()` starts the SystemC scheduler. The modules react to signals and events,
consuming simulated time at every step:

```
ARM ──trigger──► Fetcher ──routes by type──► [Load instr queue]    ──► LoadModule
                                           ──► [Compute instr queue] ──► ComputeModule
                                           ──► [Store instr queue]   ──► StoreModule

LoadModule ◄──── c2l queue ◄──── ComputeModule ◄──── s2c queue ◄──── StoreModule
LoadModule ────► l2c queue ────► ComputeModule ────► c2s queue ────► StoreModule
```

When a layer's special `FINISH` instruction is processed by `ComputeModule`, it prints:
```
211040 ns    ---------------------------- FINISH LAYER 0 ----------------------------
```
…and signals `ARM` to start the next layer.

### 5. What the output means
Each line shows the **simulated hardware time (nanoseconds)** at which that layer
finished. The difference between two consecutive lines is that layer's latency.
This is the primary research output — how hardware configuration affects performance.

---

## Module Architecture

### ARM (`include/simulation/arm.h`, `src/simulation/arm.cpp`)
Models the host CPU. It triggers the Fetcher to start a new layer, then waits for
`ComputeModule` to signal completion before starting the next.
Signals: `out_trig` (start), `in_trig` (done).

### Fetcher (`include/simulation/fetcher.h`, `src/simulation/fetcher.cpp`)
Reads all instructions for the current layer from a pre-parsed map and **routes each
instruction** to the correct queue based on its type (LOAD → load queue,
GEMM/ALU → compute queue, STORE → store queue).
Uses valid/ready handshake (`vld`/`rdy`) to flow-control each queue.

### Instruction Queues (`include/simulation/queue.h`, `src/simulation/queue.cpp`)
FIFO buffers between Fetcher and the execution modules (one each for load, compute,
store instructions). Also used for the dependency token queues between stages.
Each queue has `in_vld/in_rdy/in_data/in_end` and `out_vld/out_rdy/out_data/out_end`
ports — a standard hardware handshake protocol.

### LoadModule (`include/simulation/load_module.h`, `src/simulation/load_module.cpp`)
Processes Load instructions (moving data from DRAM into on-chip SRAM).
Latency is determined by the instruction's memory transfer size and `load_cycles` /
`load_weights_cycles` from config.
Synchronises with Compute via the **l2c** (load-to-compute) and **c2l**
(compute-to-load) dependency queues.

### ComputeModule (`include/simulation/compute_module.h`, `src/simulation/compute_module.cpp`)
Processes Compute instructions: GEMM (matrix multiply), ALU (activation functions like
ReLU), and LOAD ACC (load accumulator).

Latency formula per instruction type:
- **GEMM**: `(range_1 - range_0) × outer_loop_iter × inner_loop_iter + 100 ns`
- **ALU**: `2 × (range_1 - range_0) × outer_loop_iter × inner_loop_iter + 100 ns`
- **LOAD ACC**: based on tensor dimensions and `load_cycles` from config
- **NOP / FINISH**: 15 ns / 10 ns

The `FINISH` instruction prints the layer completion line and signals ARM.
Synchronises with Load via **l2c/c2l** queues and with Store via **c2s/s2c** queues.

### StoreModule (`include/simulation/store_module.h`, `src/simulation/store_module.cpp`)
Processes Store instructions (writing results from on-chip SRAM back to DRAM).
Latency based on transfer size and `store_cycles` from config.
Synchronises with Compute via the **c2s** (compute-to-store) and **s2c**
(store-to-compute) dependency queues.

### Dependency Queues (l2c, c2l, s2c, c2s)
These are the **key to pipeline correctness**. Because Load, Compute, and Store run in
parallel, they must synchronise to avoid data hazards:

| Queue | Direction | Meaning |
|---|---|---|
| l2c | Load → Compute | "I finished loading your input data, you can start" |
| c2l | Compute → Load | "I'm done with that buffer, you can load the next batch" |
| c2s | Compute → Store | "I finished computing, you can write the result" |
| s2c | Store → Compute | "I finished writing, you can reuse the output buffer" |

An instruction's `pop_prev`/`pop_next`/`push_prev`/`push_next` flags control whether
it must wait for (pop) or send (push) a dependency token before/after execution.

---

## Project Structure

```
VTA-Simulator-main/
├── main.cpp                          Entry point
├── CMakeLists.txt                    Build recipe (lists all source files + libraries)
├── CMakePresets.json                 Pre-configured build settings for vcpkg
├── build.cmd                         One-command terminal build helper
├── config/
│   └── config.json                   Runtime configuration (model, hardware settings)
├── include/
│   ├── simulation/
│   │   ├── vta.h                     Top-level module (wires everything together)
│   │   ├── arm.h                     ARM host module
│   │   ├── fetcher.h                 Instruction fetch and dispatch module
│   │   ├── module.h                  Abstract base class for Load/Compute/Store
│   │   ├── load_module.h             Load stage module
│   │   ├── compute_module.h          Compute stage module (GEMM/ALU)
│   │   ├── store_module.h            Store stage module
│   │   └── queue.h                   FIFO queue module
│   ├── instruction/
│   │   ├── instruction.h             Instruction data structure
│   │   └── parser.h                  CSV parser (model → instructions)
│   ├── config/
│   │   ├── config.h                  PlatformConfig singleton (reads config.json)
│   │   ├── files_manager.h           Manages input/output file paths
│   │   └── vta_config.h              Additional config helpers
│   ├── type/
│   │   └── type.h                    Enums, constants, CSV column positions
│   ├── communication/
│   │   ├── communication.h           ZMQ socket wrappers (used in extended mode)
│   │   └── message.h                 Message format for ZMQ communication
│   └── utilities/
│       ├── utilities.h               General helpers (string concat etc.)
│       ├── json.hpp                  nlohmann JSON (bundled header)
│       ├── json-schema.hpp           JSON schema validator (bundled header)
│       └── magic_enum.hpp            magic_enum (bundled header)
├── src/                              Implementations matching include/
├── archive/                          Older prototype code (kept for reference)
└── docs/
    └── VTA-Simulator-Testing-Guide.pdf   Step-by-step testing guide
```

---

## Configuration (`config/config.json`)

```json
{
    "model_name":           "resnet_101",  // which CSV to load from data/models/
    "granulatity":          "layer",       // "layer" or "block" grouping
    "en_ddr_lock":          true,          // model DRAM access contention
    "en_cache":             false,         // enable on-chip cache model
    "en_prefetch":          false,         // enable prefetch model
    "max_cache_mem_size":   3200,          // cache size budget (bytes)
    "nr_load_pipeline":     1,             // parallel load pipelines
    "nr_compute_pipeline":  1,             // parallel compute pipelines
    "nr_store_pipeline":    1,             // parallel store pipelines
    "load_cycles":          80,            // cycles per load operation
    "load_weigts_cycles":   60,            // cycles per weight-load operation
    "store_cycles":         80             // cycles per store operation
}
```

Change `model_name` and run again (no rebuild needed — config is read at runtime).

Available models (place CSVs in `data/models/`):

| Model | Layers | Notes |
|---|---|---|
| `resnet_18` | 20 | Fastest, good for quick testing |
| `resnet_34` | 36 | Medium |
| `resnet_50` | 53 | Medium-large |
| `resnet_101`| 103 | Default, full run ~58 ms simulated |

---

## Requirements

- **Windows 10 / 11**
- **Visual Studio Build Tools 2022 or later** with the *Desktop development with C++*
  workload (includes MSVC compiler, Windows SDK, CMake, Ninja)
- **vcpkg** (included as a subfolder; already bootstrapped)

### Install all required libraries (run once)

From the project root in a terminal:

```powershell
.\vcpkg\vcpkg.exe install systemc cppzmq zeromq nlohmann-json json-schema-validator magic-enum --triplet x64-windows
```

This builds and installs all six libraries locally inside the `vcpkg/` folder.
Nothing is installed system-wide.

---

## Building

### Option A — VS Code (recommended)

1. Open the project folder in VS Code.
2. Install the recommended extensions when prompted:
   **C/C++** (ms-vscode.cpptools) and **CMake Tools** (ms-vscode.cmake-tools).
3. Press `Ctrl+Shift+P` → **CMake: Configure** (select the **vcpkg** preset if asked).
4. Press `Ctrl+Shift+P` → **CMake: Build** (or click the Build button in the status bar).
5. A successful build ends with `Build finished with exit code 0`.

> If you get `fatal error C1083: Cannot open include file: 'cassert'` — reload VS Code
> (`Ctrl+Shift+P` → Developer: Reload Window) and build again. The setting
> `cmake.useVsDeveloperEnvironment: always` in `.vscode/settings.json` ensures the
> compiler environment is loaded.

### Option B — Terminal

```cmd
.\build.cmd
```

This loads the MSVC environment, configures with CMake, and builds in one step.
Useful as a fallback if the VS Code button environment is unavailable.

---

## Running

### VS Code
Press **F5** → choose **"Run VTA Simulator (Ninja build)"**.
Output appears in the terminal panel.

### Terminal
```cmd
.\build\my_systemc.exe
```

The two required runtime DLLs (`libzmq-mt-4_3_5.dll` and
`nlohmann_json_schema_validator.dll`) are already copied next to the executable in
`build\` so it runs without any PATH changes.

### Expected output (ResNet-101)
```
211040 ns     ---------------------------- FINISH LAYER 0 ----------------------------
758676 ns     ---------------------------- FINISH LAYER 1 ----------------------------
1572678 ns    ---------------------------- FINISH LAYER 2 ----------------------------
...
58021575 ns   ---------------------------- FINISH LAYER 102 ---------------------------
```

Each line = simulated hardware time (nanoseconds) when that layer finished.
The simulation exits with code 0.

---

## Testing With Different Inputs

1. Open `config/config.json`.
2. Change `"model_name"` to `resnet_18`, `resnet_34`, `resnet_50`, or `resnet_101`.
3. Run `.\build\my_systemc.exe` again (no rebuild needed).

To test the effect of configuration changes:

```powershell
# Save a baseline
.\build\my_systemc.exe > baseline.txt

# Change something in config.json, then run again
.\build\my_systemc.exe > current.txt

# Compare
Compare-Object (Get-Content baseline.txt) (Get-Content current.txt)
```

See `docs/VTA-Simulator-Testing-Guide.pdf` for the full testing workflow including
regression testing and what to do when porting new code into the modules.

---

## What is NOT in This Repository (and Why)

| Excluded | Reason |
|---|---|
| `vcpkg/installed/`, `vcpkg/packages/` etc. | Built libraries — regenerated by `vcpkg install` |
| `build/` | Compiled output — regenerated by `build.cmd` or VS Code Build |
| `data/models/*.csv` | Large model files — obtain separately and place in `data/models/` |
| `*.log`, `baseline.txt` | Generated at runtime |

---

## Relation to the Original VTA HLS Code

The files `vta.h` and `vta.cc` in the parent project folder are the original VTA
hardware design written in **Vivado HLS C++** (Xilinx-specific: `ap_int`, `hls::stream`,
`#pragma HLS`). That code targets FPGA synthesis, not simulation.

This project is a **SystemC re-expression** of the same hardware for simulation purposes.
The four HLS functions map to four `sc_module` subclasses. The ongoing work is to
faithfully translate the logic of `vta.cc` into the corresponding SystemC modules,
preserving the dependency handshake protocol and timing model.

| HLS concept | SystemC equivalent here |
|---|---|
| `hls::stream<T>` | `Queue` module with `vld/rdy/data/end` signals |
| `ap_int<N>` / `ap_uint<N>` | `sc_int<N>` / `sc_uint<N>` |
| Dependency queues (`l2g`, `g2l`, `s2g`, `g2s`) | `l2c`, `c2l`, `s2c`, `c2s` Queue modules |
| `#pragma HLS PIPELINE` | Not needed (simulation, not synthesis) |
| Function called once, returns | `sc_module` process driven by `sc_event`s |

---

## Authors

- Original VTA simulator: Yosab Bebawy, University of Siegen
- Build setup, VS Code configuration, documentation and testing framework: Jibin Babu Amakkattu
