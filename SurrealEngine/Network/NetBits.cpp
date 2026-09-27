
#include "Precomp.h"
#include "NetBits.h"
#include <cstring>

void NetBitWriter::WriteBit(bool bit)
{
	if (NumBits + 1 > MaxBits)
	{
		Error = true;
		return;
	}
	if ((NumBits >> 3) >= (int)Buffer.size())
		Buffer.push_back(0);
	if (bit)
		Buffer[NumBits >> 3] |= (uint8_t)(1 << (NumBits & 7));
	NumBits++;
}

void NetBitWriter::WriteInt(uint32_t value, uint32_t valueMax)
{
	uint32_t newValue = 0;
	for (uint32_t mask = 1; newValue + mask < valueMax && mask; mask <<= 1)
	{
		bool bit = (value & mask) != 0;
		WriteBit(bit);
		if (bit)
			newValue += mask;
		if (Error)
			return;
	}
}

void NetBitWriter::WriteBits(const uint8_t* data, int numBits)
{
	for (int i = 0; i < numBits && !Error; i++)
		WriteBit((data[i >> 3] >> (i & 7)) & 1);
}

void NetBitWriter::WriteInt32(int32_t value)
{
	uint8_t bytes[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
	WriteBytes(bytes, 4);
}

void NetBitWriter::WriteFloat(float value)
{
	int32_t bits;
	memcpy(&bits, &value, 4);
	WriteInt32(bits);
}

void NetBitWriter::WriteCompactIndex(int32_t value)
{
	uint32_t v = (uint32_t)(value < 0 ? -(int64_t)value : value);
	uint8_t b0 = (uint8_t)((value < 0 ? 0x80 : 0) | (v & 0x3f));
	v >>= 6;
	if (v)
		b0 |= 0x40;
	WriteByte(b0);
	while (v)
	{
		uint8_t b = (uint8_t)(v & 0x7f);
		v >>= 7;
		if (v)
			b |= 0x80;
		WriteByte(b);
	}
}

void NetBitWriter::WriteString(const std::string& text)
{
	if (text.empty())
	{
		WriteCompactIndex(0);
		return;
	}
	WriteCompactIndex((int32_t)text.size() + 1);
	WriteBytes(text.data(), (int)text.size());
	WriteByte(0);
}

void NetBitWriter::SetNumBits(int numBits)
{
	if (numBits > NumBits)
		return;
	NumBits = numBits;
	Buffer.resize((NumBits + 7) >> 3);
	if (NumBits & 7)
		Buffer.back() &= (uint8_t)((1 << (NumBits & 7)) - 1);
	Error = false;
}

/////////////////////////////////////////////////////////////////////////////

NetBitReader::NetBitReader(const uint8_t* data, int numBits) : NumBits(numBits)
{
	Buffer.assign(data, data + ((numBits + 7) >> 3));
}

bool NetBitReader::ReadBit()
{
	if (Pos >= NumBits)
	{
		Error = true;
		return false;
	}
	bool bit = (Buffer[Pos >> 3] >> (Pos & 7)) & 1;
	Pos++;
	return bit;
}

uint32_t NetBitReader::ReadInt(uint32_t valueMax)
{
	uint32_t value = 0;
	for (uint32_t mask = 1; value + mask < valueMax && mask; mask <<= 1)
	{
		if (Pos >= NumBits)
		{
			Error = true;
			break;
		}
		if ((Buffer[Pos >> 3] >> (Pos & 7)) & 1)
			value |= mask;
		Pos++;
	}
	return value;
}

void NetBitReader::ReadBits(uint8_t* dest, int numBits)
{
	memset(dest, 0, (numBits + 7) >> 3);
	if (Pos + numBits > NumBits)
	{
		Error = true;
		return;
	}
	for (int i = 0; i < numBits; i++)
	{
		if ((Buffer[Pos >> 3] >> (Pos & 7)) & 1)
			dest[i >> 3] |= (uint8_t)(1 << (i & 7));
		Pos++;
	}
}

uint8_t NetBitReader::ReadByte()
{
	uint8_t value = 0;
	ReadBits(&value, 8);
	return value;
}

int32_t NetBitReader::ReadInt32()
{
	uint8_t bytes[4];
	ReadBits(bytes, 32);
	return (int32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((uint32_t)bytes[3] << 24));
}

float NetBitReader::ReadFloat()
{
	int32_t bits = ReadInt32();
	float value;
	memcpy(&value, &bits, 4);
	return value;
}

int32_t NetBitReader::ReadCompactIndex()
{
	uint8_t b0 = ReadByte();
	int32_t value = b0 & 0x3f;
	if (b0 & 0x40)
	{
		int shift = 6;
		for (int i = 0; i < 4 && !Error; i++)
		{
			uint8_t b = ReadByte();
			value |= (int32_t)(b & 0x7f) << shift;
			shift += 7;
			if (!(b & 0x80))
				break;
		}
	}
	return (b0 & 0x80) ? -value : value;
}

std::string NetBitReader::ReadString()
{
	int32_t count = ReadCompactIndex();
	if (Error || count == 0)
		return {};
	if (count < -1024 * 1024 || count > 1024 * 1024)
	{
		Error = true;
		return {};
	}
	std::string text;
	if (count > 0)
	{
		for (int32_t i = 0; i < count && !Error; i++)
		{
			char c = (char)ReadByte();
			if (c)
				text.push_back(c);
		}
	}
	else
	{
		for (int32_t i = 0; i < -count && !Error; i++)
		{
			uint8_t lo = ReadByte();
			uint8_t hi = ReadByte();
			uint16_t c = lo | (hi << 8);
			if (c)
				text.push_back(c < 256 ? (char)c : '?');
		}
	}
	return text;
}

NetBitReader NetBitReader::ReadSub(int numBits)
{
	NetBitReader sub;
	if (numBits < 0 || Pos + numBits > NumBits)
	{
		Error = true;
		sub.Error = true;
		return sub;
	}
	sub.NumBits = numBits;
	sub.Buffer.resize((numBits + 7) >> 3);
	ReadBits(sub.Buffer.data(), numBits);
	return sub;
}
