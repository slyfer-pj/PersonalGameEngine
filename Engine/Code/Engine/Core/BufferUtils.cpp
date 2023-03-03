#include "Engine/Core/BufferUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

EndianMode GetPlatformEndianess()
{
	unsigned int testNumber = 0x12345678;
	char* singleByte = reinterpret_cast<char*>(&testNumber);
	if (*singleByte == 0x78)
		return EndianMode::LITTLE;
	else
		return EndianMode::BIG;
}

BufferParser::BufferParser(const void* buffer, size_t size)
	:m_readOrigin((uint8_t*)(buffer)), m_readHead(m_readOrigin), m_bufferSize(size), m_platformEndianess(GetPlatformEndianess())
{
	m_endianess = m_platformEndianess;
}

BufferParser::BufferParser(const std::vector<uint8_t>& buffer)
	: m_readOrigin((uint8_t*)(buffer.data())), m_readHead(m_readOrigin), m_bufferSize(buffer.size()), m_platformEndianess(GetPlatformEndianess())
{
	m_endianess = m_platformEndianess;
}

void BufferParser::SetEndianess(EndianMode mode)
{
	m_endianess = mode;
}

void BufferParser::ForwardReadHead(size_t offset)
{
	m_readHead += offset;
}

void BufferParser::MoveReadHeadToAbsoluteOffsetFromOrigin(size_t offset)
{
	m_readHead = m_readOrigin;
	ForwardReadHead(offset);
}

size_t BufferParser::GetReadHeadOffsetFromOrigin() const
{
	return m_readHead - m_readOrigin;
}

uint8_t BufferParser::ReadByte()
{
	size_t readSize = sizeof(uint8_t);
	if (IsReadWithinBounds(readSize))
	{
		uint8_t byteRead = *m_readHead;
		m_readHead += readSize;
		return byteRead;
	}

	return 0;
}

char BufferParser::ReadChar()
{
	size_t readSize = sizeof(char);
	if (IsReadWithinBounds(readSize))
	{
		char charRead = (char)*m_readHead;
		m_readHead += readSize;
		return charRead;
	}

	return 0;
}

bool BufferParser::ReadBool()
{
	size_t readSize = sizeof(bool);
	if (IsReadWithinBounds(readSize))
	{
		bool boolRead = (bool)*m_readHead;
		m_readHead += readSize;
		return boolRead;
	}

	return 0;
}

uint16_t BufferParser::ReadUnsignedShort()
{
	uint16_t ushortRead = 0;
	size_t readSize = sizeof(uint16_t);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&ushortRead, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&ushortRead, readSize);
	}

	return ushortRead;
}

int16_t BufferParser::ReadShort()
{
	int16_t shortRead = 0;
	size_t readSize = sizeof(int16_t);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&shortRead, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&shortRead, readSize);
	}

	return shortRead;
}

uint32_t BufferParser::ReadUnsignedInt32()
{
	uint32_t uintRead = 0;
	size_t readSize = sizeof(uint32_t);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&uintRead, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&uintRead, readSize);
	}
	return uintRead;
}

int32_t BufferParser::ReadInt32()
{
	int32_t intRead = 0;
	size_t readSize = sizeof(int32_t);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&intRead, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&intRead, readSize);
	}
	return intRead;
}

uint64_t BufferParser::ReadUnsignedInt64()
{
	uint64_t uint64Read = 0;
	size_t readSize = sizeof(uint64_t);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&uint64Read, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&uint64Read, readSize);
	}
	return uint64Read;
}

int64_t BufferParser::ReadInt64()
{
	int64_t int64Read = 0;
	size_t readSize = sizeof(int64_t);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&int64Read, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&int64Read, readSize);
	}
	return int64Read;
}

float BufferParser::ReadFloat()
{
	float floatRead = 0;
	size_t readSize = sizeof(float);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&floatRead, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&floatRead, readSize);
	}
	return floatRead;
}

double BufferParser::ReadDouble()
{
	double doubleRead = 0;
	size_t readSize = sizeof(double);
	if (IsReadWithinBounds(readSize))
	{
		memcpy(&doubleRead, m_readHead, readSize);
		m_readHead += readSize;
		MakeEndianAligned(&doubleRead, readSize);
	}
	return doubleRead;
}

void BufferParser::ReadZeroTerminatedString(std::string& outString)
{
	char charRead = (char)*m_readHead;
	while (charRead != '\0')
	{
		outString.push_back(charRead);
		m_readHead += sizeof(char);
		charRead = (char)*m_readHead;
	}

	m_readHead++;
}

void BufferParser::ReadLengthPrecededString(std::string& outString)
{
	uint32_t strLength = ReadUnsignedInt32();
	size_t readSize = strLength;
	if (IsReadWithinBounds(readSize))
	{
		memcpy((char*)(outString.data()), m_readHead, strLength);
		m_readHead += strLength;
	}
}

Vec2 BufferParser::ReadVec2()
{
	Vec2 vec2Read = Vec2::ZERO;
	vec2Read.x = ReadFloat();
	vec2Read.y = ReadFloat();
	return vec2Read;
}

Vec3 BufferParser::ReadVec3()
{
	Vec3 vec3Read = Vec3::ZERO;
	vec3Read.x = ReadFloat();
	vec3Read.y = ReadFloat();
	vec3Read.z = ReadFloat();
	return vec3Read;
}

Vec4 BufferParser::ReadVec4()
{
	Vec4 vec4Read;
	vec4Read.x = ReadFloat();
	vec4Read.y = ReadFloat();
	vec4Read.z = ReadFloat();
	vec4Read.w = ReadFloat();
	return vec4Read;
}

IntVec2 BufferParser::ReadIntVec2()
{
	IntVec2 intvec2Read;
	intvec2Read.x = ReadInt32();
	intvec2Read.y = ReadInt32();
	return intvec2Read;
}

IntVec3 BufferParser::ReadIntVec3()
{
	IntVec3 intvec3Read;
	intvec3Read.x = ReadInt32();
	intvec3Read.y = ReadInt32();
	intvec3Read.z = ReadInt32();
	return intvec3Read;
}

Rgba8 BufferParser::ReadRgba8()
{
	Rgba8 rgbaRead = Rgba8::BLACK;
	rgbaRead.r = ReadByte();
	rgbaRead.g = ReadByte();
	rgbaRead.b = ReadByte();
	rgbaRead.a = ReadByte();
	return rgbaRead;
}

Rgba8 BufferParser::ReadRgb()
{
	Rgba8 rgbRead = Rgba8::BLACK;
	rgbRead.r = ReadByte();
	rgbRead.g = ReadByte();
	rgbRead.b = ReadByte();
	return rgbRead;
}

Vertex_PCU BufferParser::ReadVertexPCU()
{
	Vertex_PCU vertexData;
	vertexData.m_position = ReadVec3();
	vertexData.m_color = ReadRgba8();
	vertexData.m_uvTexCoords = ReadVec2();
	return vertexData;
}

void BufferParser::MakeEndianAligned(void* data, size_t size)
{
	if (m_endianess == m_platformEndianess)
		return;

	uint8_t* bytes = (uint8_t*)(data);
	size_t front = 0;
	size_t back = size - 1;
	while (front < back)
	{
		std::swap(bytes[front], bytes[back]);
		front++;
		back--;
	}
}

bool BufferParser::IsReadWithinBounds(size_t readSize) const
{
	if (readSize > (m_bufferSize - GetReadHeadOffsetFromOrigin()))
		ERROR_AND_DIE("Trying to read beyond the buffer");

	return true;
}

BufferWriter::BufferWriter(std::vector<uint8_t>& outBuffer)
	:m_buffer(outBuffer), m_platformEndianess(GetPlatformEndianess())
{
	m_endianess = m_platformEndianess;
}

void BufferWriter::SetEndianess(EndianMode mode)
{
	m_endianess = mode;
}

EndianMode BufferWriter::GetEndianess() const
{
	return m_endianess;
}

size_t BufferWriter::GetCurrentWriteHeadOffsetFromOrigin() const
{
	return m_buffer.size();
}

void BufferWriter::OverwriteUnsignedInt32AtOffset(size_t absoluteOffset, uint32_t newValue)
{
	memcpy(m_buffer.data() + absoluteOffset, &newValue, sizeof(uint32_t));
}

void BufferWriter::AppendByte(uint8_t data)
{
	m_buffer.push_back(data);
}

void BufferWriter::AppendChar(char data)
{
	m_buffer.push_back(data);
}

void BufferWriter::AppendBool(bool data)
{
	m_buffer.push_back(data);
}

void BufferWriter::AppendUnsignedShort(uint16_t data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendShort(int16_t data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendUnsignedInt32(uint32_t data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendInt32(int32_t data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendUnsignedInt64(uint64_t data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendInt64(int64_t data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendFloat(float data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendDouble(double data)
{
	size_t writeSize = sizeof(data);
	MakeEndianAligned(&data, writeSize);
	AppendRaw(&data, writeSize);
}

void BufferWriter::AppendZeroTerminatedString(const char* data, size_t size)
{
	AppendRaw(data, size);
	m_buffer.push_back('\0');
}

void BufferWriter::AppendZeroTerminatedString(const std::string& inputString)
{
	AppendZeroTerminatedString(inputString.data(), inputString.size());
}

void BufferWriter::AppendLengthPrecededString(const char* data, uint32_t size)
{
	AppendUnsignedInt32(size);
	AppendRaw(data, size);
}

void BufferWriter::AppendLengthPrecededString(const std::string& inputString)
{
	AppendLengthPrecededString(inputString.data(), static_cast<uint32_t>(inputString.size()));
}

void BufferWriter::AppendVec2(const Vec2& data)
{
	AppendFloat(data.x);
	AppendFloat(data.y);
}

void BufferWriter::AppendVec3(const Vec3& data)
{
	AppendFloat(data.x);
	AppendFloat(data.y);
	AppendFloat(data.z);
}

void BufferWriter::AppendIntVec2(const IntVec2& data)
{
	AppendInt32(data.x);
	AppendInt32(data.y);
}

void BufferWriter::AppendIntVec3(const IntVec3& data)
{
	AppendInt32(data.x);
	AppendInt32(data.y);
	AppendInt32(data.z);
}

void BufferWriter::AppendRgba8(const Rgba8& data)
{
	AppendByte(data.r);
	AppendByte(data.g);
	AppendByte(data.b);
	AppendByte(data.a);
}

void BufferWriter::AppendRgb(const Rgba8& data)
{
	AppendByte(data.r);
	AppendByte(data.g);
	AppendByte(data.b);
}

void BufferWriter::AppendVertexPCU(const Vertex_PCU& data)
{
	AppendVec3(data.m_position);
	AppendRgba8(data.m_color);
	AppendVec2(data.m_uvTexCoords);
}

void BufferWriter::MakeEndianAligned(void* data, size_t size)
{
	if (m_endianess == m_platformEndianess)
		return;

	uint8_t* bytes = (uint8_t*)(data);
	size_t front = 0;
	size_t back = size - 1;
	while (front < back)
	{
		std::swap(bytes[front], bytes[back]);
		front++;
		back--;
	}
}

void BufferWriter::AppendRaw(const void* data, size_t size)
{
	const uint8_t* dataPtr = static_cast<const uint8_t*>(data);
	m_buffer.insert(m_buffer.end(), dataPtr, dataPtr + size);
}
