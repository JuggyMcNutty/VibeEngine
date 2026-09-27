#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The original's bit streams (Core's FBitWriter and FBitReader): bits from
// each byte's lowest up, and a number with a known maximum in as many bits as
// that maximum needs, lowest first -- the next bit only while the value so
// far plus that bit's weight stays under the maximum.
class NetBitWriter
{
public:
	explicit NetBitWriter(int maxBits = 0x7fffffff) : MaxBits(maxBits) {}

	void WriteBit(bool bit);
	void WriteInt(uint32_t value, uint32_t valueMax);
	void WriteBits(const uint8_t* data, int numBits);
	void WriteBytes(const void* data, int numBytes) { WriteBits((const uint8_t*)data, numBytes * 8); }
	void WriteByte(uint8_t value) { WriteBytes(&value, 1); }
	void WriteInt32(int32_t value);
	void WriteFloat(float value);
	void WriteCompactIndex(int32_t value);
	// An FString: its length with the terminator as a compact index, then
	// its characters and the terminator; none for an empty string.
	void WriteString(const std::string& text);

	int GetNumBits() const { return NumBits; }
	int GetNumBytes() const { return (NumBits + 7) >> 3; }
	const uint8_t* GetData() const { return Buffer.data(); }
	bool IsError() const { return Error; }
	int GetMaxBits() const { return MaxBits; }

	// Back to an earlier length, the bits after it cleared and an overflow
	// since forgotten (Core's FBitWriterMark::Pop).
	void SetNumBits(int numBits);

private:
	std::vector<uint8_t> Buffer;
	int NumBits = 0;
	int MaxBits;
	bool Error = false;
};

class NetBitReader
{
public:
	NetBitReader() = default;
	NetBitReader(const uint8_t* data, int numBits);

	bool ReadBit();
	uint32_t ReadInt(uint32_t valueMax);
	void ReadBits(uint8_t* dest, int numBits);
	void ReadBytes(void* dest, int numBytes) { ReadBits((uint8_t*)dest, numBytes * 8); }
	uint8_t ReadByte();
	int32_t ReadInt32();
	float ReadFloat();
	int32_t ReadCompactIndex();
	std::string ReadString();

	// The next numBits as a reader of their own.
	NetBitReader ReadSub(int numBits);

	bool AtEnd() const { return Error || Pos >= NumBits; }
	bool IsError() const { return Error; }
	void SetError() { Error = true; }
	int GetPosBits() const { return Pos; }
	int GetNumBits() const { return NumBits; }

private:
	std::vector<uint8_t> Buffer;
	int NumBits = 0;
	int Pos = 0;
	bool Error = false;
};
