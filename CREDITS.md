# Credits & Acknowledgments

## Mythological Inspiration

- **Ginnungagap (Norse Mythology)**: The vast, primordial empty void described in the *Prose Edda* and *Völuspá*, spanning between the burning fires of Muspelheim and the frozen mists of Niflheim. In Project Ginnunga, this serves as the foundational metaphor for a sparse cache: an immense, mostly vacant address space where dense, hot data clusters are dynamically ignited and managed.

---

## Algorithmic & Architectural Foundations

Project Ginnunga stands on the shoulders of foundational computer science research and modern systems engineering:

1. **Adaptive Replacement Cache (ARC)**:
   - **Authors**: Nimrod Megiddo and Dharmendra S. Modha (*IBM Research, FAST 2003*).
   - **Inspiration**: The self-tuning cache replacement policy that dynamically balances recency ($T_1$) and frequency ($T_2$) using ghost caches ($B_1$, $B_2$).

2. **Epoch-Based Reclamation (EBR)**:
   - **Author**: Keir Fraser (*University of Cambridge, 2004*).
   - **Inspiration**: Lock-free epoch tracking ensuring wait-free reads and safe deferred destruction for low-latency concurrent readers.

3. **Hazard Pointers**:
   - **Author**: Maged M. Michael (*IBM Thomas J. Watson Research Center, IEEE TPDS 2004*).
   - **Inspiration**: Safe memory reclamation and ABA prevention via reader hazard pointer publishing.

4. **SIMD-Accelerated Swiss Tables**:
   - **Authors**: Matt Kulukundis and the Google Abseil Team (*CppCon 2017/2018*).
   - **Inspiration**: 16-lane parallel metadata probing using ARM NEON and x86 SSE/AVX2 control bytes fitting inside single 64-byte CPU cache lines.

5. **Slot-Array Maps & Sparse Sets**:
   - **Inspiration**: High-performance game engine Entity Component Systems (ECS), notably popularized by Michele Caini (*EnTT*) and Sanders Mertens (*Flecs*), achieving $O(1)$ lookups alongside cache-line packed dense array iteration.

6. **Judy Arrays & Adaptive Radix Tries**:
   - **Author**: Douglas Baskins (*Hewlett-Packard*).
   - **Inspiration**: Highly compressed radix trie structures managing multi-gigabyte integer keyspaces with zero pointer chasing overhead.

7. **Virtual Memory On-Demand Paging**:
   - **Inspiration**: Unix kernel virtual memory subsystems utilizing lazy physical page frame commitment via `mmap` and `madvise(MADV_DONTNEED)`.

---

## Tools & Environment

- Developed with the **Google Antigravity IDE**.
- Built with **CMake**, modern **C++20/C++26** Concepts, and compiler toolchains (Apple Clang, GCC, LLVM Clang).
