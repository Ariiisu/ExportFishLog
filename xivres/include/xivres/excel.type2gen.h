#ifndef XIVRES_EXCELGENERATOR_H_
#define XIVRES_EXCELGENERATOR_H_

#include <set>

#include "excel.h"

namespace xivres::excel {
	class type2gen {
		static uint32_t calculate_fixed_data_size(const std::vector<exh::column>& columns);

	public:
		const std::string Name;
		const std::vector<exh::column> Columns;
		const exh::read_strategy ReadStrategy;
		const size_t DivideUnit;
		const uint32_t FixedDataSize;
		std::map<uint32_t, std::map<game_language, std::vector<cell>>> Data;
		std::set<uint32_t> DivideAtIds;
		std::vector<game_language> Languages;
		std::vector<game_language> FillMissingLanguageFrom;

		type2gen(std::string name, std::vector<exh::column> columns, exh::read_strategy readStrategy, size_t divideUnit = SIZE_MAX);

		void add_language(game_language language);

		[[nodiscard]] const std::vector<cell>& get_row(uint32_t id, game_language language) const;

		void set_row(uint32_t id, game_language language, std::vector<cell> row, bool replace = true);

	private:
		[[nodiscard]] std::pair<path_spec, std::vector<char>> flush(uint32_t startId, const std::map<uint32_t, std::vector<char>>& rows, game_language language) const;

	public:
		[[nodiscard]] std::map<path_spec, std::vector<char>, path_spec::FullPathComparator> compile();
	};
}

#endif
