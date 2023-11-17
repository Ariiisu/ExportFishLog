#include "../include/xivres/common.h"

#include <nlohmann/json.hpp>

#include "../include/xivres/util.unicode.h"

void xivres::to_json(nlohmann::json& j, const game_language& value) {
	switch (value) {
		case game_language::Japanese:
			j = "Japanese";
			break;
		case game_language::English:
			j = "English";
			break;
		case game_language::German:
			j = "German";
			break;
		case game_language::French:
			j = "French";
			break;
		case game_language::ChineseSimplified:
			j = "ChineseSimplified";
			break;
		case game_language::Korean:
			j = "Korean";
			break;
		case game_language::Unspecified:
		default:
			j = "Unspecified"; // fallback
	}
}

void xivres::from_json(const nlohmann::json& j, game_language& newValue) {
	const auto newValueString = util::unicode::convert<std::string>(j.get<std::string>(), &util::unicode::lower);

	newValue = game_language::Unspecified;
	if (newValueString.empty())
		return;

	if (newValueString == "ja" || newValueString == "japanese")
		newValue = game_language::Japanese;
	else if (newValueString == "en" || newValueString == "english")
		newValue = game_language::English;
	else if (newValueString == "de" || newValueString == "deutsche" || newValueString == "german")
		newValue = game_language::German;
	else if (newValueString == "fr" || newValueString == "french")
		newValue = game_language::French;
	else if (newValueString == "chs" || newValueString == "chinese" || newValueString == "chinesesimplified" || newValueString == "zh")
		newValue = game_language::ChineseSimplified;
	else if (newValueString == "ko" || newValueString == "korean")
		newValue = game_language::Korean;
}

const char* xivres::game_language_code(game_language lang) {
	switch (lang) {
		case game_language::Unspecified: return nullptr;
		case game_language::Japanese: return "ja";
		case game_language::English: return "en";
		case game_language::German: return "de";
		case game_language::French: return "fr";
		case game_language::ChineseSimplified: return "chs";
		case game_language::Korean: return "ko";
		default: throw std::out_of_range("Invalid language");
	}
}
