#pragma once
#include "Engine/Renderer/Texture.hpp"

class Renderer;
class Texture;

struct ID3D11Texture2D;
struct ID3D11View;
struct ID3D11ShaderResourceView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;

class TextureView
{
	friend Texture;
	friend Renderer;

public:
	TextureView();
	~TextureView();

	TextureView(const TextureView& copy) = delete;

public:
	Texture* m_source = nullptr;
	ID3D11Texture2D* m_sourceHandle = nullptr;	//ref counting
	TextureViewInfo m_info;

	union 
	{
		ID3D11View* m_handle;
		ID3D11ShaderResourceView* m_srv;
		ID3D11RenderTargetView* m_rtv;
		ID3D11DepthStencilView* m_dsv;
	};
};

