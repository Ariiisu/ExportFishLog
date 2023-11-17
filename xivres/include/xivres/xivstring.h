#ifndef XIVRES_SESTRING_H_
#define XIVRES_SESTRING_H_

#include <cstdint>
#include <format>
#include <mutex>
#include <ranges>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xivres {
	class xivstring {
	public:
		enum class xivexpr_type : uint8_t {
			// https://github.com/xivapi/SaintCoinach/blob/36e9d613f4bcc45b173959eed3f7b5549fd6f540/SaintCoinach/Text/DecodeExpressionType.cs

			// Predefined parameters
			Xd0 = 0xd0,
			Xd1 = 0xd1,
			Xd2 = 0xd2,
			Xd3 = 0xd3,
			Xd4 = 0xd4,
			Xd5 = 0xd5,
			Xd6 = 0xd6,
			Xd7 = 0xd7,
			Xd8 = 0xd8,
			Xd9 = 0xd9,
			Xda = 0xda,
			Hour = 0xdb,  // 0 ~ 23
			Xdc = 0xdc,
			Weekday = 0xdd,  // 1(sunday) ~ 7(saturday)
			Xde = 0xde,
			Xdf = 0xdf,

			// Binary expressions
			GreaterThanOrEqualTo = 0xe0,
			GreaterThan = 0xe1,
			LessThanOrEqualTo = 0xe2,
			LessThan = 0xe3,
			Equal = 0xe4,
			NotEqual = 0xe5,
			// Xe6 = 0xe6,
			// Xe7 = 0xe7,

			// Unary expressions
			IntegerParameter = 0xe8,
			PlayerParameter = 0xe9,
			StringParameter = 0xea,
			ObjectParameter = 0xeb,

			// Alone - what is this?
			Xec = 0xec,
			// Xed = 0xed,
			// Xee = 0xee,
			// Xef = 0xef,

			XivString = 0xff,  // Followed by length (including length) and data
		};

		enum class xivpayload_type : uint8_t {
			Unset = 0x00,
			ResetTime = 0x06,
			Time = 0x07,
			If = 0x08,
			Switch = 0x09,
			ActorFullName = 0x0a,
			IfEquals = 0x0c,
			IfJosa = 0x0d,  // eun/neun, i/ga, or eul/reul
			IfJosaro = 0x0e,  // ro/euro
			IfPlayer = 0x0f,  // "You are"/"Someone Else is"
			NewLine = 0x10,
			BitmapFontIcon = 0x12,
			ColorFill = 0x13,
			ColorBorder = 0x14,
			SoftHyphen = 0x16,
			DialoguePageSeparator = 0x17,  // probably
			Italics = 0x1a,
			Indent = 0x1d,
			Icon2 = 0x1e,
			Hyphen = 0x1f,
			Value = 0x20,
			Format = 0x22,
			TwoDigitValue = 0x24,  // "{:02}"
			SheetReference = 0x28,
			Highlight = 0x29,
			Link = 0x2b,
			Split = 0x2c,
			Placeholder = 0x2e,
			SheetReferenceJa = 0x30,
			SheetReferenceEn = 0x31,
			SheetReferenceDe = 0x32,
			SheetReferenceFr = 0x33,
			InstanceContent = 0x40,
			UiColorFill = 0x48,
			UiColorBorder = 0x49,
			ZeroPaddedValue = 0x50,
			OrdinalValue = 0x51,  // "1st", "2nd", "3rd", ...

			// Following values exist in 0a0000
			X18 = 0x19,
			X1a = 0x1b, // Used only in QuickChatTransient
			X1b = 0x1c, // Used only in QuickChatTransient
			X25 = 0x26, // Used only in Addon; probably some sort of value with preset format (for int)
			X2c = 0x2d, // Used only in Addon; probably some sort of value with preset format (for string)
			X2e = 0x2f, // More formatter?
			X5f = 0x60, // Takes two values
			X60 = 0x61, // More formatter?
		};

		class xivexpr {
		public:
			xivexpr() = default;
			xivexpr(xivexpr&&) = default;
			xivexpr(const xivexpr&) = default;
			xivexpr& operator=(xivexpr&&) = default;
			xivexpr& operator=(const xivexpr&) = default;
			virtual ~xivexpr() = default;

			[[nodiscard]] virtual size_t size() const = 0;

			[[nodiscard]] virtual size_t encode(std::span<char> s) const = 0;

			[[nodiscard]] virtual xivexpr_type type() const = 0;

			[[nodiscard]] virtual std::unique_ptr<xivexpr> clone() const = 0;

			[[nodiscard]] static std::unique_ptr<xivexpr> parse(std::string_view s);

			[[nodiscard]] virtual std::string repr() const = 0;
		};

		class xivexpr_uint32;
		class xivexpr_string;
		class xivexpr_unary;
		class xivexpr_binary;
		class xivexpr_param;

		class xivpayload {
		protected:
			xivpayload_type m_type;
			mutable std::string m_escaped;
			mutable std::vector<std::unique_ptr<xivexpr>> m_expressions;

		public:
			xivpayload(std::string_view s);
			xivpayload(xivpayload_type payloadType) : m_type(payloadType) {}
			xivpayload(const xivpayload& r);
			xivpayload(xivpayload&& r) = default;
			xivpayload& operator=(const xivpayload& r);
			xivpayload& operator=(xivpayload&& r) = default;
			~xivpayload() = default;

			std::string repr() const;

			size_t size() const { return escape().size(); }
			void type(xivpayload_type t) { m_type = t; }
			xivpayload_type type() const { return m_type; }
			const std::string& escaped() const { return escape(); }
			const std::vector<std::unique_ptr<xivexpr>>& params() const { return m_expressions; }

		private:
			const std::vector<std::unique_ptr<xivexpr>>& parse() const;
			const std::string& escape() const;
		};

		static constexpr auto StartOfText = '\x02';
		static constexpr auto EndOfText = '\x03';

	private:
		bool m_bUseNewlinePayload = false;
		mutable std::string m_escaped;
		mutable std::string m_parsed;
		mutable std::vector<xivpayload> m_payloads;

	public:
		xivstring();
		xivstring(xivstring&&) noexcept;
		xivstring(const xivstring&);
		xivstring(std::string s);
		xivstring(std::string s, std::vector<xivpayload> payloads);
		xivstring& operator=(xivstring&&) noexcept;
		xivstring& operator=(const xivstring&);
		~xivstring() = default;

		template<typename T>
		static bool is_valid(T ptr) {
			try {
				void(xivstring(ptr).parsed());
				return true;
			} catch (...) {
				return false;
			}
		}

		bool operator==(const xivstring& r) const { return escaped() == r.escaped(); }
		bool operator!=(const xivstring& r) const { return escaped() != r.escaped(); }
		bool operator<(const xivstring& r) const { return escaped() < r.escaped(); }
		bool operator<=(const xivstring& r) const { return escaped() <= r.escaped(); }
		bool operator>(const xivstring& r) const { return escaped() > r.escaped(); }
		bool operator>=(const xivstring& r) const { return escaped() >= r.escaped(); }

		bool use_newline_payload() const;
		void use_newline_payload(bool enable);

		bool empty() const;

		std::string repr() const;

		[[nodiscard]] const std::string& parsed() const;
		xivstring& parsed(std::string s, std::vector<xivpayload> payloads);
		xivstring& parsed(std::string s);

		[[nodiscard]] const std::string& escaped() const;
		xivstring& escaped(std::string e);

		[[nodiscard]] const std::vector<xivpayload>& payloads() const;

	private:
		const std::string& parse() const;
		const std::string& escape() const;
		void verify_component_validity_or_throw(const std::string& parsed, const std::vector<xivpayload>& components) const;
	};

	class xivstring::xivexpr_uint32 : public xivexpr {
		uint32_t m_value = 0;

	public:
		xivexpr_uint32() = default;
		xivexpr_uint32(xivexpr_uint32&& r) = default;
		xivexpr_uint32(const xivexpr_uint32& r) = default;
		xivexpr_uint32& operator =(xivexpr_uint32&& r) = default;
		xivexpr_uint32& operator =(const xivexpr_uint32& r) = default;
		~xivexpr_uint32() override = default;

		xivexpr_uint32(std::string_view s);
		xivexpr_uint32(const std::string& s) : xivexpr_uint32(std::string_view(s)) {}

		[[nodiscard]] xivexpr_type type() const override { return static_cast<xivexpr_type>(m_value & 0xFF); }
		xivexpr_uint32(uint32_t value) : m_value(value) {}
		xivexpr_uint32& operator =(uint32_t r) { m_value = r; return *this; }
		operator uint32_t() const { return m_value; }
		uint32_t& operator*() { return m_value; }
		uint32_t operator*() const { return m_value; }
		[[nodiscard]] uint32_t value() const { return m_value; }

		[[nodiscard]] size_t size() const override;
		[[nodiscard]] size_t encode(std::span<char> s) const override;

		[[nodiscard]] std::unique_ptr<xivexpr> clone() const override { return std::make_unique<std::remove_cvref_t<decltype(*this)>>(*this); }
		[[nodiscard]] std::string repr() const override;
	};

	class xivstring::xivexpr_string : public xivexpr {
		xivstring m_value;

	public:
		xivexpr_string() = default;
		xivexpr_string(xivexpr_string&&) = default;
		xivexpr_string(const xivexpr_string&) = default;
		xivexpr_string& operator=(xivexpr_string&&) = default;
		xivexpr_string& operator=(const xivexpr_string&) = default;
		~xivexpr_string() override = default;

		xivexpr_string(std::string_view s);
		xivexpr_string(const std::string& s) : xivexpr_string(std::string_view(s)) {}

		xivexpr_type type() const override { return xivexpr_type::XivString; }
		xivexpr_string(xivstring s) : m_value(std::move(s)) {}
		xivexpr_string& operator=(xivstring s) { m_value = std::move(s); return *this; }
		operator xivstring() const { return m_value; }
		xivstring& operator*() { return m_value; }
		xivstring operator*() const { return m_value; }
		xivstring value() const { return m_value; }

		size_t size() const override;
		size_t encode(std::span<char> s) const override;

		std::unique_ptr<xivexpr> clone() const override { return std::make_unique<std::remove_cvref_t<decltype(*this)>>(*this); }
		[[nodiscard]] std::string repr() const override;
	};

	class xivstring::xivexpr_param : public xivexpr {
		xivexpr_type m_type{};

	public:
		xivexpr_param() = default;
		xivexpr_param(xivexpr_type t) : m_type(t) {}
		xivexpr_param(xivexpr_param&&) = default;
		xivexpr_param(const xivexpr_param&) = default;
		xivexpr_param& operator=(xivexpr_param&&) = default;
		xivexpr_param& operator=(const xivexpr_param&) = default;
		~xivexpr_param() override = default;

		xivexpr_param(std::string_view s);
		xivexpr_param(const std::string& s) : xivexpr_param(std::string_view(s)) {}

		[[nodiscard]] xivexpr_type type() const override { return m_type; }
		void type(xivexpr_type v) { m_type = v; }

		[[nodiscard]] size_t size() const override;
		[[nodiscard]] size_t encode(std::span<char> s) const override;

		[[nodiscard]] std::unique_ptr<xivexpr> clone() const override { return std::make_unique<std::remove_cvref_t<decltype(*this)>>(*this); }
		[[nodiscard]] std::string repr() const override;
	};

	class xivstring::xivexpr_unary : public xivexpr {
		xivexpr_type m_type{};
		std::unique_ptr<xivexpr> m_operand;

	public:
		xivexpr_unary() = default;
		xivexpr_unary(xivexpr_type type) : m_type(type) {}
		xivexpr_unary(xivexpr_unary&&) = default;
		xivexpr_unary(const xivexpr_unary&);
		xivexpr_unary& operator=(xivexpr_unary&&) = default;
		xivexpr_unary& operator=(const xivexpr_unary&);
		~xivexpr_unary() override = default;

		xivexpr_unary(std::string_view s);
		xivexpr_unary(const std::string& s) : xivexpr_unary(std::string_view(s)) {}

		[[nodiscard]] xivexpr_type type() const override { return m_type; }
		void type(xivexpr_type v) { m_type = v; }
		template<typename T, typename = std::enable_if_t<std::is_base_of_v<xivexpr, T>>>
		T* operand() const { return static_cast<T*>(m_operand.get()); }
		void operand(std::unique_ptr<xivexpr> value) { m_operand = std::move(value); }

		[[nodiscard]] size_t size() const override;
		[[nodiscard]] size_t encode(std::span<char> s) const override;

		[[nodiscard]] std::unique_ptr<xivexpr> clone() const override { return std::make_unique<std::remove_cvref_t<decltype(*this)>>(*this); }
		[[nodiscard]] std::string repr() const override;
	};

	class xivstring::xivexpr_binary : public xivexpr {
		xivexpr_type m_type{};
		std::unique_ptr<xivexpr> m_operand1;
		std::unique_ptr<xivexpr> m_operand2;

	public:
		xivexpr_binary() = default;
		xivexpr_binary(xivexpr_type type) : m_type(type) {}
		xivexpr_binary(xivexpr_binary&&) = default;
		xivexpr_binary(const xivexpr_binary&);
		xivexpr_binary& operator=(xivexpr_binary&&) = default;
		xivexpr_binary& operator=(const xivexpr_binary&);
		~xivexpr_binary() override = default;

		xivexpr_binary(std::string_view s);
		xivexpr_binary(const std::string& s) : xivexpr_binary(std::string_view(s)) {}

		[[nodiscard]] xivexpr_type type() const override { return m_type; }
		void type(xivexpr_type v) { m_type = v; }
		template<typename T, typename = std::enable_if_t<std::is_base_of_v<xivexpr, T>>>
		T* operand1() const { return static_cast<T*>(m_operand1.get()); }
		void operand1(std::unique_ptr<xivexpr> value) { m_operand1 = std::move(value); }
		template<typename T, typename = std::enable_if_t<std::is_base_of_v<xivexpr, T>>>
		T* operand2() const { return static_cast<T*>(m_operand2.get()); }
		void operand2(std::unique_ptr<xivexpr> value) { m_operand2 = std::move(value); }

		[[nodiscard]] size_t size() const override;
		[[nodiscard]] size_t encode(std::span<char> s) const override;

		[[nodiscard]] std::unique_ptr<xivexpr> clone() const override { return std::make_unique<std::remove_cvref_t<decltype(*this)>>(*this); }
		[[nodiscard]] std::string repr() const override;
	};
}

#endif
