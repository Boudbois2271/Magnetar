// SPDX-License-Identifier: GPL-3.0-or-later
#include "macro.h"

#include "frame.h"

#include <algorithm>

namespace akko {

std::size_t macroSize(const Macro &m)
{
    return 1 + m.name.size() + 4 * m.steps.size();
}

std::size_t macroStorageUsed(std::span<const Macro> macros)
{
    std::size_t n = 0;
    for (const auto &m : macros)
        n += 4 + macroSize(m);
    return n;
}

std::string macroProblem(const Macro &m)
{
    if (m.name.size() < kMinMacroName || m.name.size() > kMaxMacroName)
        return "a macro name needs 2 to 255 characters";
    if (!std::all_of(m.name.begin(), m.name.end(), [](char c) { return c >= 0x20 && c <= 0x7e; }))
        return "a macro name can only hold plain ASCII characters";
    if (m.steps.empty())
        return "a macro needs at least one step";
    if (std::any_of(m.steps.begin(), m.steps.end(), [](const MacroStep &s) { return s.usage == 0; }))
        return "a macro step has no key";
    return {};
}

std::string macrosProblem(std::span<const Macro> macros)
{
    for (const auto &m : macros)
        if (auto p = macroProblem(m); !p.empty())
            return p;
    if (macroStorageUsed(macros) > kMacroStorage)
        return "the macros don't fit in the keyboard's 4096 bytes";
    if (macros.size() > 256)
        return "the keyboard can't address more than 256 macros";
    return {};
}

Macro normalizedMacro(Macro m)
{
    if (!m.steps.empty())
        m.steps.back().delayMs = 1;
    return m;
}

namespace {

std::uint16_t le16(std::span<const std::uint8_t> b, std::size_t at)
{
    return static_cast<std::uint16_t>(b[at] | b[at + 1] << 8);
}

}  // namespace

std::vector<Macro> parseMacroBlob(std::span<const std::uint8_t> blob)
{
    std::vector<Macro> out;
    if (blob.size() < 2 || (blob[0] == 0xff && blob[1] == 0xff))
        return out;  // empty storage
    const std::size_t dir = le16(blob, 0);  // the first macro's offset = directory size
    if (dir % 4 != 0 || dir > blob.size())
        throw Error("bad macro directory");
    for (std::size_t e = 0; e < dir; e += 4) {
        const std::size_t offset = le16(blob, e), size = le16(blob, e + 2);
        if (size < 1 || offset < dir || offset + size > blob.size())
            throw Error("bad macro directory entry");
        const auto bytes = blob.subspan(offset, size);
        const std::size_t nameLen = bytes[0];
        if (1 + nameLen > size || (size - 1 - nameLen) % 4 != 0)
            throw Error("bad macro record");
        Macro m;
        m.name.assign(bytes.begin() + 1, bytes.begin() + 1 + static_cast<std::ptrdiff_t>(nameLen));
        for (std::size_t at = 1 + nameLen; at + 4 <= size; at += 4)
            m.steps.push_back({(bytes[at] & kMacroRelease) == 0, static_cast<std::uint16_t>(bytes[at + 1] << 8 | bytes[at + 2]),
                               bytes[at + 3]});
        out.push_back(std::move(m));
    }
    return out;
}

std::vector<std::uint8_t> macroBlob(std::span<const Macro> macros)
{
    if (auto p = macrosProblem(macros); !p.empty())
        throw Error(p);
    std::vector<std::uint8_t> dir, body;
    std::size_t offset = 4 * macros.size();
    for (const auto &m : macros) {
        const std::size_t size = macroSize(m);
        for (std::size_t v : {offset, size}) {
            dir.push_back(static_cast<std::uint8_t>(v & 0xFF));
            dir.push_back(static_cast<std::uint8_t>(v >> 8));
        }
        offset += size;
        body.push_back(static_cast<std::uint8_t>(m.name.size()));
        body.insert(body.end(), m.name.begin(), m.name.end());
        for (const auto &s : m.steps) {
            body.push_back(static_cast<std::uint8_t>((s.press ? 0x00 : kMacroRelease) |
                                                (isModifierUsage(s.usage) ? kMacroModifier : 0x00)));
            body.push_back(static_cast<std::uint8_t>(s.delayMs >> 8));
            body.push_back(static_cast<std::uint8_t>(s.delayMs & 0xFF));
            body.push_back(s.usage);
        }
    }
    dir.insert(dir.end(), body.begin(), body.end());
    return dir;
}

Action macroAction(const MacroBinding &b)
{
    if (b.index < 0 || b.index > 255)
        throw Error("no such macro: " + std::to_string(b.index + 1));
    const bool times = b.loop == MacroLoop::Times;
    if (times && (b.count < 1 || b.count > kMaxMacroRepeat))
        throw Error("a macro can repeat 1 to 255 times");
    return {0x03, static_cast<std::uint8_t>(b.loop), static_cast<std::uint8_t>(times ? b.count : 0),
            static_cast<std::uint8_t>(b.index)};
}

std::optional<MacroBinding> macroBinding(const Action &a)
{
    if (a[0] != 0x03)
        return std::nullopt;
    const auto loop = static_cast<MacroLoop>(a[1]);
    if (loop != MacroLoop::Times && loop != MacroLoop::UntilKeyPressed && loop != MacroLoop::WhileHeld)
        return std::nullopt;
    return MacroBinding{a[3], loop, loop == MacroLoop::Times ? a[2] : 0};
}

}  // namespace akko
