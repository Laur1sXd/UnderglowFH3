// PE headers and readable ranges for mapped images.
#pragma once

#include <windows.h>
#include <cstddef>

namespace core::pe {

// [p, p + n) is committed, inside one region and readable.
bool readable(const void *p, size_t n);

// The PE64 headers of the image mapped at base, or null (no MZ/PE, not PE32+, or unreadable).
const IMAGE_NT_HEADERS64 *headers(const void *base);

} // namespace core::pe
