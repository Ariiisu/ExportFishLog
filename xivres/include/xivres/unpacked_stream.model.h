#ifndef XIVRES_MODELPACKEDFILESTREAMDECODER_H_
#define XIVRES_MODELPACKEDFILESTREAMDECODER_H_

#include "unpacked_stream.h"
#include "model.h"

namespace xivres {
	class model_unpacker : public base_unpacker {
		struct block_info_t {
			uint32_t RequestOffsetPastHeader;
			uint32_t BlockOffset;
			uint16_t PaddedChunkSize;
			uint16_t DecompressedSize;
			uint16_t GroupIndex;
			uint16_t GroupBlockIndex;
			
			friend bool operator<(uint32_t r, const block_info_t& info) {
				return r < info.RequestOffsetPastHeader;
			}
		};

		model::header m_header;
		std::vector<block_info_t> m_blocks;

	public:
		model_unpacker(const packed::file_header& header, std::shared_ptr<const packed_stream> strm);

		std::streamsize read(std::streamoff offset, void* buf, std::streamsize length) override;
	};
}

#endif
