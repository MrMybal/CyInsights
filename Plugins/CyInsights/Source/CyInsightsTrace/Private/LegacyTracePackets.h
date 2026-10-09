// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace cyinsights { namespace legacy {
// UE 4.27 TidPacket transport: preserve complete native packets, never split
// the final packet when our receiver closes. Payloads remain opaque.
class PacketStream
{
public:
    template<class Write> bool Feed(const uint8_t* Data, size_t Count, Write&& Output)
    {
        if (!Error.empty()) return false;
        if (Count == 0) return true;
        if (!Data) return Fail("Missing legacy trace bytes");
        if (Count > 65536 || Pending.size() + Count > 131072) return Fail("Legacy trace buffer exceeded bounded packet storage");
        Pending.insert(Pending.end(), Data, Data + Count);
        size_t Offset = 0;
        if (!Header) {
            if (Pending.size() < 6) return true;
            if (std::memcmp(Pending.data(), "ECRT", 4) != 0 || Pending[4] != 3)
                return Fail("Unsupported legacy native trace transport");
            if (!Output(Pending.data(), 6)) return Fail("Cannot write legacy trace header");
            Header = true;
            Offset = 6;
        }
        while (Pending.size() - Offset >= 4) {
            const size_t Size = size_t(Pending[Offset]) | (size_t(Pending[Offset + 1]) << 8);
            if (Size < 4) return Fail("Invalid legacy trace packet length");
            if (Size > Pending.size() - Offset) break;
            if (!Output(Pending.data() + Offset, Size)) return Fail("Cannot write legacy trace packet");
            Offset += Size;
        }
        Pending.erase(Pending.begin(), Pending.begin() + Offset);
        return true;
    }
    size_t IncompleteBytes() const { return Pending.size(); }
    bool HasHeader() const { return Header; }
    const std::string& GetError() const { return Error; }
private:
    bool Fail(const char* Message) { Error = Message; return false; }
    std::vector<uint8_t> Pending;
    std::string Error;
    bool Header = false;
};
}}
