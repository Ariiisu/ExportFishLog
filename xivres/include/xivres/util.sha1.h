/*
 *
 * TinySHA1 - a header only implementation of the SHA1 algorithm in C++. Based
 * on the implementation in boost::uuid::details.
 *
 * SHA1 Wikipedia Page: http://en.wikipedia.org/wiki/SHA-1
 *
 * Copyright (c) 2012-22 SAURAV MOHAPATRA <mohaps@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef XIVRES_INTERNAL_TINYSHA1_H_
#define XIVRES_INTERNAL_TINYSHA1_H_

#include <cstdint>
#include <span>

#include "common.h"
#include "util.h"

namespace xivres::util {
	class hash_sha1 {
	public:
		typedef uint32_t digest32_t[5];
		typedef uint8_t digest8_t[20];

	private:
		digest32_t m_digest;
		uint8_t m_block[64];
		size_t m_blockByteIndex;
		size_t m_byteCount;

	public:
		hash_sha1() {
			reset();
		}

		hash_sha1(const hash_sha1& s) {
			*this = s;
		}

		hash_sha1& operator=(const hash_sha1& s) {
			if (this == &s)
				return *this;
			memcpy(m_digest, s.m_digest, 5 * sizeof(uint32_t));
			memcpy(m_block, s.m_block, 64);
			m_blockByteIndex = s.m_blockByteIndex;
			m_byteCount = s.m_byteCount;
			return *this;
		}

		hash_sha1(hash_sha1&&) = delete;
		hash_sha1& operator=(hash_sha1&&) = delete;
		~hash_sha1() = default;

		hash_sha1& reset() {
			m_digest[0] = 0x67452301;
			m_digest[1] = 0xEFCDAB89;
			m_digest[2] = 0x98BADCFE;
			m_digest[3] = 0x10325476;
			m_digest[4] = 0xC3D2E1F0;
			m_blockByteIndex = 0;
			m_byteCount = 0;
			return *this;
		}

		hash_sha1& process_byte(uint8_t octet) {
			this->m_block[this->m_blockByteIndex++] = octet;
			++this->m_byteCount;
			if (m_blockByteIndex == 64) {
				this->m_blockByteIndex = 0;
				process_block();
			}
			return *this;
		}

		hash_sha1& process_block(const void* const start, const void* const end) {
			auto begin = static_cast<const uint8_t*>(start);
			const auto finish = static_cast<const uint8_t*>(end);
			while (begin != finish) {
				process_byte(*begin);
				begin++;
			}
			return *this;
		}

		hash_sha1& process_bytes(const void* const data, size_t len) {
			const auto block = static_cast<const uint8_t*>(data);
			process_block(block, block + len);
			return *this;
		}

		const uint32_t* get_digest(digest32_t digest) {
			size_t bitCount = this->m_byteCount * 8;
			process_byte(0x80);
			if (this->m_blockByteIndex > 56) {
				while (m_blockByteIndex != 0) {
					process_byte(0);
				}
				while (m_blockByteIndex < 56) {
					process_byte(0);
				}
			} else {
				while (m_blockByteIndex < 56) {
					process_byte(0);
				}
			}
			process_byte(0);
			process_byte(0);
			process_byte(0);
			process_byte(0);
			process_byte(static_cast<unsigned char>((bitCount >> 24) & 0xFF));
			process_byte(static_cast<unsigned char>((bitCount >> 16) & 0xFF));
			process_byte(static_cast<unsigned char>((bitCount >> 8) & 0xFF));
			process_byte(static_cast<unsigned char>((bitCount) & 0xFF));

			memcpy(digest, m_digest, 5 * sizeof(uint32_t));
			return digest;
		}

		const uint8_t* get_digest_bytes(digest8_t digest) {
			digest32_t d32;
			get_digest(d32);
			size_t di = 0;
			digest[di++] = ((d32[0] >> 24) & 0xFF);
			digest[di++] = ((d32[0] >> 16) & 0xFF);
			digest[di++] = ((d32[0] >> 8) & 0xFF);
			digest[di++] = ((d32[0]) & 0xFF);

			digest[di++] = ((d32[1] >> 24) & 0xFF);
			digest[di++] = ((d32[1] >> 16) & 0xFF);
			digest[di++] = ((d32[1] >> 8) & 0xFF);
			digest[di++] = ((d32[1]) & 0xFF);

			digest[di++] = ((d32[2] >> 24) & 0xFF);
			digest[di++] = ((d32[2] >> 16) & 0xFF);
			digest[di++] = ((d32[2] >> 8) & 0xFF);
			digest[di++] = ((d32[2]) & 0xFF);

			digest[di++] = ((d32[3] >> 24) & 0xFF);
			digest[di++] = ((d32[3] >> 16) & 0xFF);
			digest[di++] = ((d32[3] >> 8) & 0xFF);
			digest[di++] = ((d32[3]) & 0xFF);

			digest[di++] = ((d32[4] >> 24) & 0xFF);
			digest[di++] = ((d32[4] >> 16) & 0xFF);
			digest[di++] = ((d32[4] >> 8) & 0xFF);
			digest[di++] = ((d32[4]) & 0xFF);
			return digest;
		}

	private:
		void process_block() {
			uint32_t w[80];
			for (size_t i = 0; i < 16; i++) {
				w[i] = (m_block[i * 4 + 0] << 24);
				w[i] |= (m_block[i * 4 + 1] << 16);
				w[i] |= (m_block[i * 4 + 2] << 8);
				w[i] |= (m_block[i * 4 + 3]);
			}
			for (size_t i = 16; i < 80; i++) {
				w[i] = left_rotate((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);
			}

			uint32_t a = m_digest[0];
			uint32_t b = m_digest[1];
			uint32_t c = m_digest[2];
			uint32_t d = m_digest[3];
			uint32_t e = m_digest[4];

			for (std::size_t i = 0; i < 80; ++i) {
				uint32_t f;
				uint32_t k;

				if (i < 20) {
					f = (b & c) | (~b & d);
					k = 0x5A827999;
				} else if (i < 40) {
					f = b ^ c ^ d;
					k = 0x6ED9EBA1;
				} else if (i < 60) {
					f = (b & c) | (b & d) | (c & d);
					k = 0x8F1BBCDC;
				} else {
					f = b ^ c ^ d;
					k = 0xCA62C1D6;
				}
				uint32_t temp = left_rotate(a, 5) + f + e + k + w[i];
				e = d;
				d = c;
				c = left_rotate(b, 30);
				b = a;
				a = temp;
			}

			m_digest[0] += a;
			m_digest[1] += b;
			m_digest[2] += c;
			m_digest[3] += d;
			m_digest[4] += e;
		}

		static uint32_t left_rotate(uint32_t value, size_t count) {
			return (value << count) ^ (value >> (32 - count));
		}
	};
}

namespace xivres {
	struct sha1_value {
		uint8_t Value[20]{};

		bool operator==(const sha1_value& r) const {
			return memcmp(r.Value, Value, sizeof Value) == 0;
		}

		bool operator!=(const sha1_value& r) const {
			return memcmp(r.Value, Value, sizeof Value) != 0;
		}

		bool operator<(const sha1_value& r) const {
			return memcmp(r.Value, Value, sizeof Value) < 0;
		}

		bool operator<=(const sha1_value& r) const {
			return memcmp(r.Value, Value, sizeof Value) <= 0;
		}

		bool operator>(const sha1_value& r) const {
			return memcmp(r.Value, Value, sizeof Value) > 0;
		}

		bool operator>=(const sha1_value& r) const {
			return memcmp(r.Value, Value, sizeof Value) >= 0;
		}

		bool operator==(const char (&r)[20]) const {
			return memcmp(r, Value, sizeof Value) == 0;
		}

		bool operator!=(const char (&r)[20]) const {
			return memcmp(r, Value, sizeof Value) != 0;
		}

		[[nodiscard]] bool IsZero() const {
			return util::all_same_value(Value);
		}

		void set_from_ptr(const void* data, size_t size) {
			util::hash_sha1 hasher;
			hasher.process_bytes(data, size);
			hasher.get_digest_bytes(Value);
		}

		template<typename T>
		void set_from(std::span<T> data) {
			set_from_ptr(data.data(), data.size_bytes());
		}

		template<typename ...Args>
		void set_from_span(Args ...args) {
			set_from(std::span(std::forward<Args>(args)...));
		}

		void verify(const void* data, size_t size, const char* errorMessage) const {
			sha1_value t;
			t.set_from_ptr(data, size);
			if (*this != t) {
				if (!size && util::all_same_value(Value)) // all zero values can be in place of SHA-1 value of empty value
					return;
				throw bad_data_error(errorMessage);
			}
		}

		template<typename T>
		void verify(std::span<T> data, const char* errorMessage) const {
			verify(data.data(), data.size_bytes(), errorMessage);
		}
	};
}

#endif
