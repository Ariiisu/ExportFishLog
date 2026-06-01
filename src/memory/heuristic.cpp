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
    // ---- PE view -----------------------------------------------------------

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
        std::vector<section_range> data; // multiple .data segments are allowed
        const IMAGE_DATA_DIRECTORY* exception_dir;

        bool in_bounds(std::uint32_t rva, std::size_t need) const noexcept
        {
            return static_cast<std::size_t>(rva) + need <= image_size;
        }

        bool in_data(std::uint64_t va_or_rva) const noexcept
        {
            if (va_or_rva > 0xFFFFFFFFu) return false;
            const auto rva = static_cast<std::uint32_t>(va_or_rva);
            for (const auto& d : data)
                if (d.contains(rva)) return true;
            return false;
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

        out.image         = image;
        out.image_size    = image_size;
        out.nt            = nt;
        out.text          = {};
        out.data.clear();
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
                out.data.push_back(r);
        }

        if (out.text.end_rva == 0 || out.data.empty())
            return false;
        if (out.exception_dir->Size == 0 || out.exception_dir->VirtualAddress == 0)
            return false;

        return true;
    }

    // ---- Runtime function enumeration via .pdata --------------------------

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

        // x64 RUNTIME_FUNCTION = 3x DWORD: BeginAddress, EndAddress, UnwindData.
        constexpr std::size_t entry_size = 12;
        const auto count = size / entry_size;
        const auto* base = pe.image + rva;

        auto read_entry = [&](std::size_t idx, std::uint32_t& b, std::uint32_t& e, std::uint32_t& u)
        {
            std::memcpy(&b, base + idx * entry_size + 0, sizeof(b));
            std::memcpy(&e, base + idx * entry_size + 4, sizeof(e));
            std::memcpy(&u, base + idx * entry_size + 8, sizeof(u));
        };

        // UNWIND_INFO byte 0: low 3 bits version, high 5 bits flags.
        // UNW_FLAG_CHAININFO == 4 → bit 5 of byte 0 == 0x20.
        constexpr std::uint8_t unw_chaininfo_mask = 0x20;
        auto unwind_is_chained = [&](std::uint32_t unwind_rva) -> bool
        {
            if (!pe.in_bounds(unwind_rva, 1)) return false;
            return (pe.image[unwind_rva] & unw_chaininfo_mask) != 0;
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

            // Merge chained tail entries so the bit-test scanner sees a continuous
            // instruction stream across the logical function.
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

            result.push_back({.begin_rva = b, .end_rva = e});
            i = j;
        }
        return result;
    }

    // ---- Decoding helpers --------------------------------------------------

    struct decoded
    {
        ZydisDecodedInstruction insn;
        std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> ops;
        std::uint32_t rva;
    };

    bool decode_at(const ZydisDecoder& dec, const pe_view& pe,
                   std::uint32_t rva, decoded& out)
    {
        if (!pe.in_bounds(rva, 1)) return false;
        const auto status = ZydisDecoderDecodeFull(
            &dec, pe.image + rva, pe.image_size - rva,
            &out.insn, out.ops.data());
        if (!ZYAN_SUCCESS(status)) return false;
        out.rva = rva;
        return true;
    }

    // Resolve LEA r64,[rip+disp32] → absolute RVA target. Returns 0 on failure.
    std::uint32_t lea_rip_target(const decoded& lea)
    {
        if (lea.insn.mnemonic != ZYDIS_MNEMONIC_LEA) return 0;
        if (lea.ops[0].type != ZYDIS_OPERAND_TYPE_REGISTER) return 0;
        if (lea.ops[1].type != ZYDIS_OPERAND_TYPE_MEMORY) return 0;
        if (lea.ops[1].mem.base != ZYDIS_REGISTER_RIP) return 0;

        ZyanU64 abs = 0;
        if (ZydisCalcAbsoluteAddress(&lea.insn, &lea.ops[1], lea.rva, &abs) != ZYAN_STATUS_SUCCESS)
            return 0;
        if (abs > 0xFFFFFFFFu) return 0;
        return static_cast<std::uint32_t>(abs);
    }

    // Get signed displacement of MOVZX's memory operand (0 for register-only addressing).
    std::int64_t movzx_mem_disp(const decoded& mvz)
    {
        if (mvz.insn.mnemonic != ZYDIS_MNEMONIC_MOVZX) return 0;
        if (mvz.ops[1].type != ZYDIS_OPERAND_TYPE_MEMORY) return 0;
        if (!mvz.ops[1].mem.disp.has_displacement) return 0;
        return mvz.ops[1].mem.disp.value;
    }

    bool is_byte_movzx_mem(const decoded& mvz)
    {
        if (mvz.insn.mnemonic != ZYDIS_MNEMONIC_MOVZX) return false;
        if (mvz.ops[1].type != ZYDIS_OPERAND_TYPE_MEMORY) return false;
        return mvz.ops[1].size == 8; // byte source
    }

    bool is_low_byte_reg(ZydisRegister r) noexcept
    {
        switch (r)
        {
            case ZYDIS_REGISTER_AL: case ZYDIS_REGISTER_CL:
            case ZYDIS_REGISTER_DL: case ZYDIS_REGISTER_BL:
            case ZYDIS_REGISTER_SPL: case ZYDIS_REGISTER_BPL:
            case ZYDIS_REGISTER_SIL: case ZYDIS_REGISTER_DIL:
            case ZYDIS_REGISTER_R8B: case ZYDIS_REGISTER_R9B:
            case ZYDIS_REGISTER_R10B: case ZYDIS_REGISTER_R11B:
            case ZYDIS_REGISTER_R12B: case ZYDIS_REGISTER_R13B:
            case ZYDIS_REGISTER_R14B: case ZYDIS_REGISTER_R15B:
                return true;
            default: return false;
        }
    }

    // After the MOVZX-byte, did the code run a bit-test on the loaded byte?
    // We accept any of the compiler-equivalent shapes:
    //   TEST/AND/OR  byte_reg, byte_reg
    //   BT           reg, reg
    //   SHL/SHR ...,cl  →  TEST byte_reg, byte_reg
    bool is_bit_test_consumer(const ZydisDecoder& dec, const pe_view& pe, std::uint32_t after_rva)
    {
        decoded a{};
        if (!decode_at(dec, pe, after_rva, a)) return false;

        const auto m = a.insn.mnemonic;
        if (m == ZYDIS_MNEMONIC_TEST || m == ZYDIS_MNEMONIC_AND || m == ZYDIS_MNEMONIC_OR)
        {
            return a.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                   a.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                   is_low_byte_reg(a.ops[0].reg.value) &&
                   is_low_byte_reg(a.ops[1].reg.value);
        }
        if (m == ZYDIS_MNEMONIC_BT)
        {
            return a.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                   a.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER;
        }
        if (m == ZYDIS_MNEMONIC_SHL || m == ZYDIS_MNEMONIC_SHR)
        {
            decoded b{};
            if (!decode_at(dec, pe, after_rva + a.insn.length, b)) return false;
            return b.insn.mnemonic == ZYDIS_MNEMONIC_TEST &&
                   b.ops[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                   b.ops[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
                   is_low_byte_reg(b.ops[0].reg.value) &&
                   is_low_byte_reg(b.ops[1].reg.value);
        }
        return false;
    }

    // ---- Spearfish detector ------------------------------------------------
    //
    // Game logic does:  spearId = id - 20000; bit-test caughtSpearfish[spearId>>3].
    // The "-20000" magic is in the GAME (not the compiler), so it survives
    // recompilation. We only need to find an instruction within the same
    // function as the LEA that applies that constant adjustment to a register.
    //
    // Encoding variants we accept:
    //   SUB  r32, 0x4E20
    //   ADD  r32, 0xFFFFB1E0   (== -0x4E20 in 32-bit two's complement)
    //   LEA  r32, [reg + 0xFFFFB1E0]   (== [reg - 0x4E20])

    constexpr std::int32_t SPEAR_OFFSET = 20000;

    bool applies_spear_offset(const decoded& d)
    {
        const auto m = d.insn.mnemonic;
        const auto& op0 = d.ops[0];
        const auto& op1 = d.ops[1];

        if (m == ZYDIS_MNEMONIC_SUB && op0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            op1.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
        {
            return static_cast<std::int32_t>(op1.imm.value.s) == SPEAR_OFFSET;
        }
        if (m == ZYDIS_MNEMONIC_ADD && op0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            op1.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
        {
            return static_cast<std::int32_t>(op1.imm.value.s) == -SPEAR_OFFSET;
        }
        if (m == ZYDIS_MNEMONIC_LEA && op0.type == ZYDIS_OPERAND_TYPE_REGISTER &&
            op1.type == ZYDIS_OPERAND_TYPE_MEMORY &&
            op1.mem.disp.has_displacement)
        {
            return static_cast<std::int32_t>(op1.mem.disp.value) == -SPEAR_OFFSET;
        }
        return false;
    }

    // Linearly scan the function's instruction stream and decide whether the
    // -20000 adjustment appears anywhere before the LEA. We do this once per
    // callsite, but per-function we cache results so multiple LEAs in the same
    // function only pay the cost once.
    bool function_has_spear_adjust(const ZydisDecoder& dec, const pe_view& pe,
                                   std::uint32_t fn_begin_rva, std::uint32_t fn_end_rva,
                                   std::uint32_t lea_rva)
    {
        if (lea_rva <= fn_begin_rva || lea_rva > fn_end_rva) return false;

        std::uint32_t rva = fn_begin_rva;
        while (rva < lea_rva)
        {
            decoded d{};
            if (!decode_at(dec, pe, rva, d))
            {
                rva += 1;
                continue;
            }
            if (applies_spear_offset(d)) return true;
            rva += d.insn.length;
        }
        return false;
    }

    // ---- Bit-test idiom scanner -------------------------------------------

    struct table_info
    {
        std::vector<std::uint32_t> callsites;       // RVAs of the LEA instructions
        std::uint32_t spear_callsite_count = 0;     // how many callsites apply -20000
    };

    void scan_function(const ZydisDecoder& dec, const pe_view& pe,
                       std::uint32_t fn_begin_rva, std::uint32_t fn_end_rva,
                       std::unordered_map<std::uint32_t, table_info>& tables)
    {
        if (fn_end_rva <= fn_begin_rva) return;
        if (!pe.in_bounds(fn_begin_rva, fn_end_rva - fn_begin_rva)) return;

        std::uint32_t rva = fn_begin_rva;
        while (rva < fn_end_rva)
        {
            decoded lea{};
            if (!decode_at(dec, pe, rva, lea))
            {
                rva += 1;
                continue;
            }

            const std::uint32_t step = lea.insn.length;

            // Anchor: LEA r64, [rip+disp32]  with target inside .data.
            const std::uint32_t lea_target = lea_rip_target(lea);
            if (lea_target == 0 || !pe.in_data(lea_target))
            {
                rva += step;
                continue;
            }

            decoded mvz{};
            if (!decode_at(dec, pe, rva + lea.insn.length, mvz) ||
                !is_byte_movzx_mem(mvz))
            {
                rva += step;
                continue;
            }

            const std::int64_t mvz_disp = movzx_mem_disp(mvz);
            const std::uint64_t effective64 =
                static_cast<std::uint64_t>(lea_target) + static_cast<std::uint64_t>(mvz_disp);
            if (!pe.in_data(effective64))
            {
                rva += step;
                continue;
            }
            const auto effective = static_cast<std::uint32_t>(effective64);

            const std::uint32_t after = rva + lea.insn.length + mvz.insn.length;
            if (!is_bit_test_consumer(dec, pe, after))
            {
                rva += step;
                continue;
            }

            // It's a bit-test against our table.
            auto& info = tables[effective];
            info.callsites.push_back(rva);
            if (function_has_spear_adjust(dec, pe, fn_begin_rva, fn_end_rva, rva))
                ++info.spear_callsite_count;

            rva += step;
        }
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

    // Aggregate every bit-test callsite across the entire .text section.
    std::unordered_map<std::uint32_t, table_info> tables;
    for (const auto& fn : read_pdata(pe))
        scan_function(decoder, pe, fn.begin_rva, fn.end_rva, tables);

    if (tables.size() < 2) return std::nullopt;

    // Sort tables by RVA so we can compute "size = gap to next table".
    std::vector<std::uint32_t> sorted_rvas;
    sorted_rvas.reserve(tables.size());
    for (const auto& rva : tables | std::views::keys)
        sorted_rvas.push_back(rva);
    std::ranges::sort(sorted_rvas);

    auto known_size = [&](std::uint32_t rva) -> std::optional<std::uint32_t>
    {
        const auto it = std::ranges::find(sorted_rvas, rva);
        if (it == sorted_rvas.end()) return std::nullopt;
        const auto idx = static_cast<std::size_t>(it - sorted_rvas.begin());
        if (idx + 1 >= sorted_rvas.size()) return std::nullopt;
        return sorted_rvas[idx + 1] - rva;
    };

    // Spear classification: a table is the spearfishing log iff every observed
    // callsite applies the -20000 adjustment in the enclosing function.
    // (Game logic invariant: spearfishing item IDs start at 20000.)
    std::optional<std::uint32_t> spear_rva;
    for (const auto& [rva, info] : tables)
    {
        if (info.spear_callsite_count > 0 &&
            info.spear_callsite_count == info.callsites.size())
        {
            // If multiple tables qualify, prefer the one with the most callsites
            // (the real one will have several xrefs; spurious ones rarely do).
            if (!spear_rva || tables.at(*spear_rva).callsites.size() < info.callsites.size())
                spear_rva = rva;
        }
    }
    if (!spear_rva) return std::nullopt;

    // Fishlog classification: among tables that are NOT spear, with ≥2 callsites
    // and a known forward gap ≥64 bytes (filtering stray single-use tables and
    // unrelated small bitfields), pick the one with the largest known size.
    // FishParameter currently has 1500+ rows ⇒ ~189 bytes; any future growth
    // keeps it the largest bitfield in PlayerState by a wide margin.
    constexpr std::uint32_t MIN_FISHLOG_SIZE = 64;
    std::optional<std::uint32_t> fishlog_rva;
    std::uint32_t best_size = 0;
    for (const auto& [rva, info] : tables)
    {
        if (rva == *spear_rva) continue;
        if (info.callsites.size() < 2) continue;
        const auto sz = known_size(rva);
        if (!sz || *sz < MIN_FISHLOG_SIZE) continue;
        if (*sz > best_size)
        {
            best_size = *sz;
            fishlog_rva = rva;
        }
    }
    if (!fishlog_rva) return std::nullopt;

    return fishlog_globals{
        .fishlog = proc.base_address() + *fishlog_rva,
        .spear_fishlog = proc.base_address() + *spear_rva,
    };
}
