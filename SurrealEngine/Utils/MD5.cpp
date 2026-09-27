
#include "Precomp.h"
#include "MD5.h"
#include <cstring>

namespace
{
	inline uint32_t RotateLeft(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

	const uint32_t K[64] =
	{
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	};

	const int R[64] =
	{
		7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
	};
}

MD5::MD5()
{
	State[0] = 0x67452301;
	State[1] = 0xefcdab89;
	State[2] = 0x98badcfe;
	State[3] = 0x10325476;
}

void MD5::Transform(const uint8_t block[64])
{
	uint32_t m[16];
	for (int i = 0; i < 16; i++)
		m[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) | ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);

	uint32_t a = State[0], b = State[1], c = State[2], d = State[3];
	for (int i = 0; i < 64; i++)
	{
		uint32_t f;
		int g;
		if (i < 16) { f = (b & c) | (~b & d); g = i; }
		else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
		else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
		else { f = c ^ (b | ~d); g = (7 * i) & 15; }
		uint32_t next = d;
		d = c;
		c = b;
		b = b + RotateLeft(a + f + K[i] + m[g], R[i]);
		a = next;
	}
	State[0] += a;
	State[1] += b;
	State[2] += c;
	State[3] += d;
}

void MD5::Update(const void* data, size_t size)
{
	const uint8_t* bytes = static_cast<const uint8_t*>(data);
	size_t used = (size_t)(Count & 63);
	Count += size;
	if (used)
	{
		size_t take = std::min(size, 64 - used);
		memcpy(Buffer + used, bytes, take);
		bytes += take;
		size -= take;
		if (used + take < 64)
			return;
		Transform(Buffer);
	}
	while (size >= 64)
	{
		Transform(bytes);
		bytes += 64;
		size -= 64;
	}
	if (size)
		memcpy(Buffer, bytes, size);
}

void MD5::Final(uint8_t digest[16])
{
	uint64_t bits = Count * 8;
	uint8_t pad[72] = { 0x80 };
	size_t used = (size_t)(Count & 63);
	size_t padSize = used < 56 ? 56 - used : 120 - used;
	Update(pad, padSize);
	uint8_t length[8];
	for (int i = 0; i < 8; i++)
		length[i] = (uint8_t)(bits >> (i * 8));
	Update(length, 8);
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			digest[i * 4 + j] = (uint8_t)(State[i] >> (j * 8));
}

std::string MD5::Hex(const uint8_t digest[16])
{
	static const char* digits = "0123456789abcdef";
	std::string text;
	for (int i = 0; i < 16; i++)
	{
		text.push_back(digits[digest[i] >> 4]);
		text.push_back(digits[digest[i] & 15]);
	}
	return text;
}
