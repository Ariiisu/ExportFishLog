#include "../include/xivres/sqpack.generator.h"

#include <fstream>
#include <ranges>

#include "../include/xivres/packed_stream.hotswap.h"
#include "../include/xivres/packed_stream.model.h"
#include "../include/xivres/packed_stream.placeholder.h"
#include "../include/xivres/packed_stream.standard.h"
#include "../include/xivres/packed_stream.texture.h"

xivres::packed_stream* xivres::sqpack::generator::add_result::any() const {
	if (!Added.empty())
		return Added[0];
	if (!Replaced.empty())
		return Replaced[0];
	if (!SkippedExisting.empty())
		return SkippedExisting[0];
	return nullptr;
}

std::vector<xivres::packed_stream*> xivres::sqpack::generator::add_result::all_success() const {
	std::vector<packed_stream*> res;
	res.insert(res.end(), Added.begin(), Added.end());
	res.insert(res.end(), Replaced.begin(), Replaced.end());
	res.insert(res.end(), SkippedExisting.begin(), SkippedExisting.end());
	return res;
}

xivres::sqpack::generator::entry_info* xivres::sqpack::generator::sqpack_views::find_entry(const path_spec& pathSpec) const {
	if (const auto it = HashOnlyEntries.find(pathSpec); it != HashOnlyEntries.end())
		return it->second.get();
	if (const auto it = FullPathEntries.find(pathSpec); it != FullPathEntries.end())
		return it->second.get();
	return nullptr;
}

xivres::sqpack::generator::entry_info& xivres::sqpack::generator::sqpack_views::get_entry(const path_spec& pathSpec) const {
	if (const auto it = HashOnlyEntries.find(pathSpec); it != HashOnlyEntries.end())
		return *it->second;
	if (const auto it = FullPathEntries.find(pathSpec); it != FullPathEntries.end())
		return *it->second;
	throw std::out_of_range("File not found");
}

xivres::sqpack::generator::add_result& xivres::sqpack::generator::add_result::operator+=(add_result&& r) {
	Added.insert(Added.end(), r.Added.begin(), r.Added.end());
	Replaced.insert(Replaced.end(), r.Replaced.begin(), r.Replaced.end());
	SkippedExisting.insert(SkippedExisting.end(), r.SkippedExisting.begin(), r.SkippedExisting.end());
	Error.insert(Error.end(), r.Error.begin(), r.Error.end());
	r.Added.clear();
	r.Replaced.clear();
	r.SkippedExisting.clear();
	r.Error.clear();
	return *this;
}

xivres::sqpack::generator::add_result& xivres::sqpack::generator::add_result::operator+=(const add_result& r) {
	Added.insert(Added.end(), r.Added.begin(), r.Added.end());
	Replaced.insert(Replaced.end(), r.Replaced.begin(), r.Replaced.end());
	SkippedExisting.insert(SkippedExisting.end(), r.SkippedExisting.begin(), r.SkippedExisting.end());
	Error.insert(Error.end(), r.Error.begin(), r.Error.end());
	return *this;
}

void xivres::sqpack::generator::sqpack_view_entry_cache::buffered_entry::clear() {
	m_view = nullptr;
	m_entry = nullptr;
	if (!m_bufferTemporary.empty())
		std::vector<uint8_t>().swap(m_bufferTemporary);
}

void xivres::sqpack::generator::sqpack_view_entry_cache::buffered_entry::set(const data_view_stream* view, const entry_info* entry) {
	m_view = view;
	m_entry = entry;

	if (entry->EntrySize <= SmallEntryBufferSize) {
		if (!m_bufferTemporary.empty())
			std::vector<uint8_t>().swap(m_bufferTemporary);
		if (m_bufferPreallocated.size() != SmallEntryBufferSize)
			m_bufferPreallocated.resize(SmallEntryBufferSize);
		m_bufferActive = std::span(m_bufferPreallocated.begin(), entry->EntrySize);
	} else {
		m_bufferTemporary.resize(entry->EntrySize);
		m_bufferActive = std::span(m_bufferTemporary);
	}
	entry->Provider->read_fully(0, m_bufferActive);
}

xivres::sqpack::generator::sqpack_view_entry_cache::buffered_entry* xivres::sqpack::generator::sqpack_view_entry_cache::GetBuffer(const data_view_stream* view, const entry_info* entry) {
	if (m_lastActiveEntry.is_same(view, entry))
		return &m_lastActiveEntry;

	if (entry->EntrySize > LargeEntryBufferSizeMax)
		return nullptr;

	m_lastActiveEntry.set(view, entry);
	return &m_lastActiveEntry;
}

xivres::sqpack::generator::generator(std::string ex, std::string name, uint64_t maxFileSize)
	: m_maxFileSize(maxFileSize)
	, DatExpac(std::move(ex))
	, DatName(std::move(name)) {
	if (maxFileSize > sqdata::header::MaxFileSize_MaxValue)
		throw std::invalid_argument("MaxFileSize cannot be more than 32GiB.");
}

void xivres::sqpack::generator::add(add_result& result, std::shared_ptr<packed_stream> provider, bool overwriteExisting) {
	const auto pProvider = provider.get();

	try {
		entry_info* pEntry = nullptr;

		if (const auto it = m_hashOnlyEntries.find(provider->path_spec()); it != m_hashOnlyEntries.end()) {
			pEntry = it->second.get();
			if (!pEntry->Provider->path_spec().has_original() && provider->path_spec().has_original()) {
				pEntry->Provider->update_path_spec(provider->path_spec());
				m_fullEntries.emplace(pProvider->path_spec(), std::move(it->second));
				m_hashOnlyEntries.erase(it);
			}
		} else if (const auto it = m_fullEntries.find(provider->path_spec()); it != m_fullEntries.end()) {
			pEntry = it->second.get();
		}

		if (pEntry) {
			if (!overwriteExisting) {
				pEntry->Provider->update_path_spec(provider->path_spec());
				result.SkippedExisting.emplace_back(pEntry->Provider.get());
				return;
			}
			pEntry->Provider = std::move(provider);
			result.Replaced.emplace_back(pProvider);
			return;
		}

		auto entry = std::make_unique<entry_info>(0, sqindex::data_locator{0, 0}, std::move(provider));
		if (pProvider->path_spec().has_original())
			m_fullEntries.emplace(pProvider->path_spec(), std::move(entry));
		else
			m_hashOnlyEntries.emplace(pProvider->path_spec(), std::move(entry));
		result.Added.emplace_back(pProvider);
	} catch (const std::exception& e) {
		result.Error.emplace_back(pProvider->path_spec(), e.what());
	}
}

xivres::sqpack::generator::add_result xivres::sqpack::generator::add(std::shared_ptr<packed_stream> provider, bool overwriteExisting) {
	add_result result;
	add(result, std::move(provider), overwriteExisting);
	return result;
}

xivres::sqpack::generator::add_result xivres::sqpack::generator::add_sqpack(const std::filesystem::path& indexPath, bool overwriteExisting, bool overwriteUnknownSegments) {
	return add_sqpack(reader::from_path(indexPath));
}

xivres::sqpack::generator::add_result xivres::sqpack::generator::add_sqpack(const xivres::sqpack::reader& reader, bool overwriteExisting, bool overwriteUnknownSegments) {
	if (overwriteUnknownSegments) {
		m_sqpackIndexSegment3 = { reader.Index1.segment_3().begin(), reader.Index1.segment_3().end() };
		m_sqpackIndex2Segment3 = { reader.Index2.segment_3().begin(), reader.Index2.segment_3().end() };
	}

	add_result result;
	for (const auto& entryInfo : reader.Entries) {
		try {
			const volatile auto& x = entryInfo;
			add(result, reader.packed_at(entryInfo), overwriteExisting);
		} catch (const std::exception& e) {
			result.Error.emplace_back(entryInfo.PathSpec, e.what());
		}
	}
	return result;
}

xivres::sqpack::generator::add_result xivres::sqpack::generator::add_file(path_spec pathSpec, const std::filesystem::path& path, bool overwriteExisting) {
	std::shared_ptr<packed_stream> provider;

	auto extensionLower = path.extension().u8string();
	for (auto& c : extensionLower)
		if (u8'A' <= c && c <= u8'Z')
			c += 'a' - 'A';

	if (file_size(path) == 0) {
		provider = std::make_shared<placeholder_packed_stream>(std::move(pathSpec));
	} else if (extensionLower == u8".tex" || extensionLower == u8".atex") {
		provider = std::make_shared<passthrough_packed_stream<texture_passthrough_packer>>(std::move(pathSpec), std::make_shared<file_stream>(path));
	} else if (extensionLower == u8".mdl") {
		provider = std::make_shared<passthrough_packed_stream<model_passthrough_packer>>(std::move(pathSpec), std::make_shared<file_stream>(path));
	} else {
		provider = std::make_shared<passthrough_packed_stream<standard_passthrough_packer>>(std::move(pathSpec), std::make_shared<file_stream>(path));
	}

	return add(provider, overwriteExisting);
}

void xivres::sqpack::generator::reserve_space(path_spec pathSpec, uint32_t size) {
	if (const auto it = m_hashOnlyEntries.find(pathSpec); it != m_hashOnlyEntries.end()) {
		it->second->EntrySize = (std::max)(it->second->EntrySize, size);
		if (!it->second->Provider->path_spec().has_original() && pathSpec.has_original()) {
			it->second->Provider->update_path_spec(pathSpec);
			m_fullEntries.emplace(pathSpec, std::move(it->second));
			m_hashOnlyEntries.erase(it);
		}
	} else if (const auto it = m_fullEntries.find(pathSpec); it != m_fullEntries.end()) {
		it->second->EntrySize = (std::max)(it->second->EntrySize, size);
	} else {
		auto entry = std::make_unique<entry_info>(size, sqindex::data_locator{0, 0}, std::make_shared<placeholder_packed_stream>(std::move(pathSpec)));
		if (entry->Provider->path_spec().has_original())
			m_fullEntries.emplace(entry->Provider->path_spec(), std::move(entry));
		else
			m_hashOnlyEntries.emplace(entry->Provider->path_spec(), std::move(entry));
	}
}

template<xivres::sqpack::sqindex::sqindex_type TSqIndex, typename TFileSegmentType, typename TTextSegmentType, bool UseFolders>
static std::vector<uint8_t> export_index_file_data(
	size_t dataFilesCount,
	std::vector<TFileSegmentType> fileSegment,
	const std::vector<TTextSegmentType>& conflictSegment,
	const std::vector<xivres::sqpack::sqindex::segment_3_entry>& segment3,
	std::vector<xivres::sqpack::sqindex::path_hash_locator> folderSegment = {},
	bool strict = false
) {
	using namespace xivres;

	std::vector<uint8_t> data;
	data.reserve(sizeof(sqpack::header)
		+ sizeof(sqpack::sqindex::header)
		+ std::span(fileSegment).size_bytes()
		+ std::span(conflictSegment).size_bytes()
		+ std::span(segment3).size_bytes()
		+ std::span(folderSegment).size_bytes());

	data.resize(sizeof(sqpack::header) + sizeof(sqpack::sqindex::header));
	auto& header1 = *reinterpret_cast<sqpack::header*>(&data[0]);
	memcpy(header1.Signature, sqpack::header::Signature_Value, sizeof(sqpack::header::Signature_Value));
	header1.HeaderSize = sizeof(sqpack::header);
	header1.Unknown1 = sqpack::header::Unknown1_Value;
	header1.Type = sqpack::file_type::SqIndex;
	header1.Unknown2 = sqpack::header::Unknown2_Value;
	if (strict)
		header1.Sha1.set_from_span(reinterpret_cast<char*>(&header1), offsetof(sqpack::header, Sha1));

	auto& header2 = *reinterpret_cast<sqpack::sqindex::header*>(&data[sizeof(sqpack::header)]);
	std::sort(fileSegment.begin(), fileSegment.end());
	header2.HeaderSize = sizeof(sqpack::sqindex::header);
	header2.Type = TSqIndex;
	header2.HashLocatorSegment.Count = 1;
	header2.HashLocatorSegment.Offset = header1.HeaderSize + header2.HeaderSize;
	header2.HashLocatorSegment.Size = static_cast<uint32_t>(std::span(fileSegment).size_bytes());
	header2.TextLocatorSegment.Count = static_cast<uint32_t>(dataFilesCount);
	header2.TextLocatorSegment.Offset = header2.HashLocatorSegment.Offset + header2.HashLocatorSegment.Size;
	header2.TextLocatorSegment.Size = static_cast<uint32_t>(std::span(conflictSegment).size_bytes());
	header2.UnknownSegment3.Count = 0;
	header2.UnknownSegment3.Offset = header2.TextLocatorSegment.Offset + header2.TextLocatorSegment.Size;
	header2.UnknownSegment3.Size = static_cast<uint32_t>(std::span(segment3).size_bytes());
	header2.PathHashLocatorSegment.Count = 0;
	header2.PathHashLocatorSegment.Offset = header2.UnknownSegment3.Offset + header2.UnknownSegment3.Size;
	if constexpr (UseFolders) {
		for (size_t i = 0; i < fileSegment.size(); ++i) {
			const auto& entry = fileSegment[i];
			if (folderSegment.empty() || folderSegment.back().PathHash != entry.PathHash) {
				folderSegment.emplace_back(
					entry.PathHash,
					static_cast<uint32_t>(header2.HashLocatorSegment.Offset + i * sizeof entry),
					static_cast<uint32_t>(sizeof entry),
					0);
			} else {
				folderSegment.back().PairHashLocatorSize = folderSegment.back().PairHashLocatorSize + sizeof entry;
			}
		}
		header2.PathHashLocatorSegment.Size = static_cast<uint32_t>(std::span(folderSegment).size_bytes());
	}

	if (strict) {
		header2.Sha1.set_from_span(reinterpret_cast<char*>(&header2), offsetof(sqpack::sqindex::header, Sha1));
		if (!fileSegment.empty())
			header2.HashLocatorSegment.Sha1.set_from_span(reinterpret_cast<const uint8_t*>(&fileSegment.front()), header2.HashLocatorSegment.Size);
		if (!conflictSegment.empty())
			header2.TextLocatorSegment.Sha1.set_from_span(reinterpret_cast<const uint8_t*>(&conflictSegment.front()), header2.TextLocatorSegment.Size);
		if (!segment3.empty())
			header2.UnknownSegment3.Sha1.set_from_span(reinterpret_cast<const uint8_t*>(&segment3.front()), header2.UnknownSegment3.Size);
		if constexpr (UseFolders) {
			if (!folderSegment.empty())
				header2.PathHashLocatorSegment.Sha1.set_from_span(reinterpret_cast<const uint8_t*>(&folderSegment.front()), header2.PathHashLocatorSegment.Size);
		}

	}
	if (!fileSegment.empty())
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&fileSegment.front()), reinterpret_cast<const uint8_t*>(&fileSegment.back() + 1));
	if (!conflictSegment.empty())
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&conflictSegment.front()), reinterpret_cast<const uint8_t*>(&conflictSegment.back() + 1));
	if (!segment3.empty())
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&segment3.front()), reinterpret_cast<const uint8_t*>(&segment3.back() + 1));

	if constexpr (UseFolders) {
		if (!folderSegment.empty())
			data.insert(data.end(), reinterpret_cast<const uint8_t*>(&folderSegment.front()), reinterpret_cast<const uint8_t*>(&folderSegment.back() + 1));
	}

	return data;
}

class xivres::sqpack::generator::data_view_stream : public default_base_stream {
	const std::vector<uint8_t> m_header;
	const std::span<entry_info*> m_entries;

	const sqdata::header& SubHeader() const {
		return *reinterpret_cast<const sqdata::header*>(&m_header[sizeof header]);
	}

	static std::vector<uint8_t> Concat(const header& header, const sqdata::header& subheader) {
		std::vector<uint8_t> buffer;
		buffer.reserve(sizeof header + sizeof subheader);
		buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&header), reinterpret_cast<const uint8_t*>(&header + 1));
		buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&subheader), reinterpret_cast<const uint8_t*>(&subheader + 1));
		return buffer;
	}

	mutable size_t m_lastAccessedEntryIndex = SIZE_MAX;
	const std::shared_ptr<sqpack_view_entry_cache> m_buffer;

public:
	data_view_stream(const header& header, const sqdata::header& subheader, std::span<entry_info*> entries, std::shared_ptr<sqpack_view_entry_cache> buffer)
		: m_header(Concat(header, subheader))
		, m_entries(entries)
		, m_buffer(std::move(buffer)) {
	}

	std::streamsize read(std::streamoff offset, void* buf, std::streamsize length) const override {
		if (!length)
			return 0;

		auto relativeOffset = static_cast<uint64_t>(offset);
		auto out = std::span(static_cast<char*>(buf), static_cast<size_t>(length));

		if (relativeOffset < m_header.size()) {
			const auto src = std::span(m_header).subspan(static_cast<size_t>(relativeOffset));
			const auto available = (std::min)(out.size_bytes(), src.size_bytes());
			std::copy_n(src.begin(), available, out.begin());
			out = out.subspan(available);
			relativeOffset = 0;
		} else
			relativeOffset -= m_header.size();

		if (out.empty())
			return length;

		auto it = m_lastAccessedEntryIndex != SIZE_MAX ? m_entries.begin() + static_cast<ptrdiff_t>(m_lastAccessedEntryIndex) : m_entries.begin();
		if (const auto absoluteOffset = relativeOffset + m_header.size();
			(*it)->Locator.offset() > absoluteOffset || absoluteOffset >= (*it)->Locator.offset() + (*it)->EntrySize) {
			it = std::ranges::lower_bound(m_entries, nullptr, [&](entry_info* l, entry_info* r) {
				const auto lo = l ? l->Locator.offset() : absoluteOffset;
				const auto ro = r ? r->Locator.offset() : absoluteOffset;
				return lo < ro;
			});
			if (it != m_entries.begin() && (it == m_entries.end() || (*it)->Locator.offset() > absoluteOffset))
				--it;
		}

		if (it != m_entries.end()) {
			relativeOffset -= (*it)->Locator.offset() - m_header.size();

			for (; it < m_entries.end(); ++it) {
				const auto& entry = **it;
				m_lastAccessedEntryIndex = it - m_entries.begin();

				const auto buf = m_buffer ? m_buffer->GetBuffer(this, &entry) : nullptr;

				if (relativeOffset < entry.EntrySize) {
					const auto available = (std::min)(out.size_bytes(), static_cast<size_t>(entry.EntrySize - relativeOffset));
					if (buf)
						std::copy_n(&buf->buffer()[static_cast<size_t>(relativeOffset)], available, &out[0]);
					else
						entry.Provider->read_fully(static_cast<std::streamoff>(relativeOffset), out.data(), static_cast<std::streamsize>(available));
					out = out.subspan(available);
					relativeOffset = 0;

					if (out.empty())
						break;
				} else
					relativeOffset -= entry.EntrySize;
			}
		}

		return static_cast<std::streamsize>(length - out.size_bytes());
	}

	std::streamsize size() const override {
		return m_header.size() + SubHeader().DataSize;
	}
};

xivres::sqpack::generator::sqpack_views xivres::sqpack::generator::export_to_views(bool strict, const std::shared_ptr<sqpack_view_entry_cache>& dataBuffer) {
	header dataHeader{};
	std::vector<sqdata::header> dataSubheaders;
	std::vector<std::pair<size_t, size_t>> dataEntryRanges;

	auto res = sqpack_views{
		.HashOnlyEntries = std::move(m_hashOnlyEntries),
		.FullPathEntries = std::move(m_fullEntries),
	};

	res.Entries.reserve(m_fullEntries.size() + m_hashOnlyEntries.size());
	for (auto& entry : res.HashOnlyEntries | std::views::values)
		res.Entries.emplace_back(entry.get());
	for (auto& entry : res.FullPathEntries | std::views::values)
		res.Entries.emplace_back(entry.get());

	std::map<std::pair<uint32_t, uint32_t>, std::vector<entry_info*>> pairHashes;
	std::map<uint32_t, std::vector<entry_info*>> fullHashes;
	for (const auto& entry : res.Entries) {
		const auto& pathSpec = entry->Provider->path_spec();
		pairHashes[std::make_pair(pathSpec.path_hash(), pathSpec.name_hash())].emplace_back(entry);
		fullHashes[pathSpec.full_path_hash()].emplace_back(entry);
	}

	for (size_t i = 0; i < res.Entries.size(); ++i) {
		ProgressCallback(i, res.Entries.size());
		auto& entry = res.Entries[i];
		const auto& pathSpec = entry->Provider->path_spec();
		entry->EntrySize = align((std::max)(entry->EntrySize, static_cast<uint32_t>(entry->Provider->size()))).Alloc;
		entry->Provider = std::make_shared<hotswap_packed_stream>(pathSpec, entry->EntrySize, std::move(entry->Provider));

		if (dataSubheaders.empty() ||
			sizeof header + sizeof(sqdata::header) + dataSubheaders.back().DataSize + entry->EntrySize > dataSubheaders.back().MaxFileSize) {
			if (strict && !dataSubheaders.empty()) {
				util::hash_sha1 sha1;
				for (auto j = dataEntryRanges.back().first, j_ = j + dataEntryRanges.back().second; j < j_; ++j) {
					const auto& entry = res.Entries[j];
					const auto& provider = *entry->Provider;
					const auto length = provider.size();
					uint8_t buf[4096];
					for (std::streamoff j = 0; j < length; j += sizeof buf) {
						const auto readlen = static_cast<size_t>((std::min<uint64_t>)(sizeof buf, length - j));
						provider.read_fully(j, buf, static_cast<std::streamsize>(readlen));
						sha1.process_bytes(buf, readlen);
					}
				}
				sha1.get_digest_bytes(dataSubheaders.back().DataSha1.Value);
				dataSubheaders.back().Sha1.set_from_span(reinterpret_cast<char*>(&dataSubheaders.back()), offsetof(sqdata::header, Sha1));
			}
			dataSubheaders.emplace_back(sqdata::header{
				.HeaderSize = sizeof(sqdata::header),
				.Unknown1 = sqdata::header::Unknown1_Value,
				.DataSize = 0,
				.SpanIndex = static_cast<uint32_t>(dataSubheaders.size()),
				.MaxFileSize = m_maxFileSize,
			});
			dataEntryRanges.emplace_back(i, 0);
		}

		entry->Locator = {static_cast<uint32_t>(dataSubheaders.size() - 1), sizeof header + sizeof(sqdata::header) + dataSubheaders.back().DataSize};

		dataSubheaders.back().DataSize = dataSubheaders.back().DataSize + entry->EntrySize;
		dataEntryRanges.back().second++;
	}

	if (strict && !dataSubheaders.empty()) {
		util::hash_sha1 sha1;
		for (auto j = dataEntryRanges.back().first, j_ = j + dataEntryRanges.back().second; j < j_; ++j) {
			const auto& entry = res.Entries[j];
			const auto& provider = *entry->Provider;
			const auto length = provider.size();
			uint8_t buf[4096];
			for (std::streamoff j = 0; j < length; j += sizeof buf) {
				const auto readlen = static_cast<size_t>((std::min<uint64_t>)(sizeof buf, length - j));
				provider.read_fully(j, buf, static_cast<std::streamsize>(readlen));
				sha1.process_bytes(buf, readlen);
			}
		}
		sha1.get_digest_bytes(dataSubheaders.back().DataSha1.Value);
		dataSubheaders.back().Sha1.set_from_span(reinterpret_cast<char*>(&dataSubheaders.back()), offsetof(sqdata::header, Sha1));
	}

	std::vector<sqindex::pair_hash_locator> fileEntries1;
	std::vector<sqindex::pair_hash_with_text_locator> conflictEntries1;
	for (const auto& [pairHash, correspondingEntries] : pairHashes) {
		if (correspondingEntries.size() == 1) {
			fileEntries1.emplace_back(sqindex::pair_hash_locator{pairHash.second, pairHash.first, correspondingEntries.front()->Locator, 0});
		} else {
			fileEntries1.emplace_back(sqindex::pair_hash_locator{pairHash.second, pairHash.first, sqindex::data_locator::Synonym(), 0});
			uint32_t i = 0;
			for (const auto& entry : correspondingEntries) {
				conflictEntries1.emplace_back(sqindex::pair_hash_with_text_locator{
					.NameHash = pairHash.second,
					.PathHash = pairHash.first,
					.Locator = entry->Locator,
					.ConflictIndex = i++,
				});
				const auto& path = entry->Provider->path_spec().text();
				strncpy_s(conflictEntries1.back().FullPath, path.c_str(), path.size());
			}
		}
	}
	conflictEntries1.emplace_back(sqindex::pair_hash_with_text_locator{
		.NameHash = sqindex::pair_hash_with_text_locator::EndOfList,
		.PathHash = sqindex::pair_hash_with_text_locator::EndOfList,
		.Locator = 0,
		.ConflictIndex = sqindex::pair_hash_with_text_locator::EndOfList,
	});

	std::vector<sqindex::full_hash_locator> fileEntries2;
	std::vector<sqindex::full_hash_with_text_locator> conflictEntries2;
	for (const auto& [fullHash, correspondingEntries] : fullHashes) {
		if (correspondingEntries.size() == 1) {
			fileEntries2.emplace_back(sqindex::full_hash_locator{fullHash, correspondingEntries.front()->Locator});
		} else {
			fileEntries2.emplace_back(sqindex::full_hash_locator{fullHash, sqindex::data_locator::Synonym()});
			uint32_t i = 0;
			for (const auto& entry : correspondingEntries) {
				conflictEntries2.emplace_back(sqindex::full_hash_with_text_locator{
					.FullPathHash = fullHash,
					.UnusedHash = 0,
					.Locator = entry->Locator,
					.ConflictIndex = i++,
				});
				const auto& path = entry->Provider->path_spec().text();
				strncpy_s(conflictEntries2.back().FullPath, path.c_str(), path.size());
			}
		}
	}
	conflictEntries2.emplace_back(sqindex::full_hash_with_text_locator{
		.FullPathHash = sqindex::full_hash_with_text_locator::EndOfList,
		.UnusedHash = sqindex::full_hash_with_text_locator::EndOfList,
		.Locator = 0,
		.ConflictIndex = sqindex::full_hash_with_text_locator::EndOfList,
	});

	memcpy(dataHeader.Signature, header::Signature_Value, sizeof(header::Signature_Value));
	dataHeader.HeaderSize = sizeof header;
	dataHeader.Unknown1 = header::Unknown1_Value;
	dataHeader.Type = file_type::SqData;
	dataHeader.Unknown2 = header::Unknown2_Value;
	if (strict)
		dataHeader.Sha1.set_from_span(reinterpret_cast<char*>(&dataHeader), offsetof(sqpack::header, Sha1));

	res.Index1 = std::make_shared<memory_stream>(export_index_file_data<sqindex::sqindex_type::Index, sqindex::pair_hash_locator, sqindex::pair_hash_with_text_locator, true>(
		dataSubheaders.size(), std::move(fileEntries1), conflictEntries1, m_sqpackIndexSegment3, std::vector<sqindex::path_hash_locator>(), strict));
	res.Index2 = std::make_shared<memory_stream>(export_index_file_data<sqindex::sqindex_type::Index, sqindex::full_hash_locator, sqindex::full_hash_with_text_locator, false>(
		dataSubheaders.size(), std::move(fileEntries2), conflictEntries2, m_sqpackIndex2Segment3, std::vector<sqindex::path_hash_locator>(), strict));
	for (size_t i = 0; i < dataSubheaders.size(); ++i)
		res.Data.emplace_back(std::make_shared<data_view_stream>(dataHeader, dataSubheaders[i], std::span(res.Entries).subspan(dataEntryRanges[i].first, dataEntryRanges[i].second), dataBuffer));

	return res;
}

void xivres::sqpack::generator::export_to_files(const std::filesystem::path& dir, bool strict, size_t cores) {
	header dataHeader{};
	memcpy(dataHeader.Signature, header::Signature_Value, sizeof(header::Signature_Value));
	dataHeader.HeaderSize = sizeof header;
	dataHeader.Unknown1 = header::Unknown1_Value;
	dataHeader.Type = file_type::SqData;
	dataHeader.Unknown2 = header::Unknown2_Value;
	if (strict)
		dataHeader.Sha1.set_from_span(reinterpret_cast<char*>(&dataHeader), offsetof(sqpack::header, Sha1));

	std::vector<sqdata::header> dataSubheaders;

	std::vector<std::unique_ptr<entry_info>> entries;
	entries.reserve(m_fullEntries.size() + m_hashOnlyEntries.size());
	for (auto& val : m_fullEntries | std::views::values)
		entries.emplace_back(std::move(val));
	for (auto& val : m_hashOnlyEntries | std::views::values)
		entries.emplace_back(std::move(val));
	m_fullEntries.clear();
	m_hashOnlyEntries.clear();

	std::map<std::pair<uint32_t, uint32_t>, std::vector<entry_info*>> pairHashes;
	std::map<uint32_t, std::vector<entry_info*>> fullHashes;
	std::map<entry_info*, path_spec> entryPathSpecs;
	for (const auto& entry : entries) {
		const auto& pathSpec = entry->Provider->path_spec();
		entryPathSpecs.emplace(entry.get(), pathSpec);
		pairHashes[std::make_pair(pathSpec.path_hash(), pathSpec.name_hash())].emplace_back(entry.get());
		fullHashes[pathSpec.full_path_hash()].emplace_back(entry.get());
	}

	std::vector<sqindex::data_locator> locators;

	{
		util::thread_pool::task_waiter<std::pair<size_t, std::vector<char>>> waiter;
		std::fstream dataFile;

		for (size_t i = 0;;) {
			for (; i < entries.size() && waiter.pending() < (std::max<size_t>)(8, 2 * waiter.pool().concurrency()); ++i) {
				waiter.submit([this, i, entry = entries[i].get()](util::thread_pool::base_task& task) {
					task.throw_if_cancelled();
					return std::make_pair(i, entry->Provider->read_vector<char>());
				});
				ProgressCallback(i, entries.size());
			}

			const auto resultPair = waiter.get();
			if (!resultPair)
				break;

			auto& entry = *entries[resultPair->first];
			auto& data = resultPair->second;
			const auto provider{std::move(entry.Provider)};
			const auto entrySize = provider->size();

			if (dataSubheaders.empty() ||
				sizeof header + sizeof(sqdata::header) + dataSubheaders.back().DataSize + entrySize > dataSubheaders.back().MaxFileSize) {
				if (!dataSubheaders.empty() && dataFile.is_open()) {
					if (strict) {
						std::vector<char> buf(65536);
						util::hash_sha1 sha1;
						dataFile.seekg(sizeof header + sizeof(sqdata::header), std::ios::beg);
						align<uint64_t>(dataSubheaders.back().DataSize, buf.size()).iterate_chunks([&](uint64_t index, uint64_t offset, uint64_t size) {
							dataFile.read(&buf[0], static_cast<size_t>(size));
							if (!dataFile)
								throw std::runtime_error("Failed to read from output data file.");
							sha1.process_bytes(&buf[0], static_cast<size_t>(size));
						}, sizeof header + sizeof(sqdata::header));

						sha1.get_digest_bytes(dataSubheaders.back().DataSha1.Value);
						dataSubheaders.back().Sha1.set_from_span(reinterpret_cast<char*>(&dataSubheaders.back()), offsetof(sqdata::header, Sha1));
					}

					dataFile.seekp(0, std::ios::beg);
					dataFile.write(reinterpret_cast<const char*>(&dataHeader), sizeof dataHeader);
					dataFile.write(reinterpret_cast<const char*>(&dataSubheaders.back()), sizeof dataSubheaders.back());
					dataFile.close();
				}

				dataFile.open(dir / std::format("{}.win32.dat{}", DatName, dataSubheaders.size()), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
				dataSubheaders.emplace_back(sqdata::header{
					.HeaderSize = sizeof(sqdata::header),
					.Unknown1 = sqdata::header::Unknown1_Value,
					.DataSize = 0,
					.SpanIndex = static_cast<uint32_t>(dataSubheaders.size()),
					.MaxFileSize = m_maxFileSize,
				});
			}

			entry.Locator = {static_cast<uint32_t>(dataSubheaders.size() - 1), sizeof header + sizeof(sqdata::header) + dataSubheaders.back().DataSize};
			dataFile.seekg(static_cast<std::streamoff>(entry.Locator.offset()), std::ios::beg);
			dataFile.write(&data[0], static_cast<std::streamsize>(data.size()));
			if (!dataFile)
				throw std::runtime_error("Failed to write to output data file.");

			dataSubheaders.back().DataSize = dataSubheaders.back().DataSize + entrySize;
		}

		if (!dataSubheaders.empty() && dataFile.is_open()) {
			if (strict) {
				std::vector<char> buf(65536);
				util::hash_sha1 sha1;
				dataFile.seekg(sizeof header + sizeof(sqdata::header), std::ios::beg);
				align<uint64_t>(dataSubheaders.back().DataSize, buf.size()).iterate_chunks([&](uint64_t index, uint64_t offset, uint64_t size) {
					dataFile.read(&buf[0], static_cast<size_t>(size));
					if (!dataFile)
						throw std::runtime_error("Failed to read from output data file.");
					sha1.process_bytes(&buf[0], static_cast<size_t>(size));
				}, sizeof header + sizeof(sqdata::header));

				sha1.get_digest_bytes(dataSubheaders.back().DataSha1.Value);
				dataSubheaders.back().Sha1.set_from_span(reinterpret_cast<char*>(&dataSubheaders.back()), offsetof(sqdata::header, Sha1));
			}

			dataFile.seekp(0, std::ios::beg);
			dataFile.write(reinterpret_cast<const char*>(&dataHeader), sizeof dataHeader);
			dataFile.write(reinterpret_cast<const char*>(&dataSubheaders.back()), sizeof dataSubheaders.back());
			dataFile.close();
		}
	}

	std::vector<sqindex::pair_hash_locator> fileEntries1;
	std::vector<sqindex::pair_hash_with_text_locator> conflictEntries1;
	for (const auto& [pairHash, correspondingEntries] : pairHashes) {
		if (correspondingEntries.size() == 1) {
			fileEntries1.emplace_back(sqindex::pair_hash_locator{pairHash.second, pairHash.first, correspondingEntries.front()->Locator, 0});
		} else {
			fileEntries1.emplace_back(sqindex::pair_hash_locator{pairHash.second, pairHash.first, sqindex::data_locator::Synonym(), 0});
			uint32_t i = 0;
			for (const auto& entry : correspondingEntries) {
				conflictEntries1.emplace_back(sqindex::pair_hash_with_text_locator{
					.NameHash = pairHash.second,
					.PathHash = pairHash.first,
					.Locator = entry->Locator,
					.ConflictIndex = i++,
				});
				const auto& path = entryPathSpecs[entry].text();
				strncpy_s(conflictEntries1.back().FullPath, path.c_str(), path.size());
			}
		}
	}
	conflictEntries1.emplace_back(sqindex::pair_hash_with_text_locator{
		.NameHash = sqindex::pair_hash_with_text_locator::EndOfList,
		.PathHash = sqindex::pair_hash_with_text_locator::EndOfList,
		.Locator = 0,
		.ConflictIndex = sqindex::pair_hash_with_text_locator::EndOfList,
	});

	std::vector<sqindex::full_hash_locator> fileEntries2;
	std::vector<sqindex::full_hash_with_text_locator> conflictEntries2;
	for (const auto& [fullHash, correspondingEntries] : fullHashes) {
		if (correspondingEntries.size() == 1) {
			fileEntries2.emplace_back(sqindex::full_hash_locator{fullHash, correspondingEntries.front()->Locator});
		} else {
			fileEntries2.emplace_back(sqindex::full_hash_locator{fullHash, sqindex::data_locator::Synonym()});
			uint32_t i = 0;
			for (const auto& entry : correspondingEntries) {
				conflictEntries2.emplace_back(sqindex::full_hash_with_text_locator{
					.FullPathHash = fullHash,
					.UnusedHash = 0,
					.Locator = entry->Locator,
					.ConflictIndex = i++,
				});
				const auto& path = entryPathSpecs[entry].text();
				strncpy_s(conflictEntries2.back().FullPath, path.c_str(), path.size());
			}
		}
	}
	conflictEntries2.emplace_back(sqindex::full_hash_with_text_locator{
		.FullPathHash = sqindex::full_hash_with_text_locator::EndOfList,
		.UnusedHash = sqindex::full_hash_with_text_locator::EndOfList,
		.Locator = 0,
		.ConflictIndex = sqindex::full_hash_with_text_locator::EndOfList,
	});

	auto indexData = export_index_file_data<sqindex::sqindex_type::Index, sqindex::pair_hash_locator, sqindex::pair_hash_with_text_locator, true>(
		dataSubheaders.size(), std::move(fileEntries1), conflictEntries1, m_sqpackIndexSegment3, std::vector<sqindex::path_hash_locator>(), strict);
	std::ofstream(dir / std::format("{}.win32.index", DatName), std::ios::binary).write(reinterpret_cast<const char*>(&indexData[0]), indexData.size());

	indexData = export_index_file_data<sqindex::sqindex_type::Index, sqindex::full_hash_locator, sqindex::full_hash_with_text_locator, false>(
		dataSubheaders.size(), std::move(fileEntries2), conflictEntries2, m_sqpackIndex2Segment3, std::vector<sqindex::path_hash_locator>(), strict);
	std::ofstream(dir / std::format("{}.win32.index2", DatName), std::ios::binary).write(reinterpret_cast<const char*>(&indexData[0]), indexData.size());
}

std::unique_ptr<xivres::default_base_stream> xivres::sqpack::generator::get(const path_spec& pathSpec) const {
	if (const auto it = m_hashOnlyEntries.find(pathSpec); it != m_hashOnlyEntries.end())
		return std::make_unique<unpacked_stream>(it->second->Provider);
	if (const auto it = m_fullEntries.find(pathSpec); it != m_fullEntries.end())
		return std::make_unique<unpacked_stream>(it->second->Provider);
	throw std::out_of_range(std::format("path_spec({}) not found", pathSpec));
}

std::vector<xivres::path_spec> xivres::sqpack::generator::all_path_spec() const {
	std::vector<path_spec> res;
	res.reserve(m_hashOnlyEntries.size() + m_fullEntries.size());
	for (const auto& entry : m_hashOnlyEntries | std::views::keys)
		res.emplace_back(entry);
	for (const auto& entry : m_fullEntries | std::views::keys)
		res.emplace_back(entry);
	return res;
}
