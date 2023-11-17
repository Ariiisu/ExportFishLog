#include "../include/xivres/sound.h"

#include <ranges>

#include "../include/xivres/common.h"
#include "../include/xivres/util.span_cast.h"

const uint8_t xivres::sound::sound_entry_ogg_header::Version3XorTable[256] = {
	0x3A, 0x32, 0x32, 0x32, 0x03, 0x7E, 0x12, 0xF7, 0xB2, 0xE2, 0xA2, 0x67, 0x32, 0x32, 0x22, 0x32,
	0x32, 0x52, 0x16, 0x1B, 0x3C, 0xA1, 0x54, 0x7B, 0x1B, 0x97, 0xA6, 0x93, 0x1A, 0x4B, 0xAA, 0xA6,
	0x7A, 0x7B, 0x1B, 0x97, 0xA6, 0xF7, 0x02, 0xBB, 0xAA, 0xA6, 0xBB, 0xF7, 0x2A, 0x51, 0xBE, 0x03,
	0xF4, 0x2A, 0x51, 0xBE, 0x03, 0xF4, 0x2A, 0x51, 0xBE, 0x12, 0x06, 0x56, 0x27, 0x32, 0x32, 0x36,
	0x32, 0xB2, 0x1A, 0x3B, 0xBC, 0x91, 0xD4, 0x7B, 0x58, 0xFC, 0x0B, 0x55, 0x2A, 0x15, 0xBC, 0x40,
	0x92, 0x0B, 0x5B, 0x7C, 0x0A, 0x95, 0x12, 0x35, 0xB8, 0x63, 0xD2, 0x0B, 0x3B, 0xF0, 0xC7, 0x14,
	0x51, 0x5C, 0x94, 0x86, 0x94, 0x59, 0x5C, 0xFC, 0x1B, 0x17, 0x3A, 0x3F, 0x6B, 0x37, 0x32, 0x32,
	0x30, 0x32, 0x72, 0x7A, 0x13, 0xB7, 0x26, 0x60, 0x7A, 0x13, 0xB7, 0x26, 0x50, 0xBA, 0x13, 0xB4,
	0x2A, 0x50, 0xBA, 0x13, 0xB5, 0x2E, 0x40, 0xFA, 0x13, 0x95, 0xAE, 0x40, 0x38, 0x18, 0x9A, 0x92,
	0xB0, 0x38, 0x00, 0xFA, 0x12, 0xB1, 0x7E, 0x00, 0xDB, 0x96, 0xA1, 0x7C, 0x08, 0xDB, 0x9A, 0x91,
	0xBC, 0x08, 0xD8, 0x1A, 0x86, 0xE2, 0x70, 0x39, 0x1F, 0x86, 0xE0, 0x78, 0x7E, 0x03, 0xE7, 0x64,
	0x51, 0x9C, 0x8F, 0x34, 0x6F, 0x4E, 0x41, 0xFC, 0x0B, 0xD5, 0xAE, 0x41, 0xFC, 0x0B, 0xD5, 0xAE,
	0x41, 0xFC, 0x3B, 0x70, 0x71, 0x64, 0x33, 0x32, 0x12, 0x32, 0x32, 0x36, 0x70, 0x34, 0x2B, 0x56,
	0x22, 0x70, 0x3A, 0x13, 0xB7, 0x26, 0x60, 0xBA, 0x1B, 0x94, 0xAA, 0x40, 0x38, 0x00, 0xFA, 0xB2,
	0xE2, 0xA2, 0x67, 0x32, 0x32, 0x12, 0x32, 0xB2, 0x32, 0x32, 0x32, 0x32, 0x75, 0xA3, 0x26, 0x7B,
	0x83, 0x26, 0xF9, 0x83, 0x2E, 0xFF, 0xE3, 0x16, 0x7D, 0xC0, 0x1E, 0x63, 0x21, 0x07, 0xE3, 0x01,
};

std::vector<uint8_t> xivres::sound::reader::read_entry(const std::span<const uint32_t>& offsets, uint32_t endOffset, size_t index) const {
	if (!offsets[index])
		return {};
	const auto next = index == offsets.size() - 1 || offsets[index + 1] == 0 ? endOffset : offsets[index + 1];
	return m_stream->read_vector<uint8_t>(offsets[index], size_t() + next - offsets[index]);
}

std::vector<std::vector<uint8_t>> xivres::sound::reader::read_table(const std::span<const uint32_t>& offsets, uint32_t endOffset) const {
	std::vector<std::vector<uint8_t>> res;
	for (uint32_t i = 0; i < offsets.size(); ++i)
		res.emplace_back(read_entry(offsets, endOffset, i));
	return res;
}

std::vector<uint8_t> xivres::sound::reader::get_header_bytes(const stream& strm) {
	constexpr auto InitialBufferSize = 8192ULL;
	std::vector<uint8_t> res;
	res.resize(static_cast<size_t>((std::min<uint64_t>)(InitialBufferSize, strm.size())));
	strm.read_fully(0, std::span(res));

	const auto& header = *reinterpret_cast<sound::header*>(&res[0]);
	if (header.HeaderSize != sizeof header)
		throw std::invalid_argument("invalid HeaderSize");

	const auto& offsets = *reinterpret_cast<sound::offsets*>(&res[header.HeaderSize]);
	res.resize(static_cast<size_t>(0) + offsets.Table5Offset + 16);
	if (res.size() > InitialBufferSize)
		strm.read_fully(InitialBufferSize, std::span(res).subspan(InitialBufferSize));
	return res;
}

xivres::sound::reader::reader(std::shared_ptr<stream> strm)
	: m_stream(std::move(strm))
	, m_headerBuffer(get_header_bytes(*m_stream))
	, m_header(*reinterpret_cast<const header*>(&m_headerBuffer[0]))
	, m_offsets(*reinterpret_cast<const offsets*>(&m_headerBuffer[m_header.HeaderSize]))
	, m_offsetsTable1(util::span_cast<uint32_t>(m_headerBuffer, m_header.HeaderSize + sizeof m_offsets, m_offsets.Table1And4EntryCount))
	, m_offsetsTable2(util::span_cast<uint32_t>(m_headerBuffer, m_offsets.Table2Offset, m_offsets.Table2EntryCount))
	, m_soundEntryOffsets(util::span_cast<uint32_t>(m_headerBuffer, m_offsets.SoundEntryOffset, m_offsets.SoundEntryCount))
	, m_offsetsTable4(util::span_cast<uint32_t>(m_headerBuffer, m_offsets.Table4Offset, m_offsets.Table1And4EntryCount))
	, m_offsetsTable5(util::span_cast<uint32_t>(m_headerBuffer, m_offsets.Table5Offset, (m_headerBuffer.size() - m_offsets.Table5Offset), 1))
	, m_endOfSoundEntries(m_header.FileSize)
	, m_endOfTable5(!m_soundEntryOffsets.empty() && m_soundEntryOffsets.front() ? m_soundEntryOffsets.front() : m_endOfSoundEntries)
	, m_endOfTable2(!m_offsetsTable5.empty() && m_offsetsTable5.front() ? m_offsetsTable5.front() : m_endOfTable5)
	, m_endOfTable1(!m_offsetsTable2.empty() && m_offsetsTable2.front() ? m_offsetsTable2.front() : m_endOfTable2)
	, m_endOfTable4(!m_offsetsTable1.empty() && m_offsetsTable1.front() ? m_offsetsTable1.front() : m_endOfTable1) {
}

std::vector<uint32_t> xivres::sound::reader::sound_item::marked_sample_block_indices() const {
	std::vector<uint32_t> res;
	for (const auto& chunk : AuxChunks) {
		if (memcmp(chunk->Name, sound_entry_aux_chunk::Name_Mark, sizeof chunk->Name) != 0)
			continue;

		res.reserve(*chunk->Data.Mark.Count);
		for (uint32_t i = 0, i_ = *chunk->Data.Mark.Count; i < i_; i++)
			res.emplace_back(*chunk->Data.Mark.SampleBlockIndices[i]);
	}
	return res;
}

const xivres::sound::wave_format_ex& xivres::sound::reader::sound_item::get_wav_header() const {
	const auto& header = *reinterpret_cast<const wave_format_ex*>(&ExtraData[0]);
	if (sizeof header + header.cbSize != ExtraData.size_bytes())
		throw std::invalid_argument("invalid OggSeekTableHeader size");
	return header;
}

const xivres::sound::adpcm_wave_format& xivres::sound::reader::sound_item::get_adpcm_wav_header() const {
	if (Header->Format != sound_entry_format::WaveFormatAdpcm)
		throw std::invalid_argument("Not MS-ADPCM");
	if (ExtraData.size_bytes() < sizeof sound_entry_ogg_header)
		throw std::invalid_argument("ExtraData too small to fit MsAdpcmHeader");
	return *reinterpret_cast<const adpcm_wave_format*>(&get_wav_header());
}

std::vector<uint8_t> xivres::sound::reader::sound_item::get_wav_file() const {
	const auto& hdr = get_wav_header();
	const auto headerSpan = ExtraData.subspan(0, sizeof hdr + hdr.cbSize);
	std::vector<uint8_t> res;
	const auto insert = [&res](const auto& v) {
		res.insert(res.end(), reinterpret_cast<const uint8_t*>(&v), reinterpret_cast<const uint8_t*>(&v) + sizeof v);
	};
	const auto totalLength = static_cast<uint32_t>(size_t()
		+ 12 // "RIFF"####"WAVE"
		+ 8 + headerSpan.size() // "fmt "####<header>
		+ 8 + Data.size() // "data"####<data>
	);
	res.reserve(totalLength);
	insert(LE<uint32_t>(0x46464952U)); // "RIFF"
	insert(LE<uint32_t>(totalLength - 8));
	insert(LE<uint32_t>(0x45564157U)); // "WAVE"
	insert(LE<uint32_t>(0x20746D66U)); // "fmt "
	insert(LE<uint32_t>(static_cast<uint32_t>(headerSpan.size())));
	res.insert(res.end(), headerSpan.begin(), headerSpan.end());
	insert(LE<uint32_t>(0x61746164U)); // "data"
	insert(LE<uint32_t>(static_cast<uint32_t>(Data.size())));
	res.insert(res.end(), Data.begin(), Data.end());
	return res;
}

const xivres::sound::sound_entry_ogg_header& xivres::sound::reader::sound_item::get_ogg_seek_table_header() const {
	if (Header->Format != sound_entry_format::Ogg)
		throw std::invalid_argument("Not ogg");
	if (ExtraData.size_bytes() < sizeof sound_entry_ogg_header)
		throw std::invalid_argument("ExtraData too small to fit OggSeekTableHeader");
	const auto& header = *reinterpret_cast<sound_entry_ogg_header*>(&ExtraData[0]);
	if (header.HeaderSize != sizeof header)
		throw std::invalid_argument("invalid OggSeekTableHeader size");
	return header;
}

std::span<const uint32_t> xivres::sound::reader::sound_item::get_ogg_seek_table() const {
	const auto& tbl = get_ogg_seek_table_header();
	const auto span = ExtraData.subspan(tbl.HeaderSize, tbl.SeekTableSize);
	return util::span_cast<uint32_t>(span);
}

std::vector<uint8_t> xivres::sound::reader::sound_item::get_ogg_file() const {
	const auto& tbl = get_ogg_seek_table_header();
	const auto header = ExtraData.subspan(size_t() + tbl.HeaderSize + tbl.SeekTableSize, tbl.VorbisHeaderSize);
	std::vector<uint8_t> res;
	res.reserve(header.size() + Data.size());
	res.insert(res.end(), header.begin(), header.end());
	res.insert(res.end(), Data.begin(), Data.end());

	if (tbl.Version == 0x2) {
		if (tbl.EncodeByte) {
			for (auto& c : std::span(res).subspan(0, header.size()))
				c ^= tbl.EncodeByte;
		}
	} else if (tbl.Version == 0x3) {
		const auto byte1 = static_cast<uint8_t>(Data.size() & 0x7F);
		const auto byte2 = static_cast<uint8_t>(Data.size() & 0x3F);
		for (size_t i = 0; i < res.size(); i++)
			res[i] ^= sound_entry_ogg_header::Version3XorTable[(byte2 + i) & 0xFF] ^ byte1;
	} else {
		throw bad_data_error(std::format("Unsupported scd ogg header version: {}", tbl.Version));
	}
	return res;
}

xivres::sound::reader::sound_item xivres::sound::reader::read_sound_item(size_t entryIndex) const {
	if (entryIndex >= m_soundEntryOffsets.size())
		throw std::out_of_range("entry index >= sound entry count");

	sound_item res{};
	res.Buffer = read_entry(m_soundEntryOffsets, m_endOfSoundEntries, static_cast<uint32_t>(entryIndex));
	res.Header = reinterpret_cast<sound_entry_header*>(&res.Buffer[0]);
	
	if (const auto minSize = sizeof *res.Header + res.Header->StreamOffset + res.Header->StreamSize; res.Buffer.size() < minSize) {
		res.Buffer.resize(minSize);	
		res.Header = reinterpret_cast<sound_entry_header*>(&res.Buffer[0]);
	}
	auto pos = sizeof *res.Header;
	for (size_t i = 0; i < res.Header->AuxChunkCount; ++i) {
		res.AuxChunks.emplace_back(reinterpret_cast<sound_entry_aux_chunk*>(&res.Buffer[pos]));
		pos += res.AuxChunks.back()->ChunkSize;
	}
	res.ExtraData = std::span(res.Buffer).subspan(pos, res.Header->StreamOffset + sizeof *res.Header - pos);
	res.Data = std::span(res.Buffer).subspan(sizeof *res.Header + res.Header->StreamOffset, res.Header->StreamSize);
	return res;
}

void xivres::sound::writer::sound_item::export_to(std::vector<uint8_t>& res) const {
	const auto insert = [&res](const auto& v) {
		res.insert(res.end(), reinterpret_cast<const uint8_t*>(&v), reinterpret_cast<const uint8_t*>(&v) + sizeof v);
	};

	const auto entrySize = calculate_entry_size();
	auto hdr = Header;
	hdr.StreamOffset = static_cast<uint32_t>(entrySize - Data.size() - sizeof hdr);
	hdr.StreamSize = static_cast<uint32_t>(Data.size());
	hdr.AuxChunkCount = static_cast<uint16_t>(AuxChunks.size());

	res.reserve(res.size() + entrySize);
	insert(hdr);
	for (const auto& [name, aux] : AuxChunks) {
		if (name.size() != 4)
			throw std::invalid_argument(std::format("Length of name must be 4, got \"{}\"({})", name, name.size()));
		res.insert(res.end(), name.begin(), name.end());
		insert(static_cast<uint32_t>(8 + aux.size()));
		res.insert(res.end(), aux.begin(), aux.end());
	}
	res.insert(res.end(), ExtraData.begin(), ExtraData.end());
	res.insert(res.end(), Data.begin(), Data.end());
}

void xivres::sound::writer::sound_item::set_mark_chunks(uint32_t loopStartSampleBlockIndex, uint32_t loopEndSampleBlockIndex, std::span<const uint32_t> marks) {
	auto& buf = AuxChunks[std::string(sound_entry_aux_chunk::Name_Mark, sizeof sound_entry_aux_chunk::Name_Mark)];
	buf.resize(12 + marks.size_bytes());
	auto& markHeader = *reinterpret_cast<sound_entry_aux_chunk::mark_chunk_data*>(&buf[0]);
	markHeader.LoopStartSampleBlockIndex = loopStartSampleBlockIndex;
	markHeader.LoopEndSampleBlockIndex = loopEndSampleBlockIndex;
	markHeader.Count = static_cast<uint32_t>(marks.size());
	if (!marks.empty())
		memcpy(&buf[12], &marks[0], marks.size_bytes());
}

xivres::sound::writer::sound_item xivres::sound::writer::sound_item::make_from_reader_sound_item(const reader::sound_item& item) {
	sound_item result{};
	result.Header = *item.Header;
	for (const auto& aux : item.AuxChunks) {
		const auto dataSpan = aux->data_span();
		result.AuxChunks.emplace(aux->Name, std::vector(dataSpan.begin(), dataSpan.end()));
	}
	result.Data.insert(result.Data.begin(), item.Data.begin(), item.Data.end());
	result.ExtraData.insert(result.ExtraData.begin(), item.ExtraData.begin(), item.ExtraData.end());
	return result;
}

size_t xivres::sound::writer::sound_item::calculate_entry_size() const {
	size_t auxLength = 0;
	for (const auto& aux : AuxChunks | std::views::values)
		auxLength += 8 + aux.size();

	return sizeof sound_entry_header + auxLength + ExtraData.size() + Data.size();
}

xivres::sound::writer::sound_item xivres::sound::writer::sound_item::make_empty(std::optional<std::chrono::milliseconds> duration) {
	if (!duration) {
		return {
			.Header = {
				.Format = sound_entry_format::Empty,
			},
		};
	}

	const auto blankLength = static_cast<uint32_t>(duration->count() * 2 * 44100 / 1000);
	return {
		.Header = {
			.StreamSize = blankLength,
			.ChannelCount = 1,
			.SamplingRate = 44100,
			.Format = sound_entry_format::WaveFormatPcm,
		},
		.Data = std::vector<uint8_t>(blankLength),
	};
}

xivres::sound::writer::sound_item xivres::sound::writer::sound_item::make_from_ogg(
	const std::vector<uint8_t>& headerPages,
	std::vector<uint8_t> dataPages,
	uint32_t channels,
	uint32_t samplingRate,
	uint32_t loopStartOffset,
	uint32_t loopEndOffset,
	std::span<uint32_t> seekTable
) {
	std::vector<uint8_t> oggHeaderBytes;
	oggHeaderBytes.reserve(sizeof sound_entry_ogg_header + std::span(seekTable).size_bytes() + headerPages.size());
	oggHeaderBytes.resize(sizeof sound_entry_ogg_header);
	const auto seekTableSpan = util::span_cast<uint8_t>(seekTable);
	oggHeaderBytes.insert(oggHeaderBytes.end(), seekTableSpan.begin(), seekTableSpan.end());
	oggHeaderBytes.insert(oggHeaderBytes.end(), headerPages.begin(), headerPages.end());
	auto& oggHeader = *reinterpret_cast<sound_entry_ogg_header*>(&oggHeaderBytes[0]);
	oggHeader.Version = 0x02;
	oggHeader.HeaderSize = 0x20;
	oggHeader.SeekTableSize = static_cast<uint32_t>(seekTableSpan.size_bytes());
	oggHeader.VorbisHeaderSize = static_cast<uint32_t>(headerPages.size());
	return sound_item{
		.Header = {
			.StreamSize = static_cast<uint32_t>(dataPages.size()),
			.ChannelCount = channels,
			.SamplingRate = samplingRate,
			.Format = sound_entry_format::Ogg,
			.LoopStartOffset = loopStartOffset,
			.LoopEndOffset = loopEndOffset,
			.StreamOffset = static_cast<uint32_t>(oggHeaderBytes.size()),
			.Unknown_0x02E = 0,
		},
		.ExtraData = std::move(oggHeaderBytes),
		.Data = std::move(dataPages),
	};
}

xivres::sound::writer::sound_item xivres::sound::writer::sound_item::make_from_wave(const linear_reader<uint8_t>& reader) {
	struct expected_format {
		LE<uint32_t> Riff;
		LE<uint32_t> RemainingSize;
		LE<uint32_t> Wave;
		LE<uint32_t> fmt_;
		LE<uint32_t> WaveFormatExSize;
	};
	const auto hdr = *reinterpret_cast<const expected_format*>(reader(sizeof expected_format, true).data());
	if (hdr.Riff != 0x46464952U || hdr.Wave != 0x45564157U || hdr.fmt_ != 0x20746D66U)
		throw std::invalid_argument("Bad file header");

	std::vector<uint8_t> wfbuf;
	{
		auto r = reader(hdr.WaveFormatExSize, true);
		wfbuf.insert(wfbuf.end(), r.begin(), r.end());
	}
	auto& wfex = *reinterpret_cast<wave_format_ex*>(&wfbuf[0]);

	auto pos = sizeof hdr + hdr.WaveFormatExSize;
	while (pos - 8 < hdr.RemainingSize) {
		struct CodeAndLen {
			LE<uint32_t> Code;
			LE<uint32_t> Len;
		};
		const auto sectionHdr = *reinterpret_cast<const CodeAndLen*>(reader(sizeof CodeAndLen, true).data());
		pos += sizeof sectionHdr;
		const auto sectionData = reader(sectionHdr.Len, true);
		if (sectionHdr.Code == 0x61746164U) {
			// "data"

			auto res = sound_item{
				.Header = {
					.StreamSize = sectionHdr.Len,
					.ChannelCount = wfex.nChannels,
					.SamplingRate = wfex.nSamplesPerSec,
					.Unknown_0x02E = 0,
				},
				.Data = {sectionData.begin(), sectionData.end()},
			};

			switch (wfex.wFormatTag) {
				case wave_format_tag::Pcm:
					res.Header.Format = sound_entry_format::WaveFormatPcm;
					break;
				case wave_format_tag::Adpcm:
					res.Header.Format = sound_entry_format::WaveFormatAdpcm;
					res.Header.StreamOffset = static_cast<uint32_t>(wfbuf.size());
					res.ExtraData = std::move(wfbuf);
					break;
				default:
					throw std::invalid_argument("wave format not supported");
			}

			return res;
		}
		pos += sectionHdr.Len;
	}
	throw std::invalid_argument("No data section found");
}

const xivres::sound::wave_format_ex& xivres::sound::writer::sound_item::as_wave_format_ex() const {
	return *reinterpret_cast<const wave_format_ex*>(&ExtraData[0]);
}

xivres::sound::wave_format_ex& xivres::sound::writer::sound_item::as_wave_format_ex() {
	return *reinterpret_cast<wave_format_ex*>(&ExtraData[0]);
}

std::vector<uint8_t> xivres::sound::writer::export_to_bytes() const {
	if (m_table1.size() != m_table4.size())
		throw std::invalid_argument("table1.size != table4.size");

	const auto table1OffsetsOffset = sizeof header + sizeof offsets;
	const auto table2OffsetsOffset = xivres::align<size_t>(table1OffsetsOffset + sizeof uint32_t * (1 + m_table1.size()), 0x10).Alloc;
	const auto soundEntryOffsetsOffset = xivres::align<size_t>(table2OffsetsOffset + sizeof uint32_t * (1 + m_table2.size()), 0x10).Alloc;
	const auto table4OffsetsOffset = xivres::align<size_t>(soundEntryOffsetsOffset + sizeof uint32_t * (1 + m_soundEntries.size()), 0x10).Alloc;
	const auto table5OffsetsOffset = xivres::align<size_t>(table4OffsetsOffset + sizeof uint32_t * (1 + m_table4.size()), 0x10).Alloc;

	std::vector<uint8_t> res;
	size_t requiredSize = table5OffsetsOffset + sizeof uint32_t * 4;
	for (const auto& item : m_table4)
		requiredSize += item.size();
	for (const auto& item : m_table1)
		requiredSize += item.size();
	for (const auto& item : m_table2)
		requiredSize += item.size();
	for (const auto& item : m_table5)
		requiredSize += item.size();
	for (const auto& item : m_soundEntries)
		requiredSize += item.calculate_entry_size();
	requiredSize = xivres::align<size_t>(requiredSize, 0x10).Alloc;
	res.reserve(requiredSize);

	res.resize(table5OffsetsOffset + sizeof uint32_t * 4);

	for (size_t i = 0; i < m_table4.size(); ++i) {
		reinterpret_cast<uint32_t*>(&res[table4OffsetsOffset])[i] = static_cast<uint32_t>(res.size());
		res.insert(res.end(), m_table4[i].begin(), m_table4[i].end());
	}
	for (size_t i = 0; i < m_table1.size(); ++i) {
		reinterpret_cast<uint32_t*>(&res[table1OffsetsOffset])[i] = static_cast<uint32_t>(res.size());
		res.insert(res.end(), m_table1[i].begin(), m_table1[i].end());
	}
	for (size_t i = 0; i < m_table2.size(); ++i) {
		reinterpret_cast<uint32_t*>(&res[table2OffsetsOffset])[i] = static_cast<uint32_t>(res.size());
		res.insert(res.end(), m_table2[i].begin(), m_table2[i].end());
	}
	for (size_t i = 0; i < m_table5.size() && i < 3; ++i) {
		if (m_table5[i].empty())
			break;
		reinterpret_cast<uint32_t*>(&res[table5OffsetsOffset])[i] = static_cast<uint32_t>(res.size());
		res.insert(res.end(), m_table5[i].begin(), m_table5[i].end());
	}
	for (size_t i = 0; i < m_soundEntries.size(); ++i) {
		reinterpret_cast<uint32_t*>(&res[soundEntryOffsetsOffset])[i] = static_cast<uint32_t>(res.size());
		m_soundEntries[i].export_to(res);
	}

	*reinterpret_cast<header*>(&res[0]) = {
		.SedbVersion = header::SedbVersion_FFXIV,
		.EndianFlag = endianness::LittleEndian,
		.SscfVersion = header::SscfVersion_FFXIV,
		.HeaderSize = sizeof header,
		.FileSize = static_cast<uint32_t>(requiredSize),
	};
	memcpy(reinterpret_cast<header*>(&res[0])->SedbSignature,
			header::SedbSignature_Value,
			sizeof header::SedbSignature_Value);
	memcpy(reinterpret_cast<header*>(&res[0])->SscfSignature,
			header::SscfSignature_Value,
			sizeof header::SscfSignature_Value);

	*reinterpret_cast<offsets*>(&res[sizeof header]) = {
		.Table1And4EntryCount = static_cast<uint16_t>(m_table1.size()),
		.Table2EntryCount = static_cast<uint16_t>(m_table2.size()),
		.SoundEntryCount = static_cast<uint16_t>(m_soundEntries.size()),
		.Unknown_0x006 = 0, // ?
		.Table2Offset = static_cast<uint32_t>(table2OffsetsOffset),
		.SoundEntryOffset = static_cast<uint32_t>(soundEntryOffsetsOffset),
		.Table4Offset = static_cast<uint32_t>(table4OffsetsOffset),
		.Table5Offset = static_cast<uint32_t>(table5OffsetsOffset),
		.Unknown_0x01C = 0, // ?
	};

	res.resize(requiredSize);

	return res;
}
