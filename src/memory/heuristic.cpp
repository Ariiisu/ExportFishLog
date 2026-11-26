#include "heuristic.h"

#include <Windows.h>
#include <Zydis/Zydis.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    // ---- PE view -----------------------------------------------------------

    struct section_range
    {
        std::uint32_t begin_rva;
        std::uint32_t end_rva; // exclusive
    };

    bool in_ranges(const std::vector<section_range>& ranges, std::int64_t rva) noexcept
    {
        return std::ranges::any_of(ranges, [rva](const section_range& r) { return rva >= r.begin_rva && rva < r.end_rva; });
    }

    struct pe_view
    {
        const std::uint8_t* image;
        std::size_t image_size;
        std::vector<section_range> code; // executable sections
        std::vector<section_range> data; // writable, non-executable sections
        IMAGE_DATA_DIRECTORY exception_dir;

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
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;

        const auto* sections = IMAGE_FIRST_SECTION(nt);
        if (reinterpret_cast<const std::uint8_t*>(sections + nt->FileHeader.NumberOfSections) > image + image_size)
            return false;

        out.image         = image;
        out.image_size    = image_size;
        out.exception_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        out.code.clear();
        out.data.clear();

        // Classify by permissions, not names: other linkers name and split
        // sections differently (lld-link keeps a separate .bss, for one).
        for (std::uint16_t i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            const auto& s    = sections[i];
            const auto size  = s.Misc.VirtualSize ? s.Misc.VirtualSize : s.SizeOfRawData;
            const section_range r{s.VirtualAddress, s.VirtualAddress + size};
            if (s.Characteristics & IMAGE_SCN_MEM_EXECUTE)
                out.code.push_back(r);
            else if (s.Characteristics & IMAGE_SCN_MEM_WRITE)
                out.data.push_back(r);
        }

        return !out.code.empty() && !out.data.empty();
    }

    // ---- Code regions --------------------------------------------------------
    //
    // Every executable byte is scanned once. Unwind info (.pdata) gives the
    // function boundaries; fragments a compiler moved away from their function
    // (chained unwind info: cold code, PGO hot/cold splitting) are attributed to
    // the function they belong to. Code without unwind info (leaf functions,
    // thunks, padding) is scanned as standalone regions.

    struct code_region
    {
        std::uint32_t begin_rva;
        std::uint32_t end_rva;
        std::uint32_t function; // RVA identifying the owning function
        bool standalone;        // no unwind info: every RET/JMP starts a new function
    };

    constexpr std::uint8_t UNWIND_CHAININFO = 0x20; // UNW_FLAG_CHAININFO in bits 7:3 of UNWIND_INFO byte 0

    std::vector<code_region> code_regions(const pe_view& pe)
    {
        const auto read_entry = [&](std::uint32_t rva, IMAGE_RUNTIME_FUNCTION_ENTRY& out)
        {
            if (!pe.in_bounds(rva, sizeof out))
                return false;
            std::memcpy(&out, pe.image + rva, sizeof out);
            return true;
        };

        // UNWIND_INFO: version/flags, prolog size, code count, frame register,
        // unwind codes padded to an even count, then the parent entry if chained.
        const auto owner = [&](IMAGE_RUNTIME_FUNCTION_ENTRY f)
        {
            for (int depth = 0; depth < 32; ++depth)
            {
                const auto info = f.UnwindInfoAddress;
                if (!pe.in_bounds(info, 4) || !(pe.image[info] & UNWIND_CHAININFO))
                    break;
                const auto codes = pe.image[info + 2];
                if (!read_entry(info + 4 + 2 * ((codes + 1u) & ~1u), f))
                    break;
            }
            return f.BeginAddress;
        };

        std::vector<code_region> regions;
        const auto& dir = pe.exception_dir;
        for (std::uint32_t off = 0; dir.VirtualAddress != 0 && off + sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY) <= dir.Size;
             off += sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY))
        {
            IMAGE_RUNTIME_FUNCTION_ENTRY f{};
            if (!read_entry(dir.VirtualAddress + off, f))
                break;
            if (f.EndAddress <= f.BeginAddress || !in_ranges(pe.code, f.BeginAddress))
                continue;
            regions.push_back({f.BeginAddress, f.EndAddress, owner(f), false});
        }

        std::ranges::sort(regions, {}, &code_region::begin_rva);
        std::vector<code_region> uncovered;
        for (const auto& section : pe.code)
        {
            auto cursor = section.begin_rva;
            for (const auto& r : regions)
            {
                if (r.end_rva <= cursor || r.begin_rva >= section.end_rva)
                    continue;
                if (r.begin_rva > cursor)
                    uncovered.push_back({cursor, r.begin_rva, cursor, true});
                cursor = r.end_rva;
            }
            if (cursor < section.end_rva)
                uncovered.push_back({cursor, section.end_rva, cursor, true});
        }
        regions.insert(regions.end(), uncovered.begin(), uncovered.end());
        return regions;
    }

    // ---- Decoding ------------------------------------------------------------

    struct decoded
    {
        ZydisDecodedInstruction insn;
        std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> ops;
        std::uint32_t rva;
    };

    bool decode_at(const ZydisDecoder& dec, const pe_view& pe, std::uint32_t rva, std::uint32_t end, decoded& out)
    {
        if (rva >= end || !pe.in_bounds(rva, 1))
            return false;
        const auto length = (std::min)(static_cast<std::size_t>(end - rva), pe.image_size - rva);
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&dec, pe.image + rva, length, &out.insn, out.ops.data())))
            return false;
        out.rva = rva;
        return true;
    }

    // Index (0-15) of the 64-bit general purpose register containing reg, or -1.
    int gpr(ZydisRegister reg) noexcept
    {
        const auto full = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg);
        return ZydisRegisterGetClass(full) == ZYDIS_REGCLASS_GPR64 ? ZydisRegisterGetId(full) : -1;
    }

    int gpr(const ZydisDecodedOperand& op) noexcept
    {
        return op.type == ZYDIS_OPERAND_TYPE_REGISTER ? gpr(op.reg.value) : -1;
    }

    bool is_gpr64(const ZydisDecodedOperand& op) noexcept
    {
        return op.type == ZYDIS_OPERAND_TYPE_REGISTER && ZydisRegisterGetClass(op.reg.value) == ZYDIS_REGCLASS_GPR64;
    }

    bool ends_flow(ZydisMnemonic m) noexcept
    {
        return m == ZYDIS_MNEMONIC_RET || m == ZYDIS_MNEMONIC_JMP || m == ZYDIS_MNEMONIC_INT3 || m == ZYDIS_MNEMONIC_UD2;
    }

    template <typename F>
    void for_each_written_gpr(const decoded& d, F&& f)
    {
        for (ZyanU8 i = 0; i < d.insn.operand_count; ++i)
        {
            if (d.ops[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE)
                if (const int r = gpr(d.ops[i]); r >= 0)
                    f(r);
        }
    }

    // ---- Register tracking ---------------------------------------------------
    //
    // A linear walk that remembers which registers hold an address in the
    // image: rip-relative LEAs to writable data or to the image base (for
    // [image_base + index + table_rva] addressing), followed through copies,
    // constant offsets and added indices. This is what frees the matcher from
    // instruction scheduling: the LEA may be hoisted out of a loop, and other
    // instructions may sit between it, the load and the bit test.

    struct pointer
    {
        std::int64_t rva;
        bool indexed; // a variable index has been added
    };

    using registers = std::array<std::optional<pointer>, 16>;

    constexpr std::array volatile_registers{0, 1, 2, 8, 9, 10, 11}; // rax rcx rdx r8-r11 (Windows x64 ABI)

    std::optional<pointer> address_of(const ZydisDecodedOperand& op, const registers& regs)
    {
        if (op.type != ZYDIS_OPERAND_TYPE_MEMORY)
            return std::nullopt;
        const int base         = gpr(op.mem.base);
        const int index        = gpr(op.mem.index);
        const bool base_known  = base >= 0 && regs[base];
        const bool index_known = index >= 0 && op.mem.scale <= 1 && regs[index];
        if (base_known == index_known) // no known address, or two of them
            return std::nullopt;
        const auto& p      = *regs[base_known ? base : index];
        const bool another = base_known ? index >= 0 : base >= 0;
        return pointer{p.rva + (op.mem.disp.has_displacement ? op.mem.disp.value : 0), p.indexed || another};
    }

    void track(const pe_view& pe, const decoded& d, registers& regs)
    {
        const auto m = d.insn.mnemonic;
        if (ends_flow(m))
        {
            regs = {};
            return;
        }
        if (m == ZYDIS_MNEMONIC_CALL)
        {
            for (const int r : volatile_registers)
                regs[r].reset();
            return;
        }

        const auto& dst  = d.ops[0];
        const auto& src  = d.ops[1];
        const int target = is_gpr64(dst) ? gpr(dst) : -1;

        std::optional<pointer> value;
        if (target >= 0)
        {
            if (m == ZYDIS_MNEMONIC_LEA && src.mem.base == ZYDIS_REGISTER_RIP)
            {
                ZyanU64 rva = 0;
                if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&d.insn, &src, d.rva, &rva)) &&
                    (rva == 0 || in_ranges(pe.data, static_cast<std::int64_t>(rva))))
                    value = pointer{static_cast<std::int64_t>(rva), false};
            }
            else if (m == ZYDIS_MNEMONIC_LEA)
                value = address_of(src, regs);
            else if (m == ZYDIS_MNEMONIC_MOV && is_gpr64(src))
                value = regs[gpr(src)];
            else if ((m == ZYDIS_MNEMONIC_ADD || m == ZYDIS_MNEMONIC_SUB) && regs[target])
            {
                if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
                    value = pointer{regs[target]->rva + (m == ZYDIS_MNEMONIC_ADD ? src.imm.value.s : -src.imm.value.s), regs[target]->indexed};
                else if (m == ZYDIS_MNEMONIC_ADD && gpr(src) >= 0 && !regs[gpr(src)])
                    value = pointer{regs[target]->rva, true};
            }
        }

        for_each_written_gpr(d, [&](int r) { regs[r].reset(); });
        if (value)
            regs[target] = value;
    }

    // ---- Bit tests -----------------------------------------------------------

    constexpr int LOAD_TO_TEST_WINDOW = 8;

    // Is the byte just loaded into a register tested against a variable bit?
    // Accepts the shapes MSVC and clang-cl emit: TEST/AND with a shifted mask,
    // BT with a bit index register, SHR/SHRX by a variable count. Unrelated
    // instructions may sit in between and the byte may be copied first.
    bool feeds_bit_test(const ZydisDecoder& dec, const pe_view& pe, const decoded& load, std::uint32_t end)
    {
        std::uint32_t watched = 1u << gpr(load.ops[0]);
        const auto is_watched = [&](const ZydisDecodedOperand& op)
        {
            const int r = gpr(op);
            return r >= 0 && (watched >> r & 1);
        };

        decoded d{};
        auto rva = load.rva + load.insn.length;
        for (int n = 0; n < LOAD_TO_TEST_WINDOW && decode_at(dec, pe, rva, end, d); ++n, rva += d.insn.length)
        {
            const auto m = d.insn.mnemonic;
            if (ends_flow(m) || m == ZYDIS_MNEMONIC_CALL)
                return false;

            const auto& a = d.ops[0];
            const auto& b = d.ops[1];
            if (is_watched(a) || is_watched(b))
            {
                switch (m)
                {
                case ZYDIS_MNEMONIC_TEST:
                case ZYDIS_MNEMONIC_AND:
                case ZYDIS_MNEMONIC_OR:
                    if (a.type == ZYDIS_OPERAND_TYPE_REGISTER && b.type == ZYDIS_OPERAND_TYPE_REGISTER)
                        return is_watched(a) != is_watched(b); // against a different, variable mask
                    return b.type == ZYDIS_OPERAND_TYPE_MEMORY && b.mem.index != ZYDIS_REGISTER_NONE; // mask table
                case ZYDIS_MNEMONIC_BT:
                case ZYDIS_MNEMONIC_SHR:
                case ZYDIS_MNEMONIC_SAR:
                    return is_watched(a) && b.type == ZYDIS_OPERAND_TYPE_REGISTER; // variable bit index / count
                case ZYDIS_MNEMONIC_SHRX:
                case ZYDIS_MNEMONIC_SARX:
                    return is_watched(b); // dst, source, count register
                case ZYDIS_MNEMONIC_MOV:
                case ZYDIS_MNEMONIC_MOVZX:
                    if (gpr(a) >= 0 && is_watched(b))
                    {
                        watched |= 1u << gpr(a);
                        continue;
                    }
                    break;
                default:
                    break;
                }
            }

            for_each_written_gpr(d, [&](int r) { watched &= ~(1u << r); });
            if (!watched)
                return false;
        }
        return false;
    }

    // A one-byte read of a writable data table at a variable index into a
    // register. Returns the table RVA.
    std::optional<std::uint32_t> byte_load(const pe_view& pe, const decoded& d, const registers& regs)
    {
        const auto m = d.insn.mnemonic;
        if (m != ZYDIS_MNEMONIC_MOVZX && m != ZYDIS_MNEMONIC_MOVSX && m != ZYDIS_MNEMONIC_MOV)
            return std::nullopt;
        const auto& src = d.ops[1];
        if (d.insn.operand_count_visible < 2 || gpr(d.ops[0]) < 0 || src.type != ZYDIS_OPERAND_TYPE_MEMORY ||
            src.mem.type != ZYDIS_MEMOP_TYPE_MEM || src.size != 8)
            return std::nullopt;
        const auto table = address_of(src, regs);
        if (!table || !table->indexed || !in_ranges(pe.data, table->rva))
            return std::nullopt;
        return static_cast<std::uint32_t>(table->rva);
    }

    // Does this instruction read or update one byte of a writable data table at
    // a variable index, as a bit test? Returns the table RVA.
    std::optional<std::uint32_t> bit_table_access(const ZydisDecoder& dec, const pe_view& pe, const decoded& d, const registers& regs,
                                                  std::uint32_t end)
    {
        if (const auto table = byte_load(pe, d, regs))
            return feeds_bit_test(dec, pe, d, end) ? table : std::nullopt;
        if (d.insn.operand_count_visible < 2)
            return std::nullopt;

        int mem = -1;
        for (int i = 0; i < 2; ++i)
        {
            if (d.ops[i].type == ZYDIS_OPERAND_TYPE_MEMORY && d.ops[i].mem.type == ZYDIS_MEMOP_TYPE_MEM)
                mem = i;
        }
        if (mem < 0 || gpr(d.ops[1 - mem]) < 0) // the other operand must be a register: a constant is a fixed flag
            return std::nullopt;

        const auto table = address_of(d.ops[mem], regs);
        if (!table || !in_ranges(pe.data, table->rva))
            return std::nullopt;

        bool bit_test = false;
        switch (d.insn.mnemonic)
        {
        case ZYDIS_MNEMONIC_BT:
        case ZYDIS_MNEMONIC_BTS:
        case ZYDIS_MNEMONIC_BTR:
        case ZYDIS_MNEMONIC_BTC:
            bit_test = mem == 0; // bit string, the register is the bit index
            break;
        case ZYDIS_MNEMONIC_TEST:
        case ZYDIS_MNEMONIC_AND:
        case ZYDIS_MNEMONIC_OR:
        case ZYDIS_MNEMONIC_XOR:
            bit_test = d.ops[mem].size == 8 && table->indexed;
            break;
        default:
            break;
        }
        if (!bit_test)
            return std::nullopt;
        return static_cast<std::uint32_t>(table->rva);
    }

    // ---- Identification signals ------------------------------------------------

    constexpr std::int64_t SPEAR_OFFSET      = 20000; // spearfishing row ids start at 20000 (game logic)
    constexpr std::int64_t MAX_BITFIELD_BITS = 8 * 512;
    constexpr int RSP                        = 4;

    // Spearfishing evidence: code touching the spearfish bitfield converts
    // between row ids and bit indices with ±20000, or bounds its loops with
    // 20000 + bit count. Game logic, so every compiler keeps the constant.
    bool has_spear_constant(const decoded& d)
    {
        const auto& dst = d.ops[0];
        const auto& src = d.ops[1];
        switch (d.insn.mnemonic)
        {
        case ZYDIS_MNEMONIC_ADD:
        case ZYDIS_MNEMONIC_SUB:
            if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE && gpr(dst) != RSP)
            {
                const auto v = static_cast<std::int32_t>(src.imm.value.s);
                return v == SPEAR_OFFSET || v == -SPEAR_OFFSET;
            }
            return false;
        case ZYDIS_MNEMONIC_LEA:
            if (src.mem.base != ZYDIS_REGISTER_RIP && src.mem.disp.has_displacement)
            {
                const auto v = static_cast<std::int32_t>(src.mem.disp.value);
                return v == SPEAR_OFFSET || v == -SPEAR_OFFSET;
            }
            return false;
        case ZYDIS_MNEMONIC_CMP:
            if (src.type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
            {
                const auto bits = src.imm.value.s - SPEAR_OFFSET;
                return bits > 0 && bits <= MAX_BITFIELD_BITS && bits % 8 == 0;
            }
            return false;
        default:
            return false;
        }
    }

    // A loop bound for a log: the compared constant (or constant + 1, for <=),
    // relative to the log's first row id, is exactly the size of the log's block
    // in the Excel data. An exact match, not a range: a looser match lets other
    // loops claim the anchor when the data and the game disagree.
    bool is_loop_bound(std::int64_t compared, std::int64_t first_row_id, std::uint32_t block_bits)
    {
        const auto bits = compared - first_row_id;
        return block_bits != 0 && (bits == block_bits || bits + 1 == block_bits);
    }

    // ---- Scan ------------------------------------------------------------------

    using table_functions = std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>>; // table RVA -> functions

    struct scan_result
    {
        table_functions tables;                                  // bit tables and the functions bit-testing them
        table_functions loads;                                   // functions reading a byte at a variable index, tested or not
        std::unordered_set<std::uint32_t> spear_functions;       // functions with a spear constant
        std::unordered_set<std::uint32_t> fish_bound_functions;  // functions comparing against the fish block size
        std::unordered_set<std::uint32_t> spear_bound_functions; // functions comparing against 20000 + spear block size
    };

    void scan_region(const ZydisDecoder& dec, const pe_view& pe, const code_region& region, const mem::log_bits& needed, scan_result& out)
    {
        registers regs{};
        decoded d{};
        auto function = region.function;
        for (auto rva = region.begin_rva; rva < region.end_rva;)
        {
            if (!decode_at(dec, pe, rva, region.end_rva, d))
            {
                regs = {};
                ++rva;
                continue;
            }
            if (has_spear_constant(d))
                out.spear_functions.insert(function);
            if (d.insn.mnemonic == ZYDIS_MNEMONIC_CMP && d.ops[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
            {
                const auto compared = d.ops[1].imm.value.s;
                if (is_loop_bound(compared, 0, needed.fish))
                    out.fish_bound_functions.insert(function);
                if (is_loop_bound(compared, SPEAR_OFFSET, needed.spear))
                    out.spear_bound_functions.insert(function);
            }
            if (const auto table = bit_table_access(dec, pe, d, regs, region.end_rva))
                out.tables[*table].insert(function);
            if (const auto table = byte_load(pe, d, regs))
                out.loads[*table].insert(function);
            track(pe, d, regs);
            rva += d.insn.length;
            if (region.standalone && ends_flow(d.insn.mnemonic))
                function = rva;
        }
    }

    // ---- Identification ----------------------------------------------------------

    // Distance to the next detected table: an upper bound of a table's size.
    std::optional<std::uint32_t> room(const std::vector<std::uint32_t>& sorted_rvas, std::uint32_t rva)
    {
        const auto next = std::ranges::upper_bound(sorted_rvas, rva);
        if (next == sorted_rvas.end())
            return std::nullopt;
        return *next - rva;
    }

    // Both bitfields live in PlayerState, a few hundred bytes apart (141-210 from
    // 2021 through 7.5). Places the fish table when it has no anchor, and
    // rejects pairs that cannot both be right.
    constexpr std::uint32_t MAX_FISH_TO_SPEAR = 1024;

    bool neighbours(std::uint32_t a, std::uint32_t b) noexcept
    {
        return (a > b ? a - b : b - a) <= MAX_FISH_TO_SPEAR;
    }

    constexpr std::uint32_t MIN_FISHLOG_SIZE = 64; // bytes: 1024+ fish since 2021

    struct anchor
    {
        std::uint32_t rva;
        std::uint32_t bytes;
    };

    // The one bit table read by a function whose loop bound is the log's block
    // size. Any variable-index byte read counts here, tested nearby or not: a
    // compiler may spill the byte and test it after the calls in the loop body
    // (MSVC 14.52 /O2 does). A table the bitfield would not fit (the next table
    // starts sooner) is no candidate: round bounds such as 400h also bound
    // unrelated loops. More than one candidate is no anchor.
    std::optional<anchor> find_anchor(const scan_result& scan, const std::unordered_set<std::uint32_t>& bound_functions, std::uint32_t bits,
                                      const std::vector<std::uint32_t>& sorted_rvas, std::optional<std::uint32_t> exclude)
    {
        const auto bytes   = (bits + 7) / 8;
        const auto bounded = [&](std::uint32_t f) { return bound_functions.contains(f); };
        std::optional<anchor> found;
        for (const auto& [rva, functions] : scan.tables)
        {
            const auto loads = scan.loads.find(rva);
            if (rva == exclude || (std::ranges::none_of(functions, bounded) && (loads == scan.loads.end() || std::ranges::none_of(loads->second, bounded))))
                continue;
            if (const auto r = room(sorted_rvas, rva); r && bytes > *r)
                continue;
            if (found)
                return std::nullopt;
            found = anchor{rva, bytes};
        }
        return found;
    }

    // Fallback for the spear table: the table accessed by the most functions
    // holding a spearfishing constant. A function touching both bitfields (say, a
    // setter branching on id < 20000) counts for both, so the dedicated spearfish
    // code decides. Equal counts go to the table whose accessors carry the
    // constant more consistently; a remaining tie is rejected rather than guessed.
    std::optional<std::uint32_t> spear_by_constants(const scan_result& scan, std::optional<std::uint32_t> exclude)
    {
        std::optional<std::uint32_t> spear_rva;
        std::size_t best_count = 0;
        std::size_t best_total = 0;
        bool tie               = false;
        for (const auto& [rva, functions] : scan.tables)
        {
            if (rva == exclude)
                continue;
            const auto count = static_cast<std::size_t>(std::ranges::count_if(functions, [&](std::uint32_t f) { return scan.spear_functions.contains(f); }));
            if (count == 0)
                continue;
            if (!spear_rva || count > best_count || (count == best_count && functions.size() < best_total))
            {
                spear_rva  = rva;
                best_count = count;
                best_total = functions.size();
                tie        = false;
            }
            else if (count == best_count && functions.size() == best_total)
                tie = true;
        }
        if (tie)
            return std::nullopt;
        return spear_rva;
    }

    // Fallback for the fish table: PlayerState keeps the fish bitfield right
    // before the spearfish one; only the caught counters and the small
    // spearfishing notebook bitfield sit in between (141-210 bytes apart from 2021
    // through 7.5). Take the nearest preceding table that is big enough: the
    // tables further up are followed by fields that are not bit tables, so their
    // distance overstates their size.
    std::optional<std::uint32_t> fish_before(const std::vector<std::uint32_t>& sorted_rvas, std::uint32_t spear_rva)
    {
        for (auto it = std::ranges::lower_bound(sorted_rvas, spear_rva); it != sorted_rvas.begin();)
        {
            const auto rva = *--it;
            if (spear_rva - rva > MAX_FISH_TO_SPEAR)
                break;
            if (*room(sorted_rvas, rva) >= MIN_FISHLOG_SIZE) // the spear table follows, so the room is known
                return rva;
        }
        return std::nullopt;
    }
}

std::optional<mem::fishlog_globals> mem::find_fishlog_globals(const process& proc, const log_bits& needed)
{
    pe_view pe{};
    if (!parse_pe(proc.image_data(), proc.image_size(), pe))
        return std::nullopt;

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
        return std::nullopt;

    scan_result scan;
    for (const auto& region : code_regions(pe))
        scan_region(decoder, pe, region, needed, scan);
    if (scan.tables.size() < 2)
        return std::nullopt;

    std::vector<std::uint32_t> sorted_rvas;
    sorted_rvas.reserve(scan.tables.size());
    for (const auto& rva : scan.tables | std::views::keys)
        sorted_rvas.push_back(rva);
    std::ranges::sort(sorted_rvas);

    // Anchors first; structure only for a log without one. A fish block under
    // 64 bytes is not the fish log (1024+ fish since 2021), so it anchors nothing.
    auto spear_anchor = find_anchor(scan, scan.spear_bound_functions, needed.spear, sorted_rvas, std::nullopt);
    auto fish_anchor  = needed.fish >= 8 * MIN_FISHLOG_SIZE
                          ? find_anchor(scan, scan.fish_bound_functions, needed.fish, sorted_rvas,
                                        spear_anchor ? std::optional{spear_anchor->rva} : std::nullopt)
                          : std::nullopt;

    // Two anchors that are not PlayerState neighbours cannot both be right: other
    // loops can share a bound with one of the logs. Trust neither.
    if (spear_anchor && fish_anchor && !neighbours(spear_anchor->rva, fish_anchor->rva))
    {
        spear_anchor.reset();
        fish_anchor.reset();
    }

    const auto spear_rva = spear_anchor ? std::optional{spear_anchor->rva}
                                        : spear_by_constants(scan, fish_anchor ? std::optional{fish_anchor->rva} : std::nullopt);
    if (!spear_rva)
        return std::nullopt;
    const auto fish_rva = fish_anchor ? std::optional{fish_anchor->rva} : fish_before(sorted_rvas, *spear_rva);
    if (!fish_rva || *fish_rva == *spear_rva || !neighbours(*fish_rva, *spear_rva))
        return std::nullopt;

    const auto make = [&](std::uint32_t rva, const std::optional<anchor>& found)
    {
        return bitfield{
            .address  = proc.base_address() + rva,
            .size     = found ? std::optional{found->bytes} : room(sorted_rvas, rva),
            .anchored = found.has_value(),
        };
    };
    return fishlog_globals{.fish = make(*fish_rva, fish_anchor), .spear = make(*spear_rva, spear_anchor)};
}
