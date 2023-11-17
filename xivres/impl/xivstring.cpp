#include "../include/xivres/xivstring.h"

#include <algorithm>

#include "../include/xivres/util.span_cast.h"

std::unique_ptr<xivres::xivstring::xivexpr> xivres::xivstring::xivexpr::parse(std::string_view s) {
	const auto marker = static_cast<uint8_t>(s.at(0));
	if (marker < 0xD0)
		return std::make_unique<xivexpr_uint32>(s);
	if (marker < 0xE0)
		return std::make_unique<xivexpr_param>(s);
	if (marker < 0xE6)
		return std::make_unique<xivexpr_binary>(s);
	if (marker < 0xEC)
		return std::make_unique<xivexpr_unary>(s);
	if (marker == 0xEC)
		return std::make_unique<xivexpr_param>(s);
	if (marker < 0xF0)
		throw std::runtime_error("Unsupported expression type");
	if (marker < 0xFF)
		return std::make_unique<xivexpr_uint32>(s);
	return std::make_unique<xivexpr_string>(s);
}

xivres::xivstring::xivpayload::xivpayload(std::string_view s)
	: m_type(static_cast<xivpayload_type>(s.at(0))) {
	auto length = xivexpr_uint32(s.substr(1));
	m_escaped = s.substr(0, 1 + length.size() + length.value());
}

xivres::xivstring::xivpayload::xivpayload(const xivpayload& r)
	: m_type(r.m_type)
	, m_escaped(r.m_escaped) {
	m_expressions.reserve(r.m_expressions.size());
	for (const auto& expr : r.m_expressions)
		m_expressions.emplace_back(expr->clone());
}

xivres::xivstring::xivpayload& xivres::xivstring::xivpayload::operator=(const xivpayload& r) {
	if (this == &r)
		return *this;

	m_type = r.m_type;
	m_escaped = r.m_escaped;
	m_expressions.reserve(r.m_expressions.size());
	for (const auto& expr : r.m_expressions)
		m_expressions.emplace_back(expr->clone());
	return *this;
}

std::string xivres::xivstring::xivpayload::repr() const {
	static_cast<void>(parse());
	std::string res = "<payload type=\"";
	switch (m_type) {
		case xivpayload_type::Unset: res += "Unset";
			break;
		case xivpayload_type::ResetTime: res += "ResetTime";
			break;
		case xivpayload_type::Time: res += "Time";
			break;
		case xivpayload_type::If: res += "If";
			break;
		case xivpayload_type::Switch: res += "Switch";
			break;
		case xivpayload_type::ActorFullName: res += "ActorFullName";
			break;
		case xivpayload_type::IfEquals: res += "IfEquals";
			break;
		case xivpayload_type::IfJosa: res += "IfJosa";
			break;
		case xivpayload_type::IfJosaro: res += "IfJosaRo";
			break;
		case xivpayload_type::IfPlayer: res += "IfPlayer";
			break;
		case xivpayload_type::NewLine: res += "NewLine";
			break;
		case xivpayload_type::BitmapFontIcon: res += "BitmapFontIcon";
			break;
		case xivpayload_type::ColorFill: res += "ColorFill";
			break;
		case xivpayload_type::ColorBorder: res += "ColorBorder";
			break;
		case xivpayload_type::SoftHyphen: res += "SoftHyphen";
			break;
		case xivpayload_type::DialoguePageSeparator: res += "DialoguePageSeparator";
			break;
		case xivpayload_type::Italics: res += "Italics";
			break;
		case xivpayload_type::Indent: res += "Indent";
			break;
		case xivpayload_type::Icon2: res += "Icon2";
			break;
		case xivpayload_type::Hyphen: res += "Hyphen";
			break;
		case xivpayload_type::Value: res += "Value";
			break;
		case xivpayload_type::Format: res += "Format";
			break;
		case xivpayload_type::TwoDigitValue: res += "TwoDigitValue";
			break;
		case xivpayload_type::SheetReference: res += "SheetReference";
			break;
		case xivpayload_type::Highlight: res += "Highlight";
			break;
		case xivpayload_type::Link: res += "Link";
			break;
		case xivpayload_type::Split: res += "Split";
			break;
		case xivpayload_type::Placeholder: res += "Placeholder";
			break;
		case xivpayload_type::SheetReferenceJa: res += "SheetReferenceJa";
			break;
		case xivpayload_type::SheetReferenceEn: res += "SheetReferenceEn";
			break;
		case xivpayload_type::SheetReferenceDe: res += "SheetReferenceDe";
			break;
		case xivpayload_type::SheetReferenceFr: res += "SheetReferenceFr";
			break;
		case xivpayload_type::InstanceContent: res += "InstanceContent";
			break;
		case xivpayload_type::UiColorFill: res += "UiColorFill";
			break;
		case xivpayload_type::UiColorBorder: res += "UiColorBorder";
			break;
		case xivpayload_type::ZeroPaddedValue: res += "ZeroPaddedValue";
			break;
		case xivpayload_type::OrdinalValue: res += "OrdinalValue";
			break;
		case xivpayload_type::X18: res += "X18";
			break;
		case xivpayload_type::X1a: res += "X1a";
			break;
		case xivpayload_type::X1b: res += "X1b";
			break;
		case xivpayload_type::X25: res += "X25";
			break;
		case xivpayload_type::X2c: res += "X2c";
			break;
		case xivpayload_type::X2e: res += "X2e";
			break;
		case xivpayload_type::X5f: res += "X5f";
			break;
		case xivpayload_type::X60: res += "X60";
			break;
		default: ;
	}
	res += "\">";
	for (const auto& expr : m_expressions)
		res += expr->repr();
	res += "</payload>";
	return res;
}

const std::vector<std::unique_ptr<xivres::xivstring::xivexpr>>& xivres::xivstring::xivpayload::parse() const {
	if (!m_expressions.empty() || m_escaped.empty())
		return m_expressions;

	auto length = xivexpr_uint32(std::string_view(m_escaped).substr(1));

	std::vector<std::unique_ptr<xivexpr>> expressions;
	for (auto remaining = std::string_view(m_escaped).substr(1 + length.size()); !remaining.empty();) {
		expressions.emplace_back(xivexpr::parse(remaining));
		remaining = remaining.substr(expressions.back()->size());
	}

	return m_expressions = std::move(expressions);
}

const std::string& xivres::xivstring::xivpayload::escape() const {
	if (m_expressions.empty() || !m_escaped.empty())
		return m_escaped;

	std::string body;
	for (const auto& expr : m_expressions) {
		const auto i = body.size();
		body.resize(i + expr->size());
		static_cast<void>(expr->encode(std::span(body).subspan(i)));
	}

	auto length = xivexpr_uint32(static_cast<uint32_t>(body.size()));

	std::string res;
	res.reserve(1 + length.size() + length.value());
	res.resize(5);
	res[0] = static_cast<char>(m_type);
	res.resize(1 + length.encode(std::span(res).subspan(1)));
	res.insert(res.end(), body.begin(), body.end());

	return m_escaped = std::move(body);
}

xivres::xivstring::xivstring() = default;

xivres::xivstring::xivstring(const xivstring&) = default;

xivres::xivstring::xivstring(xivstring&&) noexcept = default;

xivres::xivstring& xivres::xivstring::operator=(const xivstring&) = default;

xivres::xivstring& xivres::xivstring::operator=(xivstring&&) noexcept = default;

xivres::xivstring::xivstring(std::string s, std::vector<xivpayload> payloads) {
	parsed(std::move(s), std::move(payloads));
}

xivres::xivstring::xivstring(std::string s) {
	escaped(std::move(s));
}

bool xivres::xivstring::use_newline_payload() const {
	return m_bUseNewlinePayload;
}

void xivres::xivstring::use_newline_payload(bool enable) {
	static_cast<void>(escape());

	m_parsed.clear();
	m_payloads.clear();
	m_bUseNewlinePayload = enable;
}

bool xivres::xivstring::empty() const {
	return m_escaped.empty() && m_parsed.empty();
}

std::string xivres::xivstring::repr() const {
	static_cast<void>(parse());
	std::string res;

	size_t escapeIndex = 0;
	for (const auto chr : m_parsed) {
		if (chr != StartOfText) {
			res += chr;
		} else {
			const auto& payload = m_payloads[escapeIndex++];
			res += payload.repr();
		}
	}

	return res;
}

const std::string& xivres::xivstring::parsed() const {
	static_cast<void>(parse());
	return m_parsed;
}

xivres::xivstring& xivres::xivstring::parsed(std::string s, std::vector<xivpayload> payloads) {
	verify_component_validity_or_throw(s, payloads);

	m_parsed = std::move(s);
	m_payloads = std::move(payloads);
	m_escaped.clear();
	return *this;
}

xivres::xivstring& xivres::xivstring::parsed(std::string s) {
	static_cast<void>(parse());

	verify_component_validity_or_throw(s, m_payloads);
	m_parsed = std::move(s);
	m_escaped.clear();
	return *this;
}

xivres::xivstring& xivres::xivstring::escaped(std::string e) {
	m_escaped = std::move(e);
	m_parsed.clear();
	m_payloads.clear();
	return *this;
}

const std::string& xivres::xivstring::escaped() const {
	return escape();
}

const std::vector<xivres::xivstring::xivpayload>& xivres::xivstring::payloads() const {
	static_cast<void>(parse());
	return m_payloads;
}

const std::string& xivres::xivstring::parse() const {
	if (!m_parsed.empty() || m_escaped.empty())
		return m_parsed;

	std::string parsed;
	std::vector<xivpayload> payloads;
	parsed.reserve(m_escaped.size());

	std::string_view remaining(m_escaped);
	while (!remaining.empty()) {
		if (remaining[0] == StartOfText) {
			if (remaining.size() < 3)
				throw std::invalid_argument("STX occurred but there are less than 3 remaining bytes");
			remaining = remaining.substr(1);

			payloads.emplace_back(remaining);
			remaining = remaining.substr(payloads.back().size());

			if (remaining.empty() || remaining[0] != EndOfText)
				throw std::invalid_argument("ETX not found");
			remaining = remaining.substr(1);

			if (m_bUseNewlinePayload && payloads.back().type() == xivpayload_type::NewLine) {
				parsed.push_back('\r');
				payloads.pop_back();
			} else {
				parsed.push_back(StartOfText);
			}
		} else {
			parsed.push_back(remaining.front());
			remaining = remaining.substr(1);
		}
	}

	m_parsed = std::move(parsed);
	m_payloads = std::move(payloads);
	return m_parsed;
}

const std::string& xivres::xivstring::escape() const {
	if (!m_escaped.empty() || m_parsed.empty())
		return m_escaped;

	size_t reserveSize = m_parsed.size();
	for (const auto& payload : m_payloads) {
		reserveSize += size_t()
			+ 1 // STX
			+ 1 // payload type
			+ 4 // payload length
			+ payload.size() // payload
			+ 1; // ETX
	}

	std::string res;
	res.reserve(reserveSize);

	size_t escapeIndex = 0;
	for (const auto chr : m_parsed) {
		if (chr == '\r' && m_bUseNewlinePayload) {
			res += "\x02\x10\x01\x03"; // STX, SeExpressionUint32(PayloadType(NewLine)), SeExpressionUint32(0), ETX
		} else if (chr != StartOfText) {
			res += chr;
		} else {
			const auto& payload = m_payloads[escapeIndex++];
			res += StartOfText;
			res.push_back(static_cast<char>(payload.type()));
			xivexpr_uint32 lengthExpr(static_cast<uint32_t>(payload.size()));
			res.resize(res.size() + lengthExpr.size());
			static_cast<void>(lengthExpr.encode(std::span(res).subspan(res.size() - lengthExpr.size())));
			res += payload.escaped();
			res += EndOfText;
		}
	}

	m_escaped = std::move(res);
	return m_escaped;
}

void xivres::xivstring::verify_component_validity_or_throw(const std::string& parsed, const std::vector<xivpayload>& components) const {
	const auto cnt = static_cast<size_t>(std::ranges::count(parsed, StartOfText));
	if (cnt != components.size())
		throw std::invalid_argument(std::format("number of sentinel characters({}) != expected number of sentinel characters({})", cnt, components.size()));
}

xivres::xivstring::xivexpr_uint32::xivexpr_uint32(std::string_view s) {
	const auto data = util::span_cast<uint8_t>(s);
	if (0x00 < data[0] && data[0] < 0xD0) {
		m_value = data[0] - 1;
	} else if (0xF0 <= data[0] && data[0] <= 0xFE) {
		const auto marker = data[0] + 1;
		uint32_t res = 0, offset = 0;
		if (marker & 8) res |= static_cast<uint8_t>(s[++offset]) << 24;
		if (marker & 4) res |= static_cast<uint8_t>(s[++offset]) << 16;
		if (marker & 2) res |= static_cast<uint8_t>(s[++offset]) << 8;
		if (marker & 1) res |= static_cast<uint8_t>(s[++offset]) << 0;
		m_value = res;
	} else {
		throw std::invalid_argument("Not an uint32 expression");
	}
}

size_t xivres::xivstring::xivexpr_uint32::size() const {
	if (m_value < 0xCF)
		return 1;
	return size_t{1}
		+ !!(m_value & 0xFF000000)
		+ !!(m_value & 0x00FF0000)
		+ !!(m_value & 0x0000FF00)
		+ !!(m_value & 0x000000FF);
}

size_t xivres::xivstring::xivexpr_uint32::encode(std::span<char> s) const {
	const auto res = util::span_cast<uint8_t>(s);
	if (m_value < 0xCF) {
		res[0] = m_value + 1;
		return 1;
	} else {
		res[0] = 0xF0;
		size_t offset = 1;
		if (const auto v = (0xFF & (m_value >> 24))) res[0] |= 8, res[offset++] = v;
		if (const auto v = (0xFF & (m_value >> 16))) res[0] |= 4, res[offset++] = v;
		if (const auto v = (0xFF & (m_value >> 8))) res[0] |= 2, res[offset++] = v;
		if (const auto v = (0xFF & (m_value >> 0))) res[0] |= 1, res[offset++] = v;
		res[0] -= 1;
		return offset;
	}
}

std::string xivres::xivstring::xivexpr_uint32::repr() const {
	return std::format("<expr type=\"uint32\">{}</expr>", m_value);
}

xivres::xivstring::xivexpr_string::xivexpr_string(std::string_view s) {
	if (s.front() != static_cast<char>(xivexpr_type::XivString))
		throw std::invalid_argument("Not a xivstring expression");
	const xivexpr_uint32 length(s.substr(1));
	m_value.escaped(std::string(s.substr(1 + length.size(), *length)));
}

size_t xivres::xivstring::xivexpr_string::size() const {
	const xivexpr_uint32 length(static_cast<uint32_t>(m_value.escaped().size()));
	return 1 + length.size() + *length;
}

size_t xivres::xivstring::xivexpr_string::encode(std::span<char> s) const {
	const auto original = s;
	s.front() = static_cast<char>(xivexpr_type::XivString);
	s = s.subspan(1);

	const xivexpr_uint32 length(m_value.escaped());
	s = s.subspan(length.encode(s));

	static_cast<void>(std::ranges::copy(m_value.escaped(), s.begin()));
	return original.size() - s.size();
}

std::string xivres::xivstring::xivexpr_string::repr() const {
	return std::format("<expr type=\"string\">{}</expr>", m_value.repr());
}

xivres::xivstring::xivexpr_param::xivexpr_param(std::string_view s) {
	m_type = static_cast<xivexpr_type>(s.front());
	if (m_type != xivexpr_type::Xec && (static_cast<uint8_t>(m_type) < 0xd0 || static_cast<uint8_t>(m_type) > 0xdf))
		throw std::invalid_argument("Not a parameter expression");
}

size_t xivres::xivstring::xivexpr_param::size() const {
	return 1;
}

size_t xivres::xivstring::xivexpr_param::encode(std::span<char> s) const {
	s.front() = static_cast<char>(m_type);
	return 1;
}

std::string xivres::xivstring::xivexpr_param::repr() const {
	switch (m_type) {
		case xivexpr_type::Hour: return "<expr type=\"Param\">hour</expr>";
		case xivexpr_type::Weekday: return "<expr type=\"Param\">weekday</expr>";
		case xivexpr_type::Xd0:
		case xivexpr_type::Xd1:
		case xivexpr_type::Xd2:
		case xivexpr_type::Xd3:
		case xivexpr_type::Xd4:
		case xivexpr_type::Xd5:
		case xivexpr_type::Xd6:
		case xivexpr_type::Xd7:
		case xivexpr_type::Xd8:
		case xivexpr_type::Xd9:
		case xivexpr_type::Xda:
		case xivexpr_type::Xdc:
		case xivexpr_type::Xde:
		case xivexpr_type::Xdf:
		case xivexpr_type::Xec:
			return std::format("<expr type=\"Param\">unknown:0x{:02}</expr>", static_cast<int>(m_type));
		default:
			return std::format("<expr type=\"Param\">invalid:0x{:02}</expr>", static_cast<int>(m_type));
	}
}

xivres::xivstring::xivexpr_unary::xivexpr_unary(std::string_view s) {
	m_type = static_cast<xivexpr_type>(s.front());
	if (static_cast<uint8_t>(m_type) < 0xe8 || static_cast<uint8_t>(m_type) > 0xeb)
		throw std::invalid_argument("Not an unary expression");

	m_operand = parse(s.substr(1));
}

xivres::xivstring::xivexpr_unary::xivexpr_unary(const xivexpr_unary& r)
	: m_type(r.m_type)
	, m_operand(r.m_operand ? r.m_operand->clone() : nullptr) {
}

xivres::xivstring::xivexpr_unary& xivres::xivstring::xivexpr_unary::operator=(const xivexpr_unary& r) {
	if (this == &r)
		return *this;

	m_type = r.m_type;
	m_operand = r.m_operand ? r.m_operand->clone() : nullptr;
	return *this;
}

size_t xivres::xivstring::xivexpr_unary::size() const {
	return 1 + m_operand->size();
}

size_t xivres::xivstring::xivexpr_unary::encode(std::span<char> s) const {
	s.front() = static_cast<char>(m_type);
	return 1 + m_operand->encode(s.subspan(1));
}

std::string xivres::xivstring::xivexpr_unary::repr() const {
	std::string res;
	switch (m_type) {
		case xivexpr_type::IntegerParameter:
			res = "<expr type=\"IntParam\">";
			break;
		case xivexpr_type::PlayerParameter:
			res = "<expr type=\"PlayerParam\">";
			break;
		case xivexpr_type::StringParameter:
			res = "<expr type=\"StringParam\">";
			break;
		case xivexpr_type::ObjectParameter:
			res = "<expr type=\"ObjectParam\">";
			break;
		default:
			res = std::format("<expr type=\"InvalidParam:0x{:02}\">", static_cast<int>(m_type));
	}
	res += m_operand->repr();
	res += "</expr>";
	return res;
}

xivres::xivstring::xivexpr_binary::xivexpr_binary(std::string_view s) {
	m_type = static_cast<xivexpr_type>(s.front());
	if (static_cast<uint8_t>(m_type) < 0xe0 || static_cast<uint8_t>(m_type) > 0xe5)
		throw std::invalid_argument("Not an unary expression");

	m_operand1 = parse(s.substr(1));
	m_operand2 = parse(s.substr(1 + m_operand1->size()));
}

xivres::xivstring::xivexpr_binary::xivexpr_binary(const xivexpr_binary& r)
	: m_type(r.m_type)
	, m_operand1(r.m_operand1 ? r.m_operand1->clone() : nullptr)
	, m_operand2(r.m_operand2 ? r.m_operand2->clone() : nullptr) {
}

xivres::xivstring::xivexpr_binary& xivres::xivstring::xivexpr_binary::operator=(const xivexpr_binary& r) {
	if (this == &r)
		return *this;

	m_type = r.m_type;
	m_operand1 = r.m_operand1 ? r.m_operand1->clone() : nullptr;
	m_operand2 = r.m_operand2 ? r.m_operand2->clone() : nullptr;
	return *this;
}

size_t xivres::xivstring::xivexpr_binary::size() const {
	return 1 + m_operand1->size() + m_operand2->size();
}

size_t xivres::xivstring::xivexpr_binary::encode(std::span<char> s) const {
	s.front() = static_cast<char>(m_type);

	size_t len = 1;
	len += m_operand1->encode(s.subspan(len));
	len += m_operand2->encode(s.subspan(len));
	return len;
}

std::string xivres::xivstring::xivexpr_binary::repr() const {
	std::string res;
	switch (m_type) {
		case xivexpr_type::GreaterThanOrEqualTo:
			res = "<expr type=\"gte\">";
			break;
		case xivexpr_type::GreaterThan:
			res = "<expr type=\"gt\">";
			break;
		case xivexpr_type::LessThanOrEqualTo:
			res = "<expr type=\"lte\">";
			break;
		case xivexpr_type::LessThan:
			res = "<expr type=\"lt\">";
			break;
		case xivexpr_type::Equal:
			res = "<expr type=\"eq\">";
			break;
		case xivexpr_type::NotEqual:
			res = "<expr type=\"ne\">";
			break;
		default:
			res = std::format("<expr type=\"InvalidBinary:0x{:02}\">", static_cast<int>(m_type));
	}
	res += m_operand1->repr();
	res += m_operand2->repr();
	res += "</expr>";
	return res;
}
