#pragma once

#include <cstdint>

// Dev tools for the game's asset table (the "toc", loaded into memory at start-up) and its archive reads. Research for loading atmosphere .smpcmod
// files from disk (docs/ROADMAP.md 4b, docs/RECON.md Part 17). Nothing here runs until a command asks for it.
namespace toc {
    // Finds the 4 consecutive sorted asset ids in the game's memory (the id table) and, from the offline layout, prints that asset's offsets and sizes entries.
    // `index` is the asset's index in the id table as tools/tocinfo.py prints it.
    void Find(uint32_t index, const uint64_t pat[4], uintptr_t offsets_delta, uintptr_t sizes_delta);

    // For `ms` milliseconds, logs the game's ReadFile calls on files under asset_archive (path, offset, length, caller). First use installs the hook.
    void IoTrace(unsigned ms);

    // Locate the toc in memory, then redirect an unused asset's loads to a loose file (see toc.cpp). `use_archive` is a toc archive index of a raw archive the game has open.
    bool Locate(bool quiet = false);
    bool AddCarrier(uint64_t carrier_id, uint32_t use_archive, const wchar_t* path);
    void RestoreCarriers();
    bool AddAsset(uint64_t id, uint32_t use_archive, const wchar_t* path, uint32_t size = 0);   // size 0: read it from the file now; else the file may appear later   // a genuinely new asset in span 255; ids must ascend
    int PickRawArchive();   // an archive index (a raw Overstrike mod archive that exists) the new assets can borrow, or -1
    int PreparePlaceholder();   // experiment: turn a language archive entry the install does not have into a raw archive of ours; its index, or -1
    void FindPointers();   // research: where the game keeps pointers to the toc sections
}
