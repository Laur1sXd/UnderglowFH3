#include "pe.hpp"

#include <cstdint>
#include <cstring>

namespace core::pe {

bool readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!VirtualQuery(p, &mi, sizeof mi) || mi.State != MEM_COMMIT) return false;
    if (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return static_cast<const uint8_t *>(p) + n <= static_cast<const uint8_t *>(mi.BaseAddress) + mi.RegionSize;
}

const IMAGE_NT_HEADERS64 *headers(const void *base)
{
    auto *b = static_cast<const uint8_t *>(base);
    auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(b);
    if (!b || !readable(b, sizeof *dos) || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    if (dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return nullptr;
    auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(b + dos->e_lfanew);
    if (!readable(nt, sizeof *nt) || nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return nullptr;
    return nt;
}

} // namespace core::pe
