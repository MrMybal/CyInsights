// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later
#include "LegacyTracePackets.h"
#include <iostream>
#include <algorithm>

static int Checks = 0, Failures = 0;
static void Check(bool Value) { ++Checks; if (!Value) ++Failures; }
int main()
{
    using cyinsights::legacy::PacketStream;
    const std::vector<uint8_t> Stream{'E','C','R','T',3,4, 7,0,1,0,11,12,13, 6,0,0x80,0x80,22,23};
    for (size_t Chunk = 1; Chunk <= Stream.size(); ++Chunk) {
        PacketStream Parser; std::vector<uint8_t> Output;
        for (size_t Offset = 0; Offset < Stream.size(); Offset += Chunk) {
            Check(Parser.Feed(Stream.data() + Offset, std::min(Chunk, Stream.size() - Offset),
                [&](const uint8_t* Data, size_t Size) { Output.insert(Output.end(), Data, Data + Size); return true; }));
        }
        Check(Parser.HasHeader() && Parser.IncompleteBytes() == 0 && Output == Stream);
    }
    for (size_t Cut = 6; Cut < Stream.size(); ++Cut) {
        PacketStream Parser; std::vector<uint8_t> Output;
        Check(Parser.Feed(Stream.data(), Cut, [&](const uint8_t* Data, size_t Size) {
            Output.insert(Output.end(), Data, Data + Size); return true; }));
        const size_t Expected = Cut < 13 ? 6 : 13;
        Check(Output.size() == Expected && Parser.IncompleteBytes() == Cut - Expected);
    }
    PacketStream Bad;
    const uint8_t Invalid[] = {'E','C','R','T',3,4, 3,0,0,0};
    Check(!Bad.Feed(Invalid, sizeof(Invalid), [](const uint8_t*, size_t) { return true; }));
    Check(!Bad.GetError().empty());
    PacketStream Failure;
    Check(!Failure.Feed(Stream.data(), Stream.size(), [](const uint8_t*, size_t) { return false; }));
    PacketStream InvalidMagic;
    const uint8_t Header[] = {0,0,0,0,3,4};
    Check(!InvalidMagic.Feed(Header, sizeof(Header), [](const uint8_t*, size_t) { return true; }));
    PacketStream Bounded;
    Check(Bounded.Feed(nullptr, 0, [](const uint8_t*, size_t) { return true; }));
    PacketStream Missing;
    Check(!Missing.Feed(nullptr, 1, [](const uint8_t*, size_t) { return true; }));
    std::vector<uint8_t> Huge(65537);
    Check(!Bounded.Feed(Huge.data(), Huge.size(), [](const uint8_t*, size_t) { return true; }));
    std::cout << Checks << " checks, " << Failures << " failures\n";
    return Failures ? 1 : 0;
}
