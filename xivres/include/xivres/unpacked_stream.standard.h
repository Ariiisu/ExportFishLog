#ifndef XIVRES_BINARYPACKEDFILESTREAMDECODER_H_
#define XIVRES_BINARYPACKEDFILESTREAMDECODER_H_

#include "packed_stream.h"
#include "sqpack.h"
#include "unpacked_stream.h"

namespace xivres {
	class standard_unpacker : public base_unpacker {
		struct block_info_t {
			uint32_t RequestOffset;
			uint32_t BlockOffset;
			uint32_t BlockSize;
			
			friend bool operator<(uint32_t r, const block_info_t& info) {
				return r < info.RequestOffset;
			}
		};
		
		const uint32_t m_headerSize;
		std::vector<block_info_t> m_blocks;

	public:
		standard_unpacker(const packed::file_header& header, std::shared_ptr<const packed_stream> strm);

		std::streamsize read(std::streamoff offset, void* buf, std::streamsize length) override;
	};
}

#endif
