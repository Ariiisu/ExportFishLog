#ifndef XIVRES_SQPACKGENERATOR_H_
#define XIVRES_SQPACKGENERATOR_H_

#include <thread>

#include "sqpack.reader.h"
#include "unpacked_stream.h"
#include "util.listener_manager.h"

namespace xivres::sqpack {
	class generator {
		const uint64_t m_maxFileSize;

		class data_view_stream;

	public:
		struct entry_info {
			uint32_t EntrySize{};
			sqindex::data_locator Locator{};

			std::shared_ptr<packed_stream> Provider;
		};

		struct add_result {
			std::vector<packed_stream*> Added;
			std::vector<packed_stream*> Replaced;
			std::vector<packed_stream*> SkippedExisting;
			std::vector<std::pair<path_spec, std::string>> Error;

			add_result& operator+=(const add_result& r);
			add_result& operator+=(add_result&& r);

			[[nodiscard]] packed_stream* any() const;
			[[nodiscard]] std::vector<packed_stream*> all_success() const;
		};

		struct sqpack_views {
			std::shared_ptr<stream> Index1;
			std::shared_ptr<stream> Index2;
			std::vector<std::shared_ptr<stream>> Data;
			std::vector<entry_info*> Entries;
			std::map<path_spec, std::unique_ptr<entry_info>, path_spec::AllHashComparator> HashOnlyEntries;
			std::map<path_spec, std::unique_ptr<entry_info>, path_spec::FullPathComparator> FullPathEntries;

			[[nodiscard]] entry_info* find_entry(const path_spec& pathSpec) const;
			[[nodiscard]] entry_info& get_entry(const path_spec& pathSpec) const;
		};

		class sqpack_view_entry_cache {
			static constexpr auto SmallEntryBufferSize = (INTPTR_MAX == INT64_MAX ? 256 : 8) * 1048576;
			static constexpr auto LargeEntryBufferSizeMax = (INTPTR_MAX == INT64_MAX ? 1024 : 64) * 1048576;

		public:
			class buffered_entry {
				const data_view_stream* m_view = nullptr;
				const entry_info* m_entry = nullptr;
				std::vector<uint8_t> m_bufferPreallocated;
				std::vector<uint8_t> m_bufferTemporary;
				std::span<uint8_t> m_bufferActive;

			public:
				bool empty() const { return m_view == nullptr || m_entry == nullptr; }
				bool is_same(const data_view_stream* view, const entry_info* entry) const { return m_view == view && m_entry == entry; }
				void clear();
				auto get() const { return std::make_pair(m_view, m_entry); }
				void set(const data_view_stream* view, const entry_info* entry);
				const auto& buffer() const { return m_bufferActive; }
			};

		private:
			buffered_entry m_lastActiveEntry;

		public:
			buffered_entry* GetBuffer(const data_view_stream* view, const entry_info* entry);

			void Flush() { m_lastActiveEntry.clear(); }
		};

		const std::string DatExpac;
		const std::string DatName;

	private:
		std::map<path_spec, std::unique_ptr<entry_info>, path_spec::AllHashComparator> m_hashOnlyEntries;
		std::map<path_spec, std::unique_ptr<entry_info>, path_spec::FullPathComparator> m_fullEntries;

		std::vector<sqindex::segment_3_entry> m_sqpackIndexSegment3;
		std::vector<sqindex::segment_3_entry> m_sqpackIndex2Segment3;

	public:
		util::listener_manager<generator, void, size_t, size_t> ProgressCallback;

		generator(std::string ex, std::string name, uint64_t maxFileSize = sqdata::header::MaxFileSize_MaxValue);

		void add(add_result& result, std::shared_ptr<packed_stream> provider, bool overwriteExisting);
		add_result add(std::shared_ptr<packed_stream> provider, bool overwriteExisting = true);
		add_result add_sqpack(const std::filesystem::path& indexPath, bool overwriteExisting = true, bool overwriteUnknownSegments = false);
		add_result add_sqpack(const xivres::sqpack::reader& reader, bool overwriteExisting = true, bool overwriteUnknownSegments = false);
		add_result add_file(path_spec pathSpec, const std::filesystem::path& path, bool overwriteExisting = true);
		void reserve_space(path_spec pathSpec, uint32_t size);

		[[nodiscard]] sqpack_views export_to_views(bool strict, const std::shared_ptr<sqpack_view_entry_cache>& dataBuffer = nullptr);
		void export_to_files(const std::filesystem::path& dir, bool strict = false, size_t cores = std::thread::hardware_concurrency());

		[[nodiscard]] std::unique_ptr<default_base_stream> get(const path_spec& pathSpec) const;
		[[nodiscard]] std::vector<path_spec> all_path_spec() const;
	};
}

#endif
