#ifndef XIVRES_HOTSWAPPABLEPACKEDFILESTREAM_H_
#define XIVRES_HOTSWAPPABLEPACKEDFILESTREAM_H_

#include "packed_stream.placeholder.h"

namespace xivres {
	class hotswap_packed_stream : public packed_stream {
		const uint32_t m_reservedSize;
		const std::shared_ptr<const packed_stream> m_baseStream;
		std::shared_ptr<const packed_stream> m_stream;

	public:
		hotswap_packed_stream(const xivres::path_spec& pathSpec, uint32_t reservedSize, std::shared_ptr<const packed_stream> strm = nullptr);

		std::shared_ptr<const packed_stream> swap_stream(std::shared_ptr<const packed_stream> newStream = nullptr);

		[[nodiscard]] std::shared_ptr<const packed_stream> base_stream() const;

		[[nodiscard]] std::streamsize size() const override;

		std::streamsize read(std::streamoff offset, void* buf, std::streamsize length) const override;

		[[nodiscard]] packed::type get_packed_type() const override;
	};
}

#endif
