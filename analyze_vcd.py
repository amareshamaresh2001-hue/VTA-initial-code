"""
VCD Waveform Analyzer for VTA Simulator
Checks every signal for correctness, handshake violations, stalls, and protocol errors.
"""

import re
from collections import defaultdict

VCD_FILE = "vta_waveforms.vcd"

# ─────────────────────────────────────────────────────────────────────────────
# 1. Minimal hand-rolled VCD parser
# ─────────────────────────────────────────────────────────────────────────────

def parse_vcd(path):
    signal_meta = {}
    changes     = defaultdict(list)

    current_time = 0

    with open(path, "r", errors="replace") as f:
        text = f.read()

    # collect $var declarations
    for m in re.finditer(
        r'\$var\s+\w+\s+(\d+)\s+(\S+)\s+([\w\[\]./: ]+?)\s+(?:\[[\d:]+\])?\s*\$end',
        text
    ):
        width = int(m.group(1))
        vid   = m.group(2)
        name  = m.group(3).strip()
        signal_meta[vid] = {'name': name, 'width': width}

    # parse value changes
    tokens = text.split()
    n = len(tokens)
    i = 0
    while i < n:
        tok = tokens[i]
        if tok.startswith('#'):
            current_time = int(tok[1:])
            i += 1
        elif tok[0] in ('b', 'B'):
            val = tok[1:]
            i += 1
            if i < n:
                vid = tokens[i]
                changes[vid].append((current_time, val))
            i += 1
        elif tok[0] in ('0', '1', 'x', 'z', 'X', 'Z') and len(tok) > 1:
            val = tok[0]
            vid = tok[1:]
            changes[vid].append((current_time, val))
            i += 1
        else:
            i += 1

    return signal_meta, changes

# ─────────────────────────────────────────────────────────────────────────────
# 2. Helper utilities
# ─────────────────────────────────────────────────────────────────────────────

def transitions(sig):
    out = []
    prev = None
    for t, v in sig:
        if v != prev:
            out.append((t, v))
            prev = v
    return out

def rising_edges(sig):
    return [(t, v) for t, v in transitions(sig) if v == '1']

def count_pulses(sig):
    return len(rising_edges(sig))

def stall_analysis(sig_sorted, threshold_ns=500):
    stalls = []
    prev_time = None
    prev_val  = None
    for t, v in sig_sorted:
        if prev_val == '0' and v == '1':
            duration = t - prev_time
            if duration >= threshold_ns:
                stalls.append((duration, prev_time, t))
        prev_time = t
        prev_val  = v
    stalls.sort(reverse=True)
    return stalls[:5]

def check_axi_write_channel(awvalid, awready, wvalid, wready, wlast, bvalid, bready):
    issues = []
    n_aw     = len([t for t, v in awvalid if v == '1'])
    n_aw_acc = len([t for t, v in awready if v == '1'])
    n_wlast  = len([t for t, v in wlast   if v == '1'])
    n_bresp  = len([t for t, v in bvalid  if v == '1'])

    issues.append(f"  AWVALID assertions : {n_aw}")
    issues.append(f"  AWREADY assertions : {n_aw_acc}")
    issues.append(f"  WLAST assertions   : {n_wlast}")
    issues.append(f"  BVALID responses   : {n_bresp}")

    if n_aw != n_aw_acc:
        issues.append(f"  MISMATCH: {n_aw} AWVALID vs {n_aw_acc} AWREADY!")
    else:
        issues.append(f"  OK AWVALID/AWREADY counts match ({n_aw})")

    if n_aw != n_wlast:
        issues.append(f"  MISMATCH: {n_aw} write bursts vs {n_wlast} WLAST!")
    else:
        issues.append(f"  OK Write bursts == WLAST count ({n_aw})")

    if n_aw != n_bresp:
        issues.append(f"  MISMATCH: {n_aw} write bursts vs {n_bresp} BVALID!")
    else:
        issues.append(f"  OK Write bursts == BVALID responses ({n_aw})")

    return issues, n_aw, n_wlast, n_bresp

def check_axi_read_channel(arvalid, arready, rvalid, rready):
    issues = []
    n_ar     = len([t for t, v in arvalid if v == '1'])
    n_ar_acc = len([t for t, v in arready if v == '1'])
    n_rresp  = len([t for t, v in rvalid  if v == '1'])

    issues.append(f"  ARVALID assertions : {n_ar}")
    issues.append(f"  ARREADY assertions : {n_ar_acc}")
    issues.append(f"  RVALID beats       : {n_rresp}")

    if n_ar != n_ar_acc:
        issues.append(f"  MISMATCH: {n_ar} ARVALID vs {n_ar_acc} ARREADY!")
    else:
        issues.append(f"  OK ARVALID/ARREADY counts match ({n_ar})")

    return issues, n_ar, n_ar_acc

def level_at(sig_sorted, t):
    """
    Return the VALUE of a signal at time t, by scanning backwards through
    the sorted (time, value) list.  This is the correct way to check whether
    a signal is HIGH at a given moment — it may have been asserted much earlier
    and simply stayed high, never re-appearing in the VCD at time t.
    """
    result = '0'  # default before any event
    for ts, v in sig_sorted:
        if ts <= t:
            result = v
        else:
            break
    return result

def longest_gap(sig_sorted):
    pulse_times = [t for t, v in sig_sorted if v == '1']
    if len(pulse_times) < 2:
        return 0, 0
    gaps = [(pulse_times[i+1] - pulse_times[i], pulse_times[i]) for i in range(len(pulse_times)-1)]
    return max(gaps, key=lambda x: x[0])

# ─────────────────────────────────────────────────────────────────────────────
# 3. Main analysis
# ─────────────────────────────────────────────────────────────────────────────

def main():
    print("=" * 70)
    print("  VTA WAVEFORM ANALYSIS REPORT")
    print(f"  File: {VCD_FILE}")
    print("=" * 70)

    print("\n[1/8] Parsing VCD file...", end='', flush=True)
    meta, changes = parse_vcd(VCD_FILE)
    print(f" done. Found {len(meta)} signals.")

    print("\n--- ALL SIGNALS IN VCD ---")
    for vid, info in sorted(meta.items(), key=lambda x: x[1]['name']):
        n_changes = len(changes.get(vid, []))
        print(f"  [{vid:3s}] w={info['width']:2d}  changes={n_changes:6d}  {info['name']}")

    def sig(name):
        for vid, info in meta.items():
            if info['name'] == name:
                return sorted(changes.get(vid, []))
        return []

    clk     = sig("1_SYS/clk")
    rst     = sig("1_SYS/reset_n")
    finish  = sig("1_SYS/FINISH_layer_done")

    f_arvalid = sig("2_FETCHER/AXI_ARVALID")
    f_arready = sig("2_FETCHER/AXI_ARREADY")
    f_araddr  = sig("2_FETCHER/AXI_ARADDR")
    f_rvalid  = sig("2_FETCHER/AXI_RVALID")
    f_rready  = sig("2_FETCHER/AXI_RREADY")
    f_exp0    = sig("2_FETCHER/VALIDATION_expected_part0")
    f_got0    = sig("2_FETCHER/VALIDATION_fetched_part0")

    disp_l_vld = sig("3_DISPATCH/to_LOAD_vld")
    disp_c_vld = sig("3_DISPATCH/to_COMPUTE_vld")
    disp_s_vld = sig("3_DISPATCH/to_STORE_vld")

    l_arvalid = sig("4_LOAD/AXI_ARVALID")
    l_arready = sig("4_LOAD/AXI_ARREADY")
    l_rvalid  = sig("4_LOAD/AXI_RVALID")
    l_rready  = sig("4_LOAD/AXI_RREADY")
    l_rdata   = sig("4_LOAD/AXI_RDATA")

    c_arvalid = sig("5_COMPUTE/AXI_ARVALID")
    c_arready = sig("5_COMPUTE/AXI_ARREADY")
    c_rvalid  = sig("5_COMPUTE/AXI_RVALID")
    c_rready  = sig("5_COMPUTE/AXI_RREADY")

    s_awvalid = sig("6_STORE/AXI_AWVALID")
    s_awready = sig("6_STORE/AXI_AWREADY")
    s_awaddr  = sig("6_STORE/AXI_AWADDR")
    s_wdata   = sig("6_STORE/AXI_WDATA")
    s_wvalid  = sig("6_STORE/AXI_WVALID")
    s_wready  = sig("6_STORE/AXI_WREADY")
    s_wlast   = sig("6_STORE/AXI_WLAST")
    s_bvalid  = sig("6_STORE/AXI_BVALID")
    s_bready  = sig("6_STORE/AXI_BREADY")

    dep_l2c = sig("7_DEPENDENCIES/l2c_Load_Ready")
    dep_c2l = sig("7_DEPENDENCIES/c2l_Compute_Done")
    dep_c2s = sig("7_DEPENDENCIES/c2s_Compute_Ready")
    dep_s2c = sig("7_DEPENDENCIES/s2c_Store_Done")

    # ── Clock & Reset ─────────────────────────────────────────────────────
    print("\n--- [2/8] CLOCK & RESET ---")
    clk_edges = count_pulses(clk)
    print(f"  Clock rising edges : {clk_edges}")
    rst_de = [t for t, v in transitions(rst) if v == '1']
    if rst_de:
        print(f"  OK Reset de-asserted at : {rst_de[0]} ns")
    else:
        print("  FAIL Reset was NEVER de-asserted!")

    fin_edges = rising_edges(finish)
    if fin_edges:
        sim_end_ns = fin_edges[0][0]
        print(f"  OK FINISH went HIGH at : {sim_end_ns} ns  (layer done successfully)")
    else:
        sim_end_ns = 0
        print("  FAIL FINISH signal never went HIGH!")

    # ── Fetcher ───────────────────────────────────────────────────────────
    print("\n--- [3/8] FETCHER (AXI Read channel) ---")
    issues, n_ar, n_ar_acc = check_axi_read_channel(f_arvalid, f_arready, f_rvalid, f_rready)
    for l in issues:
        print(l)

    if f_exp0 and f_got0:
        exp0_dict = {t: v for t, v in f_exp0}
        got0_dict = {t: v for t, v in f_got0}
        common_times = set(exp0_dict) & set(got0_dict)
        mismatches = sum(1 for t in common_times if exp0_dict[t] != got0_dict[t])
        if mismatches == 0:
            print(f"  OK Fetched instruction bits match expected (checked {len(common_times)} points)")
        else:
            print(f"  FAIL {mismatches} instruction word mismatches in part0!")

    f_stalls = stall_analysis(f_arvalid, threshold_ns=200)
    if f_stalls:
        print(f"  Longest ARVALID stall (waiting for read): {f_stalls[0][0]} ns  [{f_stalls[0][1]}-->{f_stalls[0][2]} ns]")
    else:
        print("  OK No significant ARVALID stalls")

    # ── Dispatch ──────────────────────────────────────────────────────────
    print("\n--- [4/8] DISPATCH (Fetcher -> Module Queues) ---")
    l_dispatched = count_pulses(disp_l_vld)
    c_dispatched = count_pulses(disp_c_vld)
    s_dispatched = count_pulses(disp_s_vld)
    total = l_dispatched + c_dispatched + s_dispatched
    print(f"  Instructions -> LOAD    : {l_dispatched}")
    print(f"  Instructions -> COMPUTE : {c_dispatched}")
    print(f"  Instructions -> STORE   : {s_dispatched}")
    print(f"  Total dispatched        : {total}")
    if l_dispatched > 0 and c_dispatched > 0 and s_dispatched > 0:
        print("  OK All three pipelines received instructions")
    else:
        print("  FAIL One or more pipelines received NO instructions!")

    # ── Load ──────────────────────────────────────────────────────────────
    print("\n--- [5/8] LOAD MODULE (AXI Read from DRAM) ---")
    issues, n_ar, n_ar_acc = check_axi_read_channel(l_arvalid, l_arready, l_rvalid, l_rready)
    for l in issues:
        print(l)

    # RDATA non-zero check
    nonzero_rdata = [(t, v) for t, v in l_rdata if v not in ('x', 'z', '0', '00000000')]
    print(f"  Non-zero RDATA beats   : {len(nonzero_rdata)}")
    if nonzero_rdata:
        try:
            val_hex = hex(int(nonzero_rdata[0][1], 2))
        except:
            val_hex = nonzero_rdata[0][1]
        print(f"  OK Load is reading non-zero data (first at {nonzero_rdata[0][0]} ns, val={val_hex})")
    else:
        print("  WARN All RDATA beats were zero!")

    l_stalls = stall_analysis(l_arvalid, threshold_ns=500)
    if l_stalls:
        for dur, start, end in l_stalls[:3]:
            print(f"  Load stall: {dur:,} ns  [{start:,}-->{end:,} ns]")
    else:
        print("  OK No notable ARVALID stalls in LOAD")

    # ── Compute ───────────────────────────────────────────────────────────
    print("\n--- [6/8] COMPUTE MODULE (AXI Read for weights) ---")
    if c_arvalid:
        issues, n_ar, n_ar_acc = check_axi_read_channel(c_arvalid, c_arready, c_rvalid, c_rready)
        for l in issues:
            print(l)
    else:
        print("  INFO No ARVALID from Compute (weights loaded by Load module)")

    # ── Store ─────────────────────────────────────────────────────────────
    print("\n--- [7/8] STORE MODULE (AXI Write to DRAM) ---")
    issues, n_aw, n_wlast, n_bresp = check_axi_write_channel(
        s_awvalid, s_awready, s_wvalid, s_wready,
        s_wlast, s_bvalid, s_bready
    )
    for l in issues:
        print(l)

    # WDATA non-zero check
    nonzero_wdata = [(t, v) for t, v in s_wdata if v not in ('0', 'x', 'z', '00000000')]
    print(f"  Non-zero WDATA beats   : {len(nonzero_wdata)}")
    if nonzero_wdata:
        try:
            val_hex = hex(int(nonzero_wdata[0][1], 2))
        except:
            val_hex = nonzero_wdata[0][1]
        print(f"  OK Non-zero writes found (first at {nonzero_wdata[0][0]} ns, val={val_hex})")
    else:
        print("  WARN All WDATA beats were zero - results may be zeroed!")

    # WLAST timing: at every cycle WLAST=1, WREADY must also be 1
    # Correct approach: evaluate WREADY's LEVEL at the time WLAST goes high,
    # NOT whether WREADY transitioned to 1 at the same timestamp.
    wlast_bad = []
    for t, v in s_wlast:
        if v == '1':
            wready_level = level_at(s_wready, t)
            if wready_level != '1':
                wlast_bad.append(t)
    if wlast_bad:
        print(f"  FAIL {len(wlast_bad)} WLAST assertions where WREADY was LOW (slave not ready to accept data!)")
        print(f"       First violation at {wlast_bad[0]} ns")
    else:
        print(f"  OK Every WLAST asserted while WREADY was already HIGH (correct burst completion)")

    # BREADY pre-asserted before BVALID
    # Correct approach: at the time BVALID goes high, evaluate BREADY's level.
    bvalid_events = [(t, v) for t, v in s_bvalid if v == '1']
    bready_late = 0
    for bt, _ in bvalid_events:
        if level_at(s_bready, bt) != '1':
            bready_late += 1
    if bready_late:
        print(f"  WARN {bready_late} BVALID cycles where BREADY was not yet asserted")
    else:
        print(f"  OK BREADY pre-asserted before every BVALID")

    # Unique store addresses
    aw_addrs = sorted(set(v for t, v in s_awaddr if v not in ('x','z')))
    print(f"  Unique store addresses : {len(aw_addrs)}")
    if aw_addrs:
        try:
            first_addr = hex(int(aw_addrs[0], 2))
            last_addr  = hex(int(aw_addrs[-1], 2))
        except:
            first_addr = aw_addrs[0]
            last_addr  = aw_addrs[-1]
        print(f"  Address range          : {first_addr}  ..  {last_addr}")

    # ── Dependency queues ──────────────────────────────────────────────────
    print("\n--- [8/8] PIPELINE DEPENDENCY QUEUES ---")
    l2c_pulses = count_pulses(dep_l2c)
    c2l_pulses = count_pulses(dep_c2l)
    c2s_pulses = count_pulses(dep_c2s)
    s2c_pulses = count_pulses(dep_s2c)
    print(f"  l2c (Load->Compute ready)  pulses: {l2c_pulses}")
    print(f"  c2l (Compute->Load done)   pulses: {c2l_pulses}")
    print(f"  c2s (Compute->Store ready) pulses: {c2s_pulses}")
    print(f"  s2c (Store->Compute done)  pulses: {s2c_pulses}")

    if l2c_pulses == c2l_pulses:
        print(f"  OK l2c/c2l balanced ({l2c_pulses}) -- Load/Compute sync correct")
    else:
        print(f"  WARN l2c({l2c_pulses}) != c2l({c2l_pulses}) -- Load/Compute asymmetry")

    if c2s_pulses == s2c_pulses:
        print(f"  OK c2s/s2c balanced ({c2s_pulses}) -- Compute/Store sync correct")
    else:
        print(f"  WARN c2s({c2s_pulses}) != s2c({s2c_pulses}) -- Compute/Store asymmetry")

    for name, dep in [("l2c", dep_l2c), ("c2l", dep_c2l), ("c2s", dep_c2s), ("s2c", dep_s2c)]:
        if dep:
            gap, at = longest_gap(dep)
            status = "WARN" if gap > 500_000 else "OK  "
            print(f"  {status} {name} max inter-pulse gap: {gap:>12,} ns  (after {at:,} ns)")

    print("\n" + "=" * 70)
    print("  ANALYSIS COMPLETE")
    print("=" * 70)

if __name__ == "__main__":
    main()
