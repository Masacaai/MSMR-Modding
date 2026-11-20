#pragma once

#include <cstdint>

// The game's asset table (the "toc"), found in memory, and the loose-file reads that serve this mod's own assets (docs/RECON.md Parts 17 and 23).
namespace toc {

    // For `ms` milliseconds, logs the game's ReadFile calls on files under asset_archive (path, offset, length, caller). First use installs the hook.
    void IoTrace(unsigned ms);

    // Finds the toc in memory (a heap scan; RECON Part 23 has the deterministic route through the file system object, checked by `fsinfo`).
    bool Locate(bool quiet = false);
    bool AddAsset(uint64_t id, uint32_t use_archive, const wchar_t* path, uint32_t size = 0);   // size 0: read it from the file now; else the file may appear later   // a genuinely new asset in span 0; ids must ascend
    void FsInfo();   // research: dumps the game's file system object (q[exe+74a6a00]) and its toc object, and compares them with the toc found by the heap scan
    int PickRawArchive();   // an archive index (a raw Overstrike mod archive that exists) the new assets can borrow, or -1
}
