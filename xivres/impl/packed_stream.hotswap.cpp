#include "../include/xivres/packed_stream.hotswap.h"

xivres::hotswap_packed_stream::hotswap_packed_stream(const xivres::path_spec& pathSpec, uint32_t reservedSize, std::shared_ptr<const packed_stream> strm)
	: packed_stream(pathSpec)
	, m_reservedSize(align(reservedSize))
	, m_baseStream(std::move(strm)) {
	if (m_baseStream && m_baseStream->size() > m_reservedSize)
		throw std::invalid_argument("Provided strm requires more space than reserved size");
}

std::shared_ptr<const xivres::packed_stream> xivres::hotswap_packed_stream::swap_stream(std::shared_ptr<const packed_stream> newStream) {
	if (newStream && newStream->size() > m_reservedSize)
		throw std::invalid_argument("Provided strm requires more space than reserved size");
	auto oldStream{ std::move(m_stream) };
	m_stream = std::move(newStream);
	return oldStream;
}

std::shared_ptr<const xivres::packed_stream> xivres::hotswap_packed_stream::base_stream() const {
	return m_baseStream;
}

std::streamsize xivres::hotswap_packed_stream::size() const {
	return m_reservedSize;
}

std::streamsize xivres::hotswap_packed_stream::read(std::streamoff offset, void* buf, std::streamsize length) const {
	if (offset >= m_reservedSize)
		return 0;
	if (offset + length > m_reservedSize)
		length = m_reservedSize - offset;

	auto target = std::span(static_cast<uint8_t*>(buf), static_cast<size_t>(length));
	const auto& underlyingStream = m_stream ? *m_stream : m_baseStream ? *m_baseStream : placeholder_packed_stream::instance();
	const auto underlyingStreamLength = underlyingStream.size();
	const auto dataLength = offset < underlyingStreamLength ? (std::min)(length, underlyingStreamLength - offset) : 0;

	if (offset < underlyingStreamLength) {
		const auto dataTarget = target.subspan(0, static_cast<size_t>(dataLength));
		const auto readLength = static_cast<size_t>(underlyingStream.read(offset, &dataTarget[0], dataTarget.size_bytes()));
		if (readLength != dataTarget.size_bytes())
			throw std::logic_error("HotSwappableEntryProvider underlying data read fail");
		target = target.subspan(readLength);
	}
	std::ranges::fill(target, 0);
	return length;
}

xivres::packed::type xivres::hotswap_packed_stream::get_packed_type() const {
	return m_stream ? m_stream->get_packed_type() : (m_baseStream ? m_baseStream->get_packed_type() : placeholder_packed_stream::instance().get_packed_type());
}
