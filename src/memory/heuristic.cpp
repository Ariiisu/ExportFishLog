#include "heuristic.h"

#include <Windows.h>
#include <Zydis/Zydis.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace
{
    struct section_range
    {
        std::uint32_t begin_rva;
        std::uint32_t end_rva; // exclusive

        bool contains(std::uint32_t rva) const noexcept
        {
            return rva >= begin_rva && rva < end_rva;
        }
    };

    struct pe_view
    {
        const std::uint8_t* image;
        std::size_t image_size;
        const IMAGE_NT_HEADERS64* nt;
        section_range text;
        section_range data;
        const IMAGE_DATA_DIRECTORY* exception_dir;

        bool in_bounds(std::uint32_t rva, std::size_t need) const noexcept
        {
            return static_cast<std::size_t>(rva) + need <= image_size;
        }
    };

    bool parse_pe(const std::uint8_t* image, std::size_t image_size, pe_view& out)
    {
        if (image_size < sizeof(IMAGE_DOS_HEADER))
            return false;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return false;

        const auto nt_off = static_cast<std::size_t>(dos->e_lfanew);
        if (nt_off + sizeof(IMAGE_NT_HEADERS64) > image_size)
            return false;

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + nt_off);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return false;
        if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;

        out.image      = image;
        out.image_size = image_size;
        out.nt         = nt;
        out.text       = {};
        out.data       = {};
        out.exception_dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

        const auto* sections = IMAGE_FIRST_SECTION(nt);
        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            const auto& s = sections[i];
            const auto size = s.Misc.VirtualSize ? s.Misc.VirtualSize : s.SizeOfRawData;
            const section_range r{s.VirtualAddress, s.VirtualAddress + size};

            const char* name = reinterpret_cast<const char*>(s.Name);
            if (std::strncmp(name, ".text", IMAGE_SIZEOF_SHORT_NAME) == 0)
                out.text = r;
            else if (std::strncmp(name, ".data", IMAGE_SIZEOF_SHORT_NAME) == 0)
                out.data = r;
        }

        if (out.text.end_rva == 0 || out.data.end_rva == 0)
            return false;
        if (out.exception_dir->Size == 0 || out.exception_dir->VirtualAddress == 0)
            return false;

        return true;
    }

    bool is_low_byte_reg(ZydisRegister r) noexcept
    {
        switch (r)
        {
            case ZYDIS_REGISTER_AL:
            case ZYDIS_REGISTER_CL:
            case ZYDIS_REGISTER_DL:
            case ZYDIS_REGISTER_BL:
            case ZYDIS_REGISTER_SPL:
            case ZYDIS_REGISTER_BPL:
            case ZYDIS_REGISTER_SIL:
            case ZYDIS_REGISTER_DIL:
            case ZYDIS_REGISTER_R8B:
            case ZYDIS_REGISTER_R9B:
            case ZYDIS_REGISTER_R10B:
            case ZYDIS_REGISTER_R11B:
            case ZYDIS_REGISTER_R12B:
            case ZYDIS_REGISTER_R13B:
            case ZYDIS_REGISTER_R14B:
            case ZYDIS_REGISTER_R15B:
                return true;
            default:
                return false;
        }
    }

    struct decoded
    {
        ZydisDecodedInstruction insn;
        std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> ops;
        std::uint32_t rva;
    };

    // Resolve a LEA r64, [rip+disp32] to its target RVA (only if target is in .data).
    // Returns the target RVA and the destination register on success, or 0/NONE on failure.
    struct lea_anchor
    {
        std::uint32_t target_rva;
        ZydisRegister dst_reg;
    };

    std::optional<lea_anchor> resolve_lea_data_target(const decoded& lea, const pe_view& pe)
    {
        if (lea.insn.mnemonic != ZYDIS_MNEMONIC_LEA) return std::nullopt;
        if (lea.ops[0].type != ZYDIS_OPERAND_TYPE_REGISTER) return std::nullopt;
        if (lea.ops[1].type != ZYDIS_OPERAND_TYPE_MEMORY) return std::nullopt;
        if (lea.ops[1].mem.base != ZYDIS_REGISTER_RIP) return std::nullopt;

        ZyanU64 abs = 0;
        if (ZydisCalcAbsoluteAddress(&lea.insn, &lea.ops[1], lea.rva, &abs) != ZYAN_STATUS_SUCCESS)
            return std::nullopt;
        if (abs > 0xFFFFFFFFu) return std::nullopt;
        const auto target = static_cast<std::uint32_t>(abs);
        if (!pe.data.contains(target)) return std::nullopt;

        return lea_anchor{target, lea.ops[0].reg.value};
    }

    // Check that MOVZX loads a byte from memory whose addressing uses the LEA's reg.
    bool movzx_byte_uses(const decoded& mvz, ZydisRegister lea_reg)
    {
        if (mvz.insn.mnemonic != ZYDIS_MNEMONIC_MOVZX) return false;
        if (mvz.ops[1].type != ZYDIS_OPERAND_TYPE_MEMORY) return false;
        if (mvz.ops[1].size != 8) return false; // byte load
        return mvz.ops[1].mem.base == lea_reg || mvz.ops[1].mem.index == lea_reg;
    }

    // Arm A (MSVC bit-test idiom):
    //   LEA r64, [rip+disp]; MOVZX r32, byte ptr [r+lea_reg]; SHL/SHR r32, cl; TEST r8, r8
    bool match_arm_shift_test(const decoded& sft, const decoded& tst)
    {
        if (sft.insn.mnemonic != ZYDIS_MNEMONIC_SHL && sft.insn.mnemonic != ZYDIS_MNEMONIC_SHR) return false;
        if (sft.ops[1].type != ZYDIS_OPERAND_TYPE_REGISTER) return false;
        if (sft.ops[1].reg.value != ZYDIS_REGISTER_CL) return false;

        if (tst.insn.mnemonic != ZYDIS_MNEMONIC_TEST) return false;
        if (tst.ops[0].type != ZYDIS_OPERAND_TYPE_REGISTER) return false;
        if (tst.ops[1].type != ZYDIS_OPERAND_TYPE_REGISTER) return false;
        if (!is_low_byte_reg(tst.ops[0].reg.value)) return false;
        if (!is_low_byte_reg(tst.ops[1].reg.value)) return false;
        return true;
    }

    // Arm B (clang-cl bit-test idiom):
    //   LEA r64, [rip+disp]; MOVZX r32, byte ptr [r+lea_reg]; BT r32, r32
    // The BT instruction sets CF directly from the bit-test, so no shift+test pair.
    bool match_arm_bt(const decoded& bt)
    {
        if (bt.insn.mnemonic != ZYDIS_MNEMONIC_BT) return false;
        if (bt.ops[0].type != ZYDIS_OPERAND_TYPE_REGISTER) return false;
        if (bt.ops[1].type != ZYDIS_OPERAND_TYPE_REGISTER) return false;
        // Bit-base must be a 32-bit (or wider) reg, bit-offset must be a GPR — both true
        // by construction here since we're already filtering to GPR/GPR.
        return true;
    }

    // Try both arms against a sliding window. Returns the target RVA on match.
    // `filled` is the current usable length of `w` (3 or 4).
    std::uint32_t match_idiom(const std::array<decoded, 4>& w, std::size_t filled, const pe_view& pe)
    {
        if (filled < 3) return 0;

        const auto anchor = resolve_lea_data_target(w[0], pe);
        if (!anchor) return 0;

        if (!movzx_byte_uses(w[1], anchor->dst_reg)) return 0;

        // Arm B (clang BT, 3 insns): LEA -> MOVZX -> BT
        if (match_arm_bt(w[2])) return anchor->target_rva;

        // Arm A (MSVC shift+test, 4 insns): LEA -> MOVZX -> SHL/SHR -> TEST
        if (filled >= 4 && match_arm_shift_test(w[2], w[3])) return anchor->target_rva;

        return 0;
    }

    // Decode a single function range, push bit-test idiom hits into the map.
    void scan_function(const pe_view& pe, const ZydisDecoder& decoder,
                       std::uint32_t begin_rva, std::uint32_t end_rva,
                       std::unordered_map<std::uint32_t, std::uint32_t>& hits)
    {
        if (end_rva <= begin_rva) return;
        if (!pe.in_bounds(begin_rva, end_rva - begin_rva)) return;

        const std::uint8_t* code = pe.image + begin_rva;
        const std::size_t   total = end_rva - begin_rva;

        std::array<decoded, 4> window{};
        std::size_t filled = 0;
        std::size_t off = 0;

        while (off < total)
        {
            decoded next{};
            const auto status = ZydisDecoderDecodeFull(
                &decoder, code + off, total - off,
                &next.insn, next.ops.data());

            if (!ZYAN_SUCCESS(status))
            {
                // bad opcode — resync by skipping a byte, keep scanning
                off += 1;
                filled = 0;
                continue;
            }

            next.rva = begin_rva + static_cast<std::uint32_t>(off);

            if (filled < 4)
            {
                window[filled++] = next;
            }
            else
            {
                window[0] = window[1];
                window[1] = window[2];
                window[2] = window[3];
                window[3] = next;
            }

            // Run the matcher whenever we have ≥ 3 instructions so we catch both the
            // 3-insn BT arm and the 4-insn shift+test arm.
            if (filled >= 3)
            {
                if (const auto target = match_idiom(window, filled, pe); target != 0)
                {
                    // de-dup: keep first occurrence per target (only need set membership)
                    hits.emplace(target, window[0].rva);
                }
            }

            off += next.insn.length;
        }
    }

    struct runtime_function
    {
        std::uint32_t begin_rva;
        std::uint32_t end_rva;
    };

    std::vector<runtime_function> read_pdata(const pe_view& pe)
    {
        std::vector<runtime_function> result;

        const auto rva  = pe.exception_dir->VirtualAddress;
        const auto size = pe.exception_dir->Size;
        if (!pe.in_bounds(rva, size)) return result;

        // RUNTIME_FUNCTION on x64 is 3x DWORD: BeginAddress, EndAddress, UnwindData
        constexpr std::size_t entry_size = 12;
        const auto count = size / entry_size;

        const auto* base = pe.image + rva;
        auto read_entry = [&](std::size_t idx, std::uint32_t& b, std::uint32_t& e, std::uint32_t& u)
        {
            std::memcpy(&b, base + idx * entry_size + 0, sizeof(b));
            std::memcpy(&e, base + idx * entry_size + 4, sizeof(e));
            std::memcpy(&u, base + idx * entry_size + 8, sizeof(u));
        };

        // UNWIND_INFO byte 0 layout: Version (bits 0-2) | Flags (bits 3-7).
        // UNW_FLAG_CHAININFO == 0x04 within the Flags field, i.e. bit 5 of byte 0.
        constexpr std::uint8_t unw_flag_chaininfo_byte0 = 0x20;
        auto unwind_is_chained = [&](std::uint32_t unwind_rva) -> bool
        {
            if (!pe.in_bounds(unwind_rva, 1)) return false;
            return (pe.image[unwind_rva] & unw_flag_chaininfo_byte0) != 0;
        };

        result.reserve(count);
        std::size_t i = 0;
        while (i < count)
        {
            std::uint32_t b{}, e{}, u{};
            read_entry(i, b, e, u);
            if (b == 0 || e <= b || !pe.text.contains(b))
            {
                ++i;
                continue;
            }

            // Merge chained tail entries: when the next entry is physically adjacent
            // and its UNWIND_INFO has UNW_FLAG_CHAININFO set, it's a continuation of
            // the same logical function. Sliding-window matching needs to see them
            // as one continuous instruction stream.
            std::size_t j = i + 1;
            while (j < count)
            {
                std::uint32_t nb{}, ne{}, nu{};
                read_entry(j, nb, ne, nu);
                if (nb != e) break;
                if (!unwind_is_chained(nu)) break;
                e = ne;
                ++j;
            }

            result.push_back({b, e});
            i = j;
        }
        return result;
    }
}

std::optional<mem::fishlog_globals> mem::find_fishlog_globals(const process& proc)
{
    pe_view pe{};
    if (!parse_pe(proc.image_data(), proc.image_size(), pe))
        return std::nullopt;

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
        return std::nullopt;

    // target_rva -> first observed callsite RVA (used only as proof we saw it)
    std::unordered_map<std::uint32_t, std::uint32_t> hits;

    for (const auto& fn : read_pdata(pe))
    {
        scan_function(pe, decoder, fn.begin_rva, fn.end_rva, hits);
    }

    if (hits.size() < 2)
        return std::nullopt;

    std::vector<std::uint32_t> targets;
    targets.reserve(hits.size());
    for (const auto& [t, _] : hits) targets.push_back(t);
    std::ranges::sort(targets);

    // Cluster: consecutive targets within 256 bytes belong to the same group.
    constexpr std::uint32_t cluster_gap = 256;
    std::vector<std::vector<std::uint32_t>> clusters;
    {
        std::vector<std::uint32_t> cur{targets.front()};
        for (std::size_t i = 1; i < targets.size(); ++i)
        {
            if (targets[i] - cur.back() <= cluster_gap)
            {
                cur.push_back(targets[i]);
            }
            else
            {
                clusters.push_back(std::move(cur));
                cur = {targets[i]};
            }
        }
        clusters.push_back(std::move(cur));
    }

    // Score each cluster by the largest inferred table size inside it.
    // The fishlog table (~190 bytes) dominates every other bit-test table.
    auto inferred_size = [](const std::vector<std::uint32_t>& c, std::size_t i) -> std::uint32_t
    {
        if (i + 1 < c.size()) return c[i + 1] - c[i];
        return 64; // last entry — bounded but unknown
    };

    auto cluster_score = [&](const std::vector<std::uint32_t>& c) -> std::uint32_t
    {
        if (c.size() < 2) return 0;
        std::uint32_t best = 0;
        for (std::size_t i = 0; i + 1 < c.size(); ++i)
            best = std::max(best, c[i + 1] - c[i]);
        return best;
    };

    const auto best_it = std::ranges::max_element(clusters, {}, cluster_score);
    if (best_it == clusters.end()) return std::nullopt;
    const auto& cluster = *best_it;
    if (cluster.size() < 2) return std::nullopt;
    if (cluster_score(cluster) < 64) return std::nullopt; // sanity: real fishlog is >>64 bytes

    // fishlog = entry with the LARGEST gap-to-next inside the cluster
    std::size_t fishlog_idx = 0;
    std::uint32_t best_size = 0;
    for (std::size_t i = 0; i + 1 < cluster.size(); ++i)
    {
        const auto s = inferred_size(cluster, i);
        if (s > best_size)
        {
            best_size = s;
            fishlog_idx = i;
        }
    }

    const auto fishlog_rva = cluster[fishlog_idx];
    const auto spear_rva   = cluster.back();
    if (fishlog_rva == spear_rva) return std::nullopt;

    return fishlog_globals{
        proc.base_address() + fishlog_rva,
        proc.base_address() + spear_rva,
    };
}
