/* SPDX-License-Identifier: GPL-2.0-only */
#include "native_mali_kmod.h"
#include "NativeMaliABI.h"

#include <cstdio>
#include <cstring>

static int reply(void *context, uint32_t selector, const void *, size_t,
                 void *output, size_t bytes) {
    if (selector == NativeMaliMethodQuery && bytes == sizeof(NativeMaliAbiQuery)) {
        NativeMaliAbiQuery query{};
        query.abi_version = NativeMaliAbiVersion;
        query.max_bo_bytes = UINT64_C(1) << NativeMaliAbiMaxBoBytesShift;
        std::memcpy(output, &query, bytes);
        return 0;
    }
    if (selector == NativeMaliMethodGetDeviceInfo && bytes == sizeof(NativeMaliAbiDeviceInfo)) {
        std::memcpy(output, context, bytes);
        return 0;
    }
    return -1;
}

int main() {
    struct Case { uint64_t start, bytes; unsigned shift; bool supported; };
    const Case cases[] = {
        {0, 8192, 12, true}, {4096, 4096, 12, true},
        {1, 8192, 12, true}, {0, 4096, 12, false},
        {4097, 4096, 12, false}, {UINT64_MAX - 4095, 8192, 12, false},
        {4096, 8192, 14, false},
    };
    for (const auto &c : cases) {
        NativeMaliAbiDeviceInfo info{};
        info.va_start = c.start;
        info.va_size = c.bytes;
        info.pgsize_shift = c.shift;
        NativeMaliTransportCallbacks transport{reply, &info};
        NativeMaliResult result;
        auto *device = native_mali_kmod_create(&transport, &result);
        const bool opened = device != nullptr;
        native_mali_kmod_destroy(device);
        if (opened != c.supported ||
            result != (c.supported ? NATIVE_MALI_RESULT_OK : NATIVE_MALI_RESULT_UNSUPPORTED)) {
            std::fprintf(stderr, "unexpected geometry result for start=%llu bytes=%llu shift=%u\n",
                         (unsigned long long)c.start, (unsigned long long)c.bytes, c.shift);
            return 1;
        }
    }
    std::puts("OK   native GPU page geometry");
}
