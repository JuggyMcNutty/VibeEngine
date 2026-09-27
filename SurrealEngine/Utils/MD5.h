#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

// MD5 (RFC 1321), as the original's appMD5Init/Update/Final: a client's
// world stats checksum is one (dx-reverse-info/network.md, joining).
class MD5
{
public:
	MD5();
	void Update(const void* data, size_t size);
	void Final(uint8_t digest[16]);

	// The digest as 32 lowercase hex digits.
	static std::string Hex(const uint8_t digest[16]);

private:
	void Transform(const uint8_t block[64]);

	uint32_t State[4];
	uint64_t Count = 0; // bytes
	uint8_t Buffer[64];
};
