#ifndef XIVRES_SCD_H_
#define XIVRES_SCD_H_

#include <chrono>
#include <map>
#include <vector>

#include "stream.h"
#include "util.byte_order.h"

namespace xivres::sound {
	enum class endianness : uint8_t {
		LittleEndian = 0,
		BigEndian = 1,
	};

	struct header {
		static constexpr char SedbSignature_Value[4]{ 'S', 'E', 'D', 'B' };
		static constexpr char SscfSignature_Value[4]{ 'S', 'S', 'C', 'F' };
		static constexpr uint32_t SedbVersion_FFXIV = 3;
		static constexpr uint16_t SscfVersion_FFXIV = 4;

		char SedbSignature[4]{};
		char SscfSignature[4]{};
		LE<uint32_t> SedbVersion;
		endianness EndianFlag{};
		uint8_t SscfVersion{};
		LE<uint16_t> HeaderSize;
		LE<uint32_t> FileSize;
		uint8_t Padding_0x014[0x1C]{};
	};

	static_assert(sizeof header == 0x30);

	struct offsets {
		LE<uint16_t> Table1And4EntryCount;
		LE<uint16_t> Table2EntryCount;
		LE<uint16_t> SoundEntryCount;
		LE<uint16_t> Unknown_0x006;
		LE<uint32_t> Table2Offset;
		LE<uint32_t> SoundEntryOffset;
		LE<uint32_t> Table4Offset;
		LE<uint32_t> Padding_0x014;
		LE<uint32_t> Table5Offset;
		LE<uint32_t> Unknown_0x01C;
	};

	enum class sound_entry_format : uint32_t {
		WaveFormatPcm = 0x01,
		Ogg = 0x06,
		WaveFormatAdpcm = 0x0C,
		Empty = 0xFFFFFFFF,
	};

	struct sound_entry_header {
		LE<uint32_t> StreamSize;
		LE<uint32_t> ChannelCount;
		LE<uint32_t> SamplingRate;
		LE<sound_entry_format> Format;
		LE<uint32_t> LoopStartOffset;
		LE<uint32_t> LoopEndOffset;
		LE<uint32_t> StreamOffset;
		LE<uint16_t> AuxChunkCount;
		LE<uint16_t> Unknown_0x02E;
	};

	static_assert(sizeof sound_entry_header == 0x20);

	struct sound_entry_aux_chunk {
		static constexpr char Name_Mark[4]{ 'M', 'A', 'R', 'K' };

		struct mark_chunk_data {
			LE<uint32_t> LoopStartSampleBlockIndex;
			LE<uint32_t> LoopEndSampleBlockIndex;
			LE<uint32_t> Count;
			LE<uint32_t> SampleBlockIndices[1];
		};

		union aux_chunk_data {
			mark_chunk_data Mark;
		};

		char Name[4]{};
		LE<uint32_t> ChunkSize;
		aux_chunk_data Data;

		[[nodiscard]] std::span<const uint8_t> data_span() const {
			return util::span_cast<const uint8_t>(ChunkSize, &Data);
		}
	};

	struct sound_entry_ogg_header {
		static const uint8_t Version3XorTable[256];

		uint8_t Version{};
		uint8_t HeaderSize{};
		uint8_t EncodeByte{};
		uint8_t Padding_0x003{};
		LE<uint32_t> Unknown_0x004;
		LE<uint32_t> Unknown_0x008;
		LE<uint32_t> Unknown_0x00C;
		LE<uint32_t> SeekTableSize;
		LE<uint32_t> VorbisHeaderSize;
		uint32_t Unknown_0x018{};
		uint8_t Padding_0x01C[4]{};
	};

	enum class wave_format_tag : uint16_t {
		Pcm = 1,
		Adpcm = 2,
	};

#pragma pack(push, 1)
	struct wave_format_ex {
		wave_format_tag wFormatTag;
		uint16_t nChannels;
		uint32_t nSamplesPerSec;
		uint32_t nAvgBytesPerSec;
		uint16_t nBlockAlign;
		uint16_t wBitsPerSample;
		uint16_t cbSize;
	};

	struct adpcm_coef_set {
		short iCoef1;
		short iCoef2;
	};

	struct adpcm_wave_format {
		wave_format_ex wfx;
		short wSamplesPerBlock;
		short wNumCoef;
		adpcm_coef_set aCoef[32];
	};
#pragma pack(pop)

	class reader {
		const std::shared_ptr<stream> m_stream;
		const std::vector<uint8_t> m_headerBuffer;
		const header& m_header;
		const offsets& m_offsets;

		const std::span<const uint32_t> m_offsetsTable1;
		const std::span<const uint32_t> m_offsetsTable2;
		const std::span<const uint32_t> m_soundEntryOffsets;
		const std::span<const uint32_t> m_offsetsTable4;
		const std::span<const uint32_t> m_offsetsTable5;

		const uint32_t m_endOfSoundEntries;
		const uint32_t m_endOfTable5;
		const uint32_t m_endOfTable2;
		const uint32_t m_endOfTable1;
		const uint32_t m_endOfTable4;

		[[nodiscard]] std::vector<uint8_t> read_entry(const std::span<const uint32_t>& offsets, uint32_t endOffset, size_t index) const;

		[[nodiscard]] std::vector<std::vector<uint8_t>> read_table(const std::span<const uint32_t>& offsets, uint32_t endOffset) const;

		[[nodiscard]] static std::vector<uint8_t> get_header_bytes(const stream& strm);

	public:
		reader(std::shared_ptr<stream> strm);

		struct sound_item {
			std::vector<uint8_t> Buffer;
			sound_entry_header* Header;
			std::vector<sound_entry_aux_chunk*> AuxChunks;
			std::span<uint8_t> ExtraData;
			std::span<uint8_t> Data;

			[[nodiscard]] std::vector<uint32_t> marked_sample_block_indices() const;

			[[nodiscard]] const wave_format_ex& get_wav_header() const;

			[[nodiscard]] const adpcm_wave_format& get_adpcm_wav_header() const;

			[[nodiscard]] std::vector<uint8_t> get_wav_file() const;

			[[nodiscard]] const sound_entry_ogg_header& get_ogg_seek_table_header() const;

			[[nodiscard]] std::span<const uint32_t> get_ogg_seek_table() const;

			[[nodiscard]] std::vector<uint8_t> get_ogg_file() const;

			struct audio_info {
				size_t Channels;
				size_t SamplingRate;
				size_t LoopStartBlockIndex;
				size_t LoopEndBlockIndex;
				std::vector<uint8_t> Data;
			};

			[[nodiscard]] audio_info get_ogg_decoded() const;
			
			[[nodiscard]] static audio_info decode_ogg(const std::vector<uint8_t>& oggf);
		};

		[[nodiscard]] std::vector<std::vector<uint8_t>> read_table_1() const { return read_table(m_offsetsTable1, m_endOfTable1); }

		[[nodiscard]] std::vector<std::vector<uint8_t>> read_table_2() const { return read_table(m_offsetsTable2, m_endOfTable2); }

		[[nodiscard]] std::vector<sound_item> read_sound_table() const {
			std::vector<sound_item> res;
			for (size_t i = 0; i < sound_item_count(); ++i)
				res.push_back(read_sound_item(i));
			return res;
		}

		[[nodiscard]] std::vector<std::vector<uint8_t>> read_table_4() const { return read_table(m_offsetsTable4, m_endOfTable4); }

		[[nodiscard]] std::vector<std::vector<uint8_t>> read_table_5() const { return read_table(m_offsetsTable5, m_endOfTable5); }

		[[nodiscard]] size_t sound_item_count() const { return m_soundEntryOffsets.size(); }

		[[nodiscard]] sound_item read_sound_item(size_t entryIndex) const;
	};

	class writer {
	public:
		struct sound_item {
			sound_entry_header Header;
			std::map<std::string, std::vector<uint8_t>> AuxChunks;
			std::vector<uint8_t> ExtraData;
			std::vector<uint8_t> Data;

			[[nodiscard]] wave_format_ex& as_wave_format_ex();
			[[nodiscard]] const wave_format_ex& as_wave_format_ex() const;

			[[nodiscard]] size_t calculate_entry_size() const;
			void export_to(std::vector<uint8_t>& res) const;

			void set_mark_chunks(uint32_t loopStartSampleBlockIndex, uint32_t loopEndSampleBlockIndex, std::span<const uint32_t> chunks);

			static sound_item make_from_reader_sound_item(const reader::sound_item& item); 

			static sound_item make_from_wave(const linear_reader<uint8_t>& reader);

			static sound_item make_from_ogg_encode(
				size_t channels,
				size_t samplingRate,
				size_t loopStartBlockIndex,
				size_t loopEndBlockIndex,
				const linear_reader<uint8_t>& floatSamplesReader,
				const std::function<bool(size_t blockIndex)>& progressCallback,
				std::span<const uint32_t> markIndices,
				float baseQuality = 1.f);

			static sound_item make_from_ogg(const linear_reader<uint8_t>& reader);

			static sound_item make_from_ogg(
				const std::vector<uint8_t>& headerPages,
				std::vector<uint8_t> dataPages,
				uint32_t channels,
				uint32_t samplingRate,
				uint32_t loopStartOffset,
				uint32_t loopEndOffset,
				std::span<uint32_t> seekTable
			);

			static sound_item make_empty(std::optional<std::chrono::milliseconds> duration = std::nullopt);
		};

	private:
		std::vector<std::vector<uint8_t>> m_table1;
		std::vector<std::vector<uint8_t>> m_table2;
		std::vector<sound_item> m_soundEntries;
		std::vector<std::vector<uint8_t>> m_table4;
		std::vector<std::vector<uint8_t>> m_table5;

	public:
		void set_table_1(std::vector<std::vector<uint8_t>> t) {
			m_table1 = std::move(t);
		}

		void set_table_2(std::vector<std::vector<uint8_t>> t) {
			m_table2 = std::move(t);
		}

		void set_table_4(std::vector<std::vector<uint8_t>> t) {
			m_table4 = std::move(t);
		}

		void set_table_5(std::vector<std::vector<uint8_t>> t) {
			// Apparently the game still plays sounds without this table

			m_table5 = std::move(t);
		}

		void set_sound_item(size_t index, sound_item entry) {
			if (m_soundEntries.size() <= index)
				m_soundEntries.resize(index + 1);
			m_soundEntries[index] = std::move(entry);
		}

		[[nodiscard]] std::vector<uint8_t> export_to_bytes() const;
	};
}

#endif
