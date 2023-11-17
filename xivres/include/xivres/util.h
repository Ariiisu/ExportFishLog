#ifndef XIVRES_INTERNAL_MISC_H_
#define XIVRES_INTERNAL_MISC_H_

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace xivres::util {
	template<typename T>
	T clamp(T value, T minValue, T maxValue) {
		return (std::min)(maxValue, (std::max)(minValue, value));
	}

	template<typename T, typename TFrom>
	T range_check_cast(TFrom value) {
		if (value < (std::numeric_limits<T>::min)())
			throw std::range_error("Out of range");
		if (value > (std::numeric_limits<T>::max)())
			throw std::range_error("Out of range");
		return static_cast<T>(value);
	}

	template<typename T, size_t C>
	bool all_same_value(T(&arr)[C], std::remove_cv_t<T> supposedValue = 0) {
		for (size_t i = 0; i < C; ++i) {
			if (arr[i] != supposedValue)
				return false;
		}
		return true;
	}

	template<typename T>
	bool all_same_value(std::span<T> arr, std::remove_cv_t<T> supposedValue = 0) {
		for (const auto& e : arr)
			if (e != supposedValue)
				return false;
		return true;
	}

	template<class TElem, class TTraits, class TAlloc>
	[[nodiscard]] std::vector<std::basic_string<TElem, TTraits, TAlloc>> split(const std::basic_string<TElem, TTraits, TAlloc>& str, const std::basic_string<TElem, TTraits, TAlloc>& delimiter, size_t maxSplit = SIZE_MAX) {
		std::vector<std::basic_string<TElem, TTraits, TAlloc>> result;
		if (delimiter.empty()) {
			for (size_t i = 0; i < str.size(); ++i)
				result.push_back(str.substr(i, 1));
		} else {
			size_t previousOffset = 0, offset;
			while (maxSplit && (offset = str.find(delimiter, previousOffset)) != std::string::npos) {
				result.push_back(str.substr(previousOffset, offset - previousOffset));
				previousOffset = offset + delimiter.length();
				--maxSplit;
			}
			result.push_back(str.substr(previousOffset));
		}
		return result;
	}

	template<class TElem, class TTraits, class TAlloc>
	std::basic_string<TElem, TTraits, TAlloc> trim(std::basic_string<TElem, TTraits, TAlloc> s, bool left = true, bool right = true) {
		auto view = std::basic_string_view<TElem, TTraits>(s);
		if (left) {
			while (!view.empty() && (view.front() < 255 && std::isspace(view.front())))
				view = view.substr(1);
		}
		if (right) {
			while (!view.empty() && (view.back() < 255 && std::isspace(view.back())))
				view = view.substr(0, view.size() - 1);
		}
		return { view.begin(), view.end() };
	}

	template<class TElem, class TTraits, class TAlloc>
	[[nodiscard]] std::basic_string<TElem, TTraits, TAlloc> replace(const std::basic_string<TElem, TTraits, TAlloc>& source, const std::basic_string<TElem, TTraits, TAlloc>& from, const std::basic_string<TElem, TTraits, TAlloc>& to) {
		std::basic_string<TElem, TTraits, TAlloc> s;
		s.reserve(source.length());

		size_t last = 0;
		size_t pos;

		while (std::string::npos != (pos = source.find(from, last))) {
			s.append(&source[last], &source[pos]);
			s += to;
			last = pos + from.length();
		}

		s += source.substr(last);
		return s;
	}
}

#endif
