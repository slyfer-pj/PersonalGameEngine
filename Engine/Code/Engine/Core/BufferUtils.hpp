#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

enum class EndianMode
{
	LITTLE,
	BIG
};

class BufferParser
{
public:
	BufferParser(const void* buffer, size_t size);
	BufferParser(const std::vector<uint8_t>& buffer);
	void SetEndianess(EndianMode mode);
	void ForwardReadHead(size_t offset);
	void MoveReadHeadToAbsoluteOffsetFromOrigin(size_t offset);
	size_t GetReadHeadOffsetFromOrigin() const;

	uint8_t ReadByte();
	char ReadChar();
	bool ReadBool();
	uint16_t ReadUnsignedShort();
	int16_t ReadShort();
	uint32_t ReadUnsignedInt32();
	int32_t ReadInt32();
	uint64_t ReadUnsignedInt64();
	int64_t ReadInt64();
	float ReadFloat();
	double ReadDouble();
	void ReadZeroTerminatedString(std::string& outString);
	void ReadLengthPrecededString(std::string& outString);
	Vec2 ReadVec2();
	Vec3 ReadVec3();
	Vec4 ReadVec4();
	IntVec2 ReadIntVec2();
	IntVec3 ReadIntVec3();
	Rgba8 ReadRgba8();
	Rgba8 ReadRgb();
	Vertex_PCU ReadVertexPCU();

private:
	uint8_t* m_readOrigin = nullptr;
	uint8_t* m_readHead = nullptr;
	size_t m_bufferSize = 0;
	EndianMode m_endianess = EndianMode::LITTLE;
	EndianMode m_platformEndianess = EndianMode::LITTLE;

private:
	void MakeEndianAligned(void* data, size_t size);
	bool IsReadWithinBounds(size_t readSize) const;
};

class BufferWriter
{
public:
	BufferWriter(std::vector<uint8_t>& outBuffer);
	void SetEndianess(EndianMode mode);
	EndianMode GetEndianess() const;
	size_t GetCurrentWriteHeadOffsetFromOrigin() const;
	void OverwriteUnsignedInt32AtOffset(size_t absoluteOffset, uint32_t newValue);

	void AppendByte(uint8_t data);
	void AppendChar(char data);
	void AppendBool(bool data);
	void AppendUnsignedShort(uint16_t data);
	void AppendShort(int16_t data);
	void AppendUnsignedInt32(uint32_t data);
	void AppendInt32(int32_t data);
	void AppendUnsignedInt64(uint64_t data);
	void AppendInt64(int64_t data);
	void AppendFloat(float data);
	void AppendDouble(double data);
	void AppendZeroTerminatedString(const char* data, size_t size);
	void AppendZeroTerminatedString(const std::string& inputString);
	void AppendLengthPrecededString(const char* data, uint32_t size);
	void AppendLengthPrecededString(const std::string& inputString);
	void AppendVec2(const Vec2& data);
	void AppendVec3(const Vec3& data);
	void AppendIntVec2(const IntVec2& data);
	void AppendIntVec3(const IntVec3& data);
	void AppendRgba8(const Rgba8& data);
	void AppendRgb(const Rgba8& data);
	void AppendVertexPCU(const Vertex_PCU& data);

private:
	std::vector<uint8_t>& m_buffer;
	EndianMode m_endianess = EndianMode::LITTLE;
	EndianMode m_platformEndianess = EndianMode::LITTLE;

private:
	void MakeEndianAligned(void* data, size_t size);
	void AppendRaw(const void* data, size_t size);
};