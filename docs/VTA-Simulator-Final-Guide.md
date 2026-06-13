# VTA Simulator: Comprehensive Architecture & Memory Integration Guide

## 1. Project Evolution: `main` vs. `development`
When this project started on the `main` branch, the `VTA-Simulator` was purely a **Timing Model**. The modules (`Load`, `Compute`, `Store`) contained empty `dependencies_received()` functions that simply called `finish.notify(latency())`. They simulated time passing, but no actual neural network math or data movement occurred.

On the `development` branch, we upgraded the simulator into a **Functionally Accurate Hardware Model**. We accomplished two massive milestones:
1.  **Functional Math:** The `ComputeModule` now natively executes the TVM VTA GEMM (Matrix Multiplication) and ALU (Arithmetic Logic Unit) algorithms.
2.  **AXI Hardware Integration:** The `LoadModule` and `StoreModule` now physically communicate with the 64KB Main Memory using strict, cycle-accurate AXI hardware handshakes.

---

## 2. Global Dataflow (End-to-End)
Here is the journey of a single neural network instruction from start to finish:

1.  **Parser:** The simulation boots, and `parser.cpp` reads the CSV file (e.g., `resnet_101.csv`), converting text into `Instruction` objects.
2.  **Fetcher:** Pushes the instructions into the three execution queues (`Load`, `Compute`, `Store`).
3.  **Dependency Queues (FIFOs):** If an instruction depends on a previous one (e.g., Compute needs Load to finish), it waits for a software token via the `l2c` (Load-to-Compute) FIFO queues.
4.  **Load Phase:** `LoadModule` triggers its AXI state machine. It negotiates the AXI bus and pulls 32-bit words from Main Memory into the 8-bit `inp_mem` and `wgt_mem` SRAMs.
5.  **Compute Phase:** `ComputeModule` triggers its math loops. It multiplies `inp_mem` with `wgt_mem`, accumulates the sum in `acc_mem`, and writes the 8-bit result to `out_mem`.
6.  **Store Phase:** `StoreModule` triggers its AXI state machine. It packs the 8-bit results from `out_mem` into 32-bit words and negotiates the AXI bus to write them back to Main Memory.

---

## 3. The Hardware Bridge (Event-Driven to Clock-Driven)
**The Problem:** The VTA Simulator's base `Module` class is **event-driven**. Functions like `dependencies_received()` trigger instantly and cannot use the `wait()` command. However, AXI memory relies on a **10ns clock** and requires `wait()` to synchronize signals.
**The Solution:** We created a "Bridge". 
*   We added `sc_event start_axi_read` and `start_axi_write`.
*   We created independent `SC_THREAD`s (`axi_read_thread` and `axi_write_thread`) that sleep in the background.
*   When `dependencies_received()` triggers, it instantly fires the event, wakes up the AXI thread, and lets the thread take its time waiting on the clock cycles to do the heavy hardware lifting.

---

## 4. Module 1: `LoadModule` (The Data Fetcher)

### Core Variables and Meaning:
*   `sram_base`: The starting index in the local SRAM array where data should be saved.
*   `current_address`: The physical address in DRAM we are reading from. Driven dynamically by `START_ADDR`.
*   `y_size`, `x_size`: The 2D dimensions of the data block being fetched.
*   `stride`: How many bytes to jump in DRAM to get to the next row of the 2D block.
*   `y0_pad, y1_pad, x0_pad, x1_pad`: Padding variables. Neural networks often require a border of zeros around an image/tensor before convolution.

### Step-by-Step Logic (`axi_read_thread`):
1.  **Wait for Trigger:** Thread waits for `start_axi_read`.
2.  **Top Padding (`y0_pad`):** Writes rows of pure `0`s to the SRAM before fetching real data.
3.  **Row Loop (`y_size`):**
    *   **Left Padding (`x0_pad`):** Writes a few `0`s to start the row.
    *   **AXI Address Phase:** 
        *   Sets `ARADDR = current_address`.
        *   Sets `ARLEN = 3` (Asking for 4 beats of data).
        *   Sets `ARVALID = 1`.
        *   *Handshake:* `do { wait(); } while (ARREADY == 0);` (Wait for memory to accept address).
    *   **AXI Data Phase (The 4-Beat Chunking):**
        *   Because the memory is hardcoded for 4-beat bursts, we loop until `x_size` is fulfilled, requesting 4 beats at a time.
        *   *Delayed Handshake:* `RREADY` starts at `0`. We wait for the memory to assert `RVALID == 1`. *Only then* do we set `RREADY = 1` and `wait()` for one clock cycle. This prevents delta-cycle race conditions.
    *   **Unpacking:** We read the 32-bit `RDATA`. We use bitshifts (`>> 0`, `>> 8`, `>> 16`, `>> 24`) and masks (`& 0xFF`) to split the 32-bit word into four 8-bit pieces. These are saved sequentially into `inp_mem` or `wgt_mem`.
    *   **Right Padding (`x1_pad`):** Writes a few `0`s to end the row.
    *   **Stride:** `current_address += stride` moves the DRAM pointer to the next row.
4.  **Bottom Padding (`y1_pad`):** Writes rows of pure `0`s at the end.
5.  **Finish:** `finish.notify(latency())` tells the high-level simulator to pass the token to Compute.

---

## 5. Module 2: `ComputeModule` (The Math Engine)

### Core Variables and Meaning:
*   `inp_mem`, `wgt_mem`, `out_mem`: The 8-bit globally shared SRAM arrays.
*   `acc_mem`: The 32-bit private accumulator SRAM.
*   `uop_mem`: The Micro-Op program SRAM. Holds instructions on *how* to stitch the tiles together.
*   `DST_MASK`, `SRC_MASK`, `WGT_MASK`: Bitmasks used to extract 11-bit and 10-bit indices from the 32-bit `uop` word.

### Step-by-Step Logic (`dependencies_received`):
1.  **Loop Setup:** Sets up the Outer Loop (`iter_out`) and Inner Loop (`iter_in`).
2.  **Micro-Op Loop:** Iterates through `uop_mem` from `range_0` to `range_1`.
3.  **Decoding:** Grabs the 32-bit `uop` word. 
    *   `dst_idx = (uop >> 0) & DST_MASK` (Finds where to save the result).
    *   `src_idx` and `wgt_idx` are extracted similarly.
4.  **GEMM Execution:**
    *   Loops over `VTA_BLOCK_OUT` (16 output channels).
    *   Reads the previous sum from `acc_mem[dst_idx]`.
    *   Loops over `VTA_BLOCK_IN` (16 input channels).
    *   **The Dot Product:** Multiplies `inp_mem * wgt_mem` and adds it to `tmp`.
    *   Adds `tmp` to the accumulator.
    *   *Reset logic:* If the instruction `reset_out` is true, the accumulator clears to 0 for the next run.
    *   *Quantization:* Truncates the 32-bit sum down to an 8-bit integer (`& 0xFF`) and saves it to `out_mem`.
5.  **ALU Execution:**
    *   If the instruction is `ALU - max/min/add/shr`, it ignores weights.
    *   It extracts `src_0` from the accumulator.
    *   It extracts `src_1` (either from another accumulator tile, or a default immediate value `imm`).
    *   Performs element-wise math (e.g., `result = src_0 + src_1`).
    *   Truncates and saves to `out_mem`.

---

## 6. Module 3: `StoreModule` (The Data Pusher)

### Core Variables and Meaning:
*   `AWADDR`, `AWVALID`, `AWREADY`: AXI Address Write handshake pins.
*   `WDATA`, `WVALID`, `WLAST`, `WREADY`: AXI Data Write handshake pins.
*   `BVALID`, `BREADY`: AXI Write Response (confirmation) pins.

### Step-by-Step Logic (`axi_write_thread`):
1.  **Wait for Trigger:** Waits for `start_axi_write` from the Compute module finishing.
2.  **Row Loop (`y_size`):**
    *   **AXI Address Phase:** Sets `AWADDR` and `AWVALID = 1`. Waits for `AWREADY == 1`.
    *   **AXI Data Phase (The 4-Beat Chunking & Packing):**
        *   Loops 4 times for the 4-beat burst.
        *   **Packing:** Reads four separate 8-bit integers from `out_mem`. Shifts them left (`<< 0`, `<< 8`, `<< 16`, `<< 24`) and uses binary OR (`|`) to pack them into a single 32-bit `data_chunk`.
        *   Sets `WDATA = data_chunk`.
        *   Sets `WLAST = 1` on the 4th beat.
        *   *Delayed Handshake:* Waits for `WREADY == 1` *before* asserting `WVALID = 1`. (Prevents the slave race-condition bug).
    *   **AXI Response Phase:** Sets `BREADY = 1`. Waits for the memory to send `BVALID == 1` confirming the write succeeded to DRAM.
    *   **Stride:** `current_address += stride`.
3.  **Finish:** `finish.notify(latency())` ends the instruction pipeline.

---

## 7. The Memory Integration Strategy (Why we bypassed the Arbiter)
During the Stage 2 AXI integration, we encountered a severe hardware deadlock with the teammate's `axi_interconnect` (Arbiter) module.

**The Bug:** The teammate's Memory Slave state machine had a delta-cycle race condition. It asserted `RLAST = 1` (transfer finished), but instantly overwrote it to `RLAST = 0` in the exact same clock cycle. Because the Arbiter never saw `RLAST`, it permanently locked the bus, assuming `LoadModule` was still using it, causing the entire simulator to freeze.

**The Solution:** Our supervisor mandated that we must implement manual AXI interfaces and protocols, but stated that the teammate's code should not be altered.
To satisfy both constraints, we wired the "Motherboard" (`vta.cpp`) intelligently:
*   Instead of wiring Load and Store to the Arbiter, we wired them **directly to the Memory Slave (`axi_lite_slave`)**.
*   Because `LoadModule` only reads (using the `AR` and `R` channels) and `StoreModule` only writes (using the `AW`, `W`, and `B` channels), they use entirely separate physical pins on the Memory chip. They cannot collide.
*   This allowed us to maintain the strict, 100% accurate AXI `VALID`/`READY` handshakes inside our modules—fulfilling the educational requirements of the assignment—without being frozen by the bugged third-party Arbiter.