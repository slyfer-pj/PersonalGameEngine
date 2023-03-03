#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include <string>

class Renderer;
class TextureView;
struct ID3D11Texture2D;

enum eTextureBindFlagBit : unsigned int
{
	TEXTURE_BIND_NONE = 0,
	TEXTURE_BIND_SHADER_RESOURCE_BIT = (1 << 0),
	TEXTURE_BIND_RENDER_TARGET_BIT = (1 << 1),
	TEXTURE_BIND_DEPTH_STENCIL_BIT = (1 << 2)
};
typedef unsigned int eTextureBindFlags;

enum class eTextureType
{
	TEXTURE_2D,
	TEXTURE_ARRAY_2D
};

enum class eTextureFormat : int 
{
	R8G8B8A8_UNORM,
	D24_UNORM_S8_UINT,
	R24G8_TYPELESS,
};

struct TextureCreateInfo
{
	std::string m_name;
	IntVec2 m_dimensions;
	eTextureFormat m_format = eTextureFormat::R8G8B8A8_UNORM;
	eTextureBindFlags m_bindFlags = TEXTURE_BIND_SHADER_RESOURCE_BIT;
	eMemoryHint m_memoryHint = eMemoryHint::STATIC;
	eTextureType m_textureType = eTextureType::TEXTURE_2D;
	unsigned int m_textureArraySize = 1;
	const void* m_initialData = nullptr;
	size_t m_initialDataByteSize = 0;
	ID3D11Texture2D* m_handle = nullptr;
};

struct TextureViewInfo
{
	eTextureBindFlagBit m_type;

	inline bool operator==(const TextureViewInfo& other) const 
	{
		return m_type == other.m_type;
	}
};

class Texture
{
	friend class Renderer; // Only the Renderer can create new Texture objects!

private:
	Texture() = default; // can't instantiate directly; must ask Renderer to do it for you
	Texture(Texture const& copy) = delete; // No copying allowed!  This represents GPU memory.
	~Texture();

public:
	IntVec2	GetDimensions() const { return m_dimensions; }
	std::string const& GetImageFilePath() const { return m_name; }
	TextureView* GetShaderResourceView();
	TextureView* GetDepthStencilView();
	TextureView* GetRenderTargetView();
	TextureView* FindOrCreateView(const TextureViewInfo& info);

protected:
	Renderer* m_owner = nullptr;
	std::string	m_name;
	IntVec2	m_dimensions;
	eTextureFormat m_format = eTextureFormat::R8G8B8A8_UNORM;
	ID3D11Texture2D* m_texture = nullptr;
	eTextureBindFlags m_allowedBinds = TEXTURE_BIND_NONE;
	eTextureType m_textureType = eTextureType::TEXTURE_2D;
	unsigned int m_arraySize = 1;
	std::vector<TextureView*> m_views;
};