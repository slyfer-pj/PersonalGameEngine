#include <d3d11.h>
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/TextureView.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

DXGI_FORMAT TextureLocalToD3D11(eTextureFormat format)
{
	switch (format)
	{
	case eTextureFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case eTextureFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case eTextureFormat::R24G8_TYPELESS: return DXGI_FORMAT_R24G8_TYPELESS;
	default: ERROR_AND_DIE("Unsupported format");
	}
}

DXGI_FORMAT GetDXColorFormat(eTextureFormat format)
{
	switch (format)
	{
	case eTextureFormat::R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	default: return TextureLocalToD3D11(format);
		break;
	}
}

Texture::~Texture()
{
	DX_SAFE_RELEASE(m_texture);

	for (int i = 0; i < m_views.size(); i++)
	{
		delete m_views[i];
	}

	m_views.clear();
}

TextureView* Texture::GetShaderResourceView()
{
	TextureViewInfo info;
	info.m_type = TEXTURE_BIND_SHADER_RESOURCE_BIT;
	return FindOrCreateView(info);
}

TextureView* Texture::GetDepthStencilView()
{
	TextureViewInfo info;
	info.m_type = TEXTURE_BIND_DEPTH_STENCIL_BIT;
	return FindOrCreateView(info);
}

TextureView* Texture::GetRenderTargetView()
{
	TextureViewInfo info;
	info.m_type = TEXTURE_BIND_RENDER_TARGET_BIT;
	return FindOrCreateView(info);
}

TextureView* Texture::FindOrCreateView(const TextureViewInfo& info)
{
	for (int i = 0; i < m_views.size(); i++)
	{
		if (m_views[i]->m_info == info)
			return m_views[i];
	}

	TextureView* view = new TextureView();
	view->m_handle = nullptr;
	view->m_source = this;
	m_texture->AddRef();
	view->m_sourceHandle = m_texture;
	view->m_info = info;

	ID3D11Device* device = m_owner->GetDevice();

	switch (info.m_type)
	{
	case TEXTURE_BIND_SHADER_RESOURCE_BIT:
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = GetDXColorFormat(m_format);

		switch (m_textureType)
		{
		case eTextureType::TEXTURE_2D:
		{
			desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MostDetailedMip = 0;
			desc.Texture2D.MipLevels = 1;
			break;
		}
		case eTextureType::TEXTURE_ARRAY_2D:
		{
			desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			desc.Texture2DArray.ArraySize = m_arraySize;
			desc.Texture2DArray.FirstArraySlice = 0;
			desc.Texture2DArray.MipLevels = 1;
			desc.Texture2DArray.MostDetailedMip = 0;
			break;
		}
		default:
		{
			ERROR_AND_DIE("Unsupported texture type");
			break;
		}
		}
		device->CreateShaderResourceView(m_texture, &desc, &(view->m_srv));
		break;
	}
	case TEXTURE_BIND_RENDER_TARGET_BIT:
	{
		device->CreateRenderTargetView(m_texture, nullptr, &(view->m_rtv));
		break;
	}
	case TEXTURE_BIND_DEPTH_STENCIL_BIT:
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Flags = 0;
		desc.Texture2D.MipSlice = 0;

		device->CreateDepthStencilView(m_texture, &desc, &(view->m_dsv));
		break;
	}
	default:
	{
		ERROR_AND_DIE("Unhandled view type");
	}
	}

	m_views.push_back(view);
	return view;
}
