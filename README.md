# GTU-C312 Sim-OS

*A tiny, educational operating-system skeleton and CPU interpreter for the fictional
GTU-C312 architecture.*

<p align="center">
  <img src="docs/gtu-c312_banner.svg" width="620" alt="GTU-C312 banner"/>
</p>

---

##  Why?

- **Architectures are fun** – but most teaching CPUs are either too tiny
  (no system-calls, no threads) or too huge (x86, RISC-V).
- **Co-operative scheduling** lets you see every context-switch and
  understand *exactly* what the OS does, one instruction at a time.
- **C assembly**: the interpreter is modern C; the “OS” and
  user programs are written in raw GTU-C312 assembly – perfect for
  systems-programming courses.

---

##  Features

| component | details |
|-----------|---------|
| **Interpreter** | • 65 536-cell memory • kernel / user protection<br>• full instruction set (SET, CPY, CPYI, …) plus `SYSCALL` |
| **System calls** | `PRN`, `YIELD`, `HLT` |
| **Thread table** | 11 PCBs, 8 words each (ID, start-time, used-ticks, PC, SP, state, blk, reserved) |
| **Scheduler** | simple round-robin, non-preemptive, co-operative; stamps start-time and counts ticks |
| **Debug levels** | `-D0` memory-dump on halt • `-D1` dump after every tick • `-D2` single-step • `-D3` dump thread table on every switch |
| **Sample threads** | bubble-sort, linear search, demo loop – all yield correctly |

---

##  Quick start

```bash
git clone https://github.com/<you>/gtu-c312-sim-os.git
cd gtu-c312-sim-os
make            # builds simulate (interpreter) + trace lib
./simulation os.gtu | ./simulation os.gtu -D 0 | 1 | 2 | 3
