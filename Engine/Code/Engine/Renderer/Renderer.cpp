#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#include "ThirdParty/stb/stb_image.h"
#include "ThirdParty/ImGUI/imgui.h"
#include "ThirdParty/ImGUI/imgui_impl_dx11.h"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Renderer/DefaultShader.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Core/Vertex_PNCU.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/TextureView.hpp"
#include "Game/EngineBuildPreferences.hpp"

#ifdef ENGINE_DEBUG_RENDER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

extern Window* g_theWindow;

struct CameraConstants
{
	Mat44 projectionMatrix;
	Mat44 viewMatrix;
};

struct ModelConstants
{
	Mat44 modelMatrix;
	float modelColor[4] = { 0.f };
};

struct LightingConstants
{
	Vec3 sunDirection;
	float sunIntensity = 0.f;
	float ambientIntensity = 0.f;
	Vec3 padding;
	SpotLight spotLight;
};

const int lightingConstantBufferSlot = 1;
const int cameraConstantBufferSlot = 2;
const int modelConstantBufferSlot = 3;
const char* defaultShaderFilename = "Data/Shaders/Default";
const char* defaultShader = "Default";

DXGI_FORMAT LocalToD3D11(eTextureFormat format)
{
	switch (format)
	{
	case eTextureFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case eTextureFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case eTextureFormat::R24G8_TYPELESS: return DXGI_FORMAT_R24G8_TYPELESS;
	default: ERROR_AND_DIE("Unsupported format");
	}
}

static D3D11_USAGE LocalToD3D11(eMemoryHint hint)
{
	switch (hint)
	{
	case eMemoryHint::STATIC: return D3D11_USAGE_IMMUTABLE;
	case eMemoryHint::GPU: return D3D11_USAGE_DEFAULT;
	case eMemoryHint::DYNAMIC: return D3D11_USAGE_DYNAMIC;
	case eMemoryHint::STAGING: return D3D11_USAGE_STAGING;
	default: ERROR_AND_DIE("Unsupported format");
	}
}

static UINT LocalToD3D11BindFlags(const eTextureBindFlags bindFlags)
{
	UINT result = 0;

	if (bindFlags & TEXTURE_BIND_SHADER_RESOURCE_BIT)
	{
		result |= D3D11_BIND_SHADER_RESOURCE;
	}
	if (bindFlags & TEXTURE_BIND_RENDER_TARGET_BIT)
	{
		result |= D3D11_BIND_RENDER_TARGET;
	}
	if (bindFlags & TEXTURE_BIND_DEPTH_STENCIL_BIT)
	{
		result |= D3D11_BIND_DEPTH_STENCIL;
	}

	return result;
}

Renderer::Renderer(const RendererConfig& config)
	:m_config(config)
{
}

void Renderer::Startup()
{
#ifdef ENGINE_DEBUG_RENDER
	//get dxgi debug interface
	m_dxgiDebugLibModule = (void*)::LoadLibraryA("dxgidebug.dll");
	typedef HRESULT(WINAPI* GetDebugModuleCB)(REFIID, void**);
	((GetDebugModuleCB) ::GetProcAddress((HMODULE)m_dxgiDebugLibModule, "DXGIGetDebugInterface"))(__uuidof(IDXGIDebug), &m_dxgiDebugInterface);
#endif

	// At init, on windows
	HMODULE mod = GetModuleHandleA("renderdoc.dll");
	if (mod)
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI =
			(pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
		RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&m_rdoc_api);
	}

	//create and config dx graphics pipeline
	CreateD3DDeviceAndSwapChain();
	CreateBackBuffer();
	CreateDepthStencilTextureAndView();

	std::string shaderSource = shaderSourceString;
	if (m_config.m_defaultShader)
	{
		FileReadToString(shaderSource, m_config.m_defaultShader);
	}
	m_defaultShader = CreateShader(defaultShader, shaderSource.c_str());
	
	m_immediateVBO_PCU = CreateVertexBuffer(1, sizeof(Vertex_PCU));
	m_immediateVBO_PNCU = CreateVertexBuffer(1, sizeof(Vertex_PNCU));
	m_cameraCBO = CreateConstantBuffer(sizeof(CameraConstants));
	m_modelCBO = CreateConstantBuffer(sizeof(ModelConstants));
	m_lightCBO = CreateConstantBuffer(sizeof(LightingConstants));

	Image white1By1Image(IntVec2(2, 2), Rgba8::WHITE);
	white1By1Image.m_imageFilePath = "Default";
	m_defaultTexture = CreateTextureFromImage(white1By1Image);
	m_loadedTextures.push_back(m_defaultTexture);
	SetDebugName(m_defaultTexture->m_texture, "Default");

	DebugRenderSystemConfig debugRenderConfig;
	debugRenderConfig.m_renderer = this;
	debugRenderConfig.m_fontFilepath = g_gameConfigBlackboard.GetValue("devconsoleDefaultFont", debugRenderConfig.m_fontFilepath);
	DebugRenderSystemStartup(debugRenderConfig);

	m_ambientIntensity = 0.8f;
	m_sunIntensity = 0.4f;

}

void Renderer::BeginFrame()
{
	DebugRenderBeginFrame();
}

void Renderer::EndFrame()
{
	Sleep(0);
	DebugRenderEndFrame();
	CopyTexture(m_backBuffer, m_defaultColorTarget[m_activeColorTargetIndex]);

	//imgui render function
	TextureView* rtv = m_backBuffer->GetRenderTargetView();
	TextureView* dsv = m_depthBuffer->GetDepthStencilView();
	m_deviceContext->OMSetRenderTargets(1, &(rtv->m_rtv), (dsv->m_dsv));

#ifdef ENABLE_VSYNC
	m_swapChain->Present(1, 0);
#else
	m_swapChain->Present(0, 0);
#endif
}

void Renderer::Shutdown()
{
	for (int i = 0; i < m_loadedTextures.size(); i++)
	{
		delete m_loadedTextures[i];
		m_loadedTextures[i] = nullptr;
	}

	for (int i = 0; i < m_loadedFonts.size(); i++)
	{
		delete m_loadedFonts[i];
		m_loadedFonts[i] = nullptr;
	}

	for (int i = 0; i < m_loadedShaders.size(); i++)
	{
		delete m_loadedShaders[i];
		m_loadedShaders[i] = nullptr;
	}

	delete m_immediateVBO_PCU;
	m_immediateVBO_PCU = nullptr;
	delete m_immediateVBO_PNCU;
	m_immediateVBO_PNCU = nullptr;
	delete m_lightCBO;
	m_lightCBO = nullptr;
	delete m_cameraCBO;
	m_cameraCBO = nullptr;
	delete m_modelCBO;
	m_modelCBO = nullptr;
	DX_SAFE_RELEASE(m_device);
	DX_SAFE_RELEASE(m_deviceContext);
	DX_SAFE_RELEASE(m_swapChain);
	DX_SAFE_RELEASE(m_depthStencilState);
	DX_SAFE_RELEASE(m_rasterizerState);
	DX_SAFE_RELEASE(m_blendState);
	DX_SAFE_RELEASE(m_samplerState);
	DestroyTexture(m_backBuffer);
	DestroyTexture(m_depthBuffer);
	DestroyTexture(m_defaultColorTarget[0]);
	DestroyTexture(m_defaultColorTarget[1]);
	m_depthBuffer = nullptr;
	m_backBuffer = nullptr;
	m_defaultColorTarget[0] = nullptr;
	m_defaultColorTarget[1] = nullptr;

#ifdef ENGINE_DEBUG_RENDER
	((IDXGIDebug*)m_dxgiDebugInterface)->ReportLiveObjects(DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
	((IDXGIDebug*)m_dxgiDebugInterface)->Release();
	m_dxgiDebugInterface = nullptr;
	::FreeLibrary((HMODULE)m_dxgiDebugLibModule);
	m_dxgiDebugLibModule = nullptr;
#endif

}

void Renderer::ClearScreen(const Rgba8& clearColor)
{
	float clearColorAsFloats[4];
	clearColor.GetAsFloats(clearColorAsFloats);
	Texture* color = GetActiveColorTarget();
	if (color != nullptr)
	{
		TextureView* rtv = color->GetRenderTargetView();
		m_deviceContext->ClearRenderTargetView(rtv->m_rtv, clearColorAsFloats);
	}
	ClearDepth();
}

void Renderer::BeginCamera(const Camera& camera)
{
	m_currentCamera = const_cast<Camera*>(&camera);
	DefineViewport(camera.GetCameraViewport());

	Texture* color = GetActiveColorTarget();
	TextureView* rtv = color->GetRenderTargetView();

	Texture* depth = GetCurrentDepthTarget();
	if (depth != nullptr)
	{
		TextureView* dsv = depth->GetDepthStencilView();
		m_deviceContext->OMSetRenderTargets(1, &(rtv->m_rtv), dsv->m_dsv);
	}

	BindCameraConstants(camera);

	m_modelMatrix = Mat44();
	Rgba8::WHITE.GetAsFloats(m_modelColor);

	SetRasterizerState(CullMode::NONE, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
	SetDepthStencilState(DepthTest::ALWAYS, false);
	SetSamplerMode(SamplerMode::POINTCLAMP);
	BindShaderByName(defaultShader);
	SetBlendMode(BlendMode::ALPHA);
	BindTexture(m_defaultTexture);
}

void Renderer::EndCamera(const Camera& camera)
{
	ASSERT_OR_DIE(m_currentCamera == &camera, "Unbinding unmatched camera");
	m_currentCamera = nullptr;
	m_deviceContext->ClearState();
}

ID3D11Device* Renderer::GetDevice() const
{
	return m_device;
}

ID3D11DeviceContext* Renderer::GetDeviceContext() const
{
	return m_deviceContext;
}

void Renderer::DrawVertexArray(int numVertexes, const Vertex_PCU* vertexes)
{
	BindModelConstants();

	size_t size = sizeof(*vertexes) * numVertexes;
	CopyCPUToGPU(vertexes, size, m_immediateVBO_PCU);
	DrawVertexBuffer(m_immediateVBO_PCU, numVertexes);
}

void Renderer::DrawVertexArray(int numVertexes, const Vertex_PNCU* vertexes)
{
	BindModelConstants();
	BindLightConstants();

	size_t size = sizeof(*vertexes) * numVertexes;
	CopyCPUToGPU(vertexes, size, m_immediateVBO_PNCU);
	DrawVertexBuffer(m_immediateVBO_PNCU, numVertexes);
}

void Renderer::DrawIndexed(int numIndices)
{
	BindModelConstants();
	BindLightConstants();

	m_deviceContext->DrawIndexed(numIndices, 0, 0);
}

void Renderer::DrawIndexed(unsigned int numIndices, unsigned int startIndexInIndexList, int startVertexInVertexList)
{
	BindModelConstants();
	m_deviceContext->DrawIndexed(numIndices, startIndexInIndexList, startVertexInVertexList);
}

Texture* Renderer::CreateSkyboxTexture(const char* skyboxName, const char* front, const char* back, const char* left, const char* right, const char* top, const char* bottom)
{
	Texture* skybox = GetTextureForFileName(skyboxName);

	if (!skybox)
	{
		Image frontImage(front);
		Image backImage(back);
		Image leftImage(left);
		Image rightImage(right);
		Image topImage(top);
		Image bottomImage(bottom);

		ID3D11Texture2D* handle = nullptr;
		D3D11_TEXTURE2D_DESC pTextureDesc = { 0 };
		pTextureDesc.Width = frontImage.GetDimensions().x;
		pTextureDesc.Height = frontImage.GetDimensions().y;
		pTextureDesc.MipLevels = 1;
		pTextureDesc.ArraySize = 6;
		pTextureDesc.Format = LocalToD3D11(eTextureFormat::R8G8B8A8_UNORM);
		pTextureDesc.SampleDesc.Count = 1;
		pTextureDesc.Usage = LocalToD3D11(eMemoryHint::STATIC);
		pTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		pTextureDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA pData[6] = {};
		pData[0].pSysMem = frontImage.GetRawData();
		pData[0].SysMemPitch = frontImage.GetDimensions().x * sizeof(Rgba8);
		pData[1].pSysMem = backImage.GetRawData();
		pData[1].SysMemPitch = backImage.GetDimensions().x * sizeof(Rgba8);
		pData[2].pSysMem = leftImage.GetRawData();
		pData[2].SysMemPitch = leftImage.GetDimensions().x * sizeof(Rgba8);
		pData[3].pSysMem = rightImage.GetRawData();
		pData[3].SysMemPitch = rightImage.GetDimensions().x * sizeof(Rgba8);
		pData[4].pSysMem = topImage.GetRawData();
		pData[4].SysMemPitch = topImage.GetDimensions().x * sizeof(Rgba8);
		pData[5].pSysMem = bottomImage.GetRawData();
		pData[5].SysMemPitch = bottomImage.GetDimensions().x * sizeof(Rgba8);

		HRESULT textureResult = m_device->CreateTexture2D(&pTextureDesc, pData, &handle);
		if (!SUCCEEDED(textureResult))
		{
			std::string errorString = "Error while creating 2d texture: ";
			errorString.append("skybox");
			ERROR_AND_DIE(errorString);
		}

		Texture* tex = new Texture();
		tex->m_owner = this;
		tex->m_name = skyboxName;
		tex->m_dimensions = frontImage.GetDimensions();
		tex->m_format = eTextureFormat::R8G8B8A8_UNORM;
		tex->m_texture = handle;
		tex->m_allowedBinds = TEXTURE_BIND_SHADER_RESOURCE_BIT;
		tex->m_arraySize = 6;
		tex->m_textureType = eTextureType::TEXTURE_ARRAY_2D;
		skybox = tex;

		m_loadedTextures.push_back(tex);
	}

	return skybox;
}

Texture* Renderer::CreateTexture(const TextureCreateInfo& createInfo)
{
	ID3D11Texture2D* handle = createInfo.m_handle;

	if (handle == nullptr)
	{
		D3D11_TEXTURE2D_DESC pTextureDesc = { 0 };
		pTextureDesc.Width = createInfo.m_dimensions.x;
		pTextureDesc.Height = createInfo.m_dimensions.y;
		pTextureDesc.MipLevels = 1;
		pTextureDesc.ArraySize = 1;
		pTextureDesc.Format = LocalToD3D11(createInfo.m_format);
		pTextureDesc.SampleDesc.Count = 1;
		pTextureDesc.Usage = LocalToD3D11(createInfo.m_memoryHint);
		pTextureDesc.BindFlags = LocalToD3D11BindFlags(createInfo.m_bindFlags);

		D3D11_SUBRESOURCE_DATA pIntialData = { 0 };
		D3D11_SUBRESOURCE_DATA* pInitialDataPtr = nullptr;
		if (createInfo.m_initialData != nullptr)
		{
			pIntialData.pSysMem = createInfo.m_initialData;
			pIntialData.SysMemPitch = (UINT)createInfo.m_initialDataByteSize / (UINT)createInfo.m_dimensions.y;
			pInitialDataPtr = &pIntialData;
		}

		HRESULT textureResult =  m_device->CreateTexture2D(&pTextureDesc, pInitialDataPtr, &handle);
		if (!SUCCEEDED(textureResult))
		{
			std::string errorString = "Error while creating 2d texture: ";
			errorString.append(createInfo.m_name);
			ERROR_AND_DIE(errorString);
		}
	}
	else
	{
		handle->AddRef();
	}

	Texture* tex = new Texture();
	tex->m_owner = this;
	tex->m_name = createInfo.m_name;
	tex->m_dimensions = createInfo.m_dimensions;
	tex->m_format = createInfo.m_format;
	tex->m_texture = handle;
	tex->m_allowedBinds = createInfo.m_bindFlags;
	return tex;
}

void Renderer::DestroyTexture(Texture* texture)
{
	if (texture != nullptr)
	{
		delete texture;
	}
}

Texture* Renderer::GetTextureForFileName(char const* imageFilePath)
{
	for (int textureCount = 0; textureCount < m_loadedTextures.size(); textureCount++)
	{
		std::string textureFilePath = m_loadedTextures[textureCount]->GetImageFilePath();
		if (textureFilePath == imageFilePath)
			return m_loadedTextures[textureCount];
	}

	return nullptr;
}

BitmapFont* Renderer::GetBitmpaFontForFileName(const char* fontFilePath)
{
	for (int fontCount = 0; fontCount < m_loadedFonts.size(); fontCount++)
	{
		std::string textureFilePath = m_loadedFonts[fontCount]->GetFontFilePath();
		if (textureFilePath == fontFilePath)
			return m_loadedFonts[fontCount];
	}

	return nullptr;
}

BitmapFont* Renderer::CreateBitmapFont(const char* fontFilePath)
{
	std::string fontFilePathWithExtension = std::string(fontFilePath) + ".png";
	Texture* bitmapFontTexture = CreateTextureFromFile(fontFilePathWithExtension.c_str());
	BitmapFont* newFont = new BitmapFont(fontFilePath, *bitmapFontTexture);

	m_loadedFonts.push_back(newFont);
	return newFont;
}

Texture* Renderer::CreateOrGetTextureFromFile(char const* imageFilePath)
{
	// See if we already have this texture previously loaded
	Texture* existingTexture = GetTextureForFileName(imageFilePath);
	if (existingTexture)
	{
		return existingTexture;
	}

	// Never seen this texture before!  Let's load it.
	Texture* newTexture = CreateTextureFromFile(imageFilePath);
	return newTexture;
}


BitmapFont* Renderer::CreateOrGetBitmapFont(const char* bitmapFontFilePathWithNoExtension)
{
	// See if we already have this texture previously loaded
	BitmapFont* existingFontTexture = GetBitmpaFontForFileName(bitmapFontFilePathWithNoExtension);
	if (existingFontTexture)
	{
		return existingFontTexture;
	}

	// Never seen this texture before!  Let's load it.
	BitmapFont* newFontTexture = CreateBitmapFont(bitmapFontFilePathWithNoExtension);
	return newFontTexture;
}

Texture* Renderer::CreateTextureFromFile(char const* imageFilePath, bool saveTexture)
{
	Image image(imageFilePath);
	Texture* newTexture = CreateTextureFromImage(image);

	if (saveTexture)
	{
		m_loadedTextures.push_back(newTexture);
		SetDebugName(newTexture->m_texture, newTexture->m_name.c_str());
	}

	return newTexture;
}


Texture* Renderer::CreateTextureFromData(char const* name, IntVec2 dimensions, int bytesPerTexel, uint8_t* texelData)
{
	// Check if the load was successful
	GUARANTEE_OR_DIE(texelData, Stringf("CreateTextureFromData failed for \"%s\" - texelData was null!", name));
	GUARANTEE_OR_DIE(bytesPerTexel >= 3 && bytesPerTexel <= 4, Stringf("CreateTextureFromData failed for \"%s\" - unsupported BPP=%i (must be 3 or 4)", name, bytesPerTexel));
	GUARANTEE_OR_DIE(dimensions.x > 0 && dimensions.y > 0, Stringf("CreateTextureFromData failed for \"%s\" - illegal texture dimensions (%i x %i)", name, dimensions.x, dimensions.y));

	Texture* newTexture = new Texture();
	newTexture->m_name = name;
	newTexture->m_dimensions = dimensions;

	return newTexture;
}

Texture* Renderer::CreateTextureFromImage(const Image& image)
{
	TextureCreateInfo ci{};
	ci.m_name = image.GetImageFilePath();
	ci.m_dimensions = image.GetDimensions();
	ci.m_initialData = image.GetRawData();
	ci.m_initialDataByteSize = ci.m_dimensions.x * ci.m_dimensions.y * sizeof(Rgba8);

	Texture* newTexture = CreateTexture(ci);
	return newTexture;
}

void Renderer::BindTexture(const Texture* texture, unsigned int slot, bool bindToVS)
{
	const Texture* textureToBind = m_defaultTexture;
	if (texture != nullptr)
	{
		textureToBind = texture;
	}

	Texture* tex = const_cast<Texture*>(textureToBind);
	TextureView* view = tex->GetShaderResourceView();
	m_deviceContext->PSSetShaderResources(slot, 1, &(view->m_srv));
	if (bindToVS)
		m_deviceContext->VSSetShaderResources(slot, 1, &(view->m_srv));
}

void Renderer::SetBlendMode(BlendMode blendMode)
{
	DX_SAFE_RELEASE(m_blendState);

	D3D11_BLEND_DESC pBlendDesc = { 0 };
	D3D11_RENDER_TARGET_BLEND_DESC pRenderTargetBlendDesc = { 0 };
	pRenderTargetBlendDesc.BlendEnable = true;
	pRenderTargetBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	pRenderTargetBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
	pRenderTargetBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
	pRenderTargetBlendDesc.DestBlendAlpha = D3D11_BLEND_ONE;
	pRenderTargetBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	switch (blendMode)
	{
	case BlendMode::OPAQUE:
		pRenderTargetBlendDesc.SrcBlend = D3D11_BLEND_ONE;
		pRenderTargetBlendDesc.DestBlend = D3D11_BLEND_ZERO;
		break;
	case BlendMode::ADDITIVE:
		pRenderTargetBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		pRenderTargetBlendDesc.DestBlend = D3D11_BLEND_ONE;
		break;
	case BlendMode::ALPHA:
		pRenderTargetBlendDesc.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		pRenderTargetBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		break;
	default:
		break;
	}

	pBlendDesc.RenderTarget[0] = pRenderTargetBlendDesc;
	HRESULT result = m_device->CreateBlendState(&pBlendDesc, &m_blendState);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Error while creating blend state");
	}

	float blendfactor[4] = { 0.f };
	UINT sampleMask = 0xffffffff;
	m_deviceContext->OMSetBlendState(m_blendState, blendfactor, sampleMask);

}

void Renderer::CreateD3DDeviceAndSwapChain()
{
	DXGI_SWAP_CHAIN_DESC pSwapChainDesc = { 0 };
	pSwapChainDesc.BufferDesc.Width = g_theWindow->GetClientDimensions().x;
	pSwapChainDesc.BufferDesc.Height = g_theWindow->GetClientDimensions().y;
	pSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	pSwapChainDesc.SampleDesc.Count = 1;
	pSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	pSwapChainDesc.BufferCount = 2;
	pSwapChainDesc.OutputWindow = HWND(g_theWindow->GetOSWindowHandle());
	pSwapChainDesc.Windowed = TRUE;
	pSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	UINT flags = 0;
#if defined(ENGINE_DEBUG_RENDER)
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;
	UINT sdkVersion = D3D11_SDK_VERSION;

	HRESULT result = D3D11CreateDeviceAndSwapChain(
		NULL,
		driverType,
		NULL,
		flags,
		NULL,
		0,
		sdkVersion,
		&pSwapChainDesc,
		&m_swapChain,
		&m_device,
		NULL,
		&m_deviceContext);

	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Error while creating swap chain and device");
	}
}

void Renderer::CreateBackBuffer()
{
	ID3D11Texture2D* texture2D = nullptr;
	m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)(&texture2D));

	D3D11_TEXTURE2D_DESC desc;
	texture2D->GetDesc(&desc);

	TextureCreateInfo info;
	info.m_name = "Swapchain";
	info.m_dimensions = IntVec2(desc.Width, desc.Height);
	ASSERT_OR_DIE(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM, "Swapchain isn't R8G8B8A - add conversion to local format");
	info.m_format = eTextureFormat::R8G8B8A8_UNORM;
	info.m_bindFlags = TEXTURE_BIND_RENDER_TARGET_BIT;
	info.m_memoryHint = eMemoryHint::GPU;
	info.m_handle = texture2D;
	m_backBuffer = CreateTexture(info);

	info.m_name = "DefaultColor";
	info.m_handle = nullptr;
	info.m_bindFlags |= TEXTURE_BIND_SHADER_RESOURCE_BIT;
	m_defaultColorTarget[0] = CreateTexture(info);
	m_defaultColorTarget[1] = CreateTexture(info);

	DX_SAFE_RELEASE(texture2D);
}

void Renderer::DefineViewport(const AABB2& camViewport)
{
	D3D11_VIEWPORT viewport = { 0 };
	viewport.TopLeftX = static_cast<float>(g_theWindow->GetClientDimensions().x) * camViewport.m_mins.x;
	viewport.TopLeftY = static_cast<float>(g_theWindow->GetClientDimensions().y) * camViewport.m_mins.y;
	viewport.Width = static_cast<float>(g_theWindow->GetClientDimensions().x) * (camViewport.m_maxs.x);
	viewport.Height = static_cast<float>(g_theWindow->GetClientDimensions().y) * (camViewport.m_maxs.y);
	viewport.MinDepth = 0.f;
	viewport.MaxDepth = 1.f;

	m_deviceContext->RSSetViewports(1, &viewport);
}

void Renderer::CreateAndSetRasterizerState()
{
	D3D11_RASTERIZER_DESC pRasterizerDesc = { };
	pRasterizerDesc.FillMode = D3D11_FILL_SOLID;
	pRasterizerDesc.CullMode = D3D11_CULL_NONE;
	pRasterizerDesc.DepthClipEnable = true;
	pRasterizerDesc.AntialiasedLineEnable = true;

	m_device->CreateRasterizerState(&pRasterizerDesc, &m_rasterizerState);
	m_deviceContext->RSSetState(m_rasterizerState);
}

void Renderer::CreateInputLayout(Shader* shader, bool isLitShader)
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
	inputElementDesc.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	if (isLitShader)
	{
		inputElementDesc.push_back({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	}
	inputElementDesc.push_back({ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	inputElementDesc.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	

	HRESULT hr = m_device->CreateInputLayout(
		inputElementDesc.data(),
		(UINT)inputElementDesc.size(),
		(void*)(m_vertexShaderByteCode.data()),
		m_vertexShaderByteCode.size(),
		&(shader->m_inputLayout));

	if (!SUCCEEDED(hr))
	{
		ERROR_AND_DIE("Error while creating input layout for the vertex shader");
	}
}

Shader* Renderer::CreateOrGetShader(const char* shaderName)
{
	Shader* existingShader = GetShaderForName(shaderName);
	if (existingShader)
		return existingShader;

	Shader* createdShader = CreateShader(shaderName);
	return createdShader;
}

const Shader* Renderer::GetDefaultShader() const
{
	return m_defaultShader;
}

void Renderer::BindShaderByName(const char* shaderName)
{
	Shader* shaderToBind = GetShaderForName(shaderName);
	BindShader(shaderToBind);
}

void Renderer::BindShader(Shader* shader)
{
	if (shader != nullptr)
	{
		m_currentShader = shader;
	}
	else
	{
		m_currentShader = m_defaultShader;
	}
	m_deviceContext->VSSetShader(m_currentShader->m_vertexShader, NULL, 0);
	m_deviceContext->PSSetShader(m_currentShader->m_pixelShader, NULL, 0);
}

Shader* Renderer::GetShaderForName(const char* shaderName)
{
	for (int i = 0; i < m_loadedShaders.size(); i++)
	{
		if (m_loadedShaders[i]->GetName() == shaderName)
			return m_loadedShaders[i];
	}

	return nullptr;
}

Shader* Renderer::CreateShader(char const* shaderName, char const* shaderSource)
{
	ShaderConfig config;
	config.m_name = shaderName;
	Shader* newShader = new Shader(config);
	CompileShaderToByteCode(m_vertexShaderByteCode, shaderName, shaderSource, newShader->m_config.m_vertexEntryPoint.c_str(), "vs_5_0");
	HRESULT vertexShaderCreationResult = m_device->CreateVertexShader(m_vertexShaderByteCode.data(), m_vertexShaderByteCode.size(), NULL, &(newShader->m_vertexShader));
	if (!SUCCEEDED(vertexShaderCreationResult))
	{
		ERROR_AND_DIE("Error in creating vertex shader");
	}

	const char* litShader = strstr(shaderName, "Lit");
	CreateInputLayout(newShader, litShader != nullptr ? true : false);

	CompileShaderToByteCode(m_pixelShaderByteCode, shaderName, shaderSource, newShader->m_config.m_pixelEntryPoint.c_str(), "ps_5_0");
	HRESULT pixelShaderCreationResult = m_device->CreatePixelShader(m_pixelShaderByteCode.data(), m_pixelShaderByteCode.size(), NULL, &(newShader->m_pixelShader));
	if (!SUCCEEDED(pixelShaderCreationResult))
	{
		ERROR_AND_DIE("Error in creating pixel shader");
	}

	m_loadedShaders.push_back(newShader);
	return newShader;
}

Shader* Renderer::CreateShader(const char* shaderName)
{
	std::string fileName = shaderName;
	fileName.append(".hlsl");
	std::string shaderSource;
	FileReadToString(shaderSource, fileName);
	return CreateShader(shaderName, shaderSource.c_str());
}

void Renderer::CreateAndCompileComputeShader(char const* shaderName, ID3D11ComputeShader** pComputeShader, const char* entryName)
{
	std::vector<uint8_t> shaderByteCode;
	std::string shaderSource;
	FileReadToString(shaderSource, shaderName);
	CompileShaderToByteCode(shaderByteCode, shaderName, shaderSource.c_str(), entryName, "cs_5_0");
	HRESULT result = m_device->CreateComputeShader(shaderByteCode.data(), shaderByteCode.size(), nullptr, pComputeShader);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create and compile compute shader");
	}
}

bool Renderer::CompileShaderToByteCode(std::vector<unsigned char>& outByteCode, char const* name, char const* source, char const* entryPoint, char const* target)
{
	UINT shaderCompileFlags = 0;
#ifdef  ENGINE_DEBUG_RENDER
	shaderCompileFlags |= D3DCOMPILE_DEBUG;
	shaderCompileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
	//shaderCompileFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
#else
	shaderCompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif //  ENGINE_DEBUG_RENDER

	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* shaderBlob = nullptr;

	HRESULT compileResult = D3DCompile(source, strlen(source), name, NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint, target, shaderCompileFlags, 0, &shaderBlob, &errorBlob);

	if (!SUCCEEDED(compileResult))
	{
		const char* compileErrorString = reinterpret_cast<const char*>(errorBlob->GetBufferPointer());
		std::string completeErrorString = "Failed to compile: ";
		completeErrorString.append(name);
		completeErrorString.append(compileErrorString);
		errorBlob->Release();
		ERROR_AND_DIE(compileErrorString);
	}

	outByteCode.resize(shaderBlob->GetBufferSize());
	memcpy(outByteCode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
	shaderBlob->Release();

	return true;
}

VertexBuffer* Renderer::CreateVertexBuffer(const size_t size, unsigned int stride)
{
	VertexBuffer* newBuffer = new VertexBuffer(size, stride);
	CreateD3DVertexBuffer(size, newBuffer);
	return newBuffer;
}

void Renderer::CreateD3DVertexBuffer(const size_t size, VertexBuffer* vbo)
{
	D3D11_BUFFER_DESC pDesc = { 0 };
	pDesc.Usage = D3D11_USAGE_DYNAMIC;
	pDesc.ByteWidth = static_cast<UINT>(size);
	pDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	pDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = m_device->CreateBuffer(&pDesc, NULL, &(vbo->m_buffer));
	if (!SUCCEEDED(hr))
	{
		ERROR_AND_DIE("Error while creating vertex buffer");
	}
	vbo->m_size = size;
}

void Renderer::CopyCPUToGPU(const void* data, size_t size, VertexBuffer* vbo)
{
	if (size > vbo->m_size)
	{
		DX_SAFE_RELEASE(vbo->m_buffer);
		CreateD3DVertexBuffer(size, vbo);
		vbo->m_size = size;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResources = { 0 };
	m_deviceContext->Map(vbo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResources);
	memcpy(mappedResources.pData, data, size);
	m_deviceContext->Unmap(vbo->m_buffer, 0);
}

void Renderer::CopyCPUToGPU(const void* data, const size_t size, ID3D11Buffer* pDestinationBuffer)
{
	D3D11_BUFFER_DESC buffDesc = { 0 };
	pDestinationBuffer->GetDesc(&buffDesc);
	if (buffDesc.ByteWidth < size)
	{
		ERROR_AND_DIE("Trying to copy data which is larger than the buffer");
	}

	D3D11_MAPPED_SUBRESOURCE mappedResource = { 0 };
	m_deviceContext->Map(pDestinationBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, data, size);
	m_deviceContext->Unmap(pDestinationBuffer, 0);
}

void Renderer::BindVertexBuffer(VertexBuffer* vbo)
{
	UINT stride = vbo->GetStride();
	UINT offset = 0;
	m_deviceContext->IASetVertexBuffers(0, 1, &(vbo->m_buffer), &stride, &offset);
	m_deviceContext->IASetInputLayout(m_currentShader->m_inputLayout);
	m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Renderer::DrawVertexBuffer(VertexBuffer* vbo, int vertexCount, int vertexOffset)
{
	BindVertexBuffer(vbo);
	m_deviceContext->Draw(vertexCount, vertexOffset);
}

ConstantBuffer* Renderer::CreateConstantBuffer(const size_t size)
{
	ConstantBuffer* newBuffer = new ConstantBuffer(size);
	CreateD3DConstantBuffer(size, newBuffer);
	return newBuffer;
}

void Renderer::CreateD3DConstantBuffer(const size_t size, ConstantBuffer* cbo)
{
	D3D11_BUFFER_DESC pDesc = { 0 };
	pDesc.Usage = D3D11_USAGE_DYNAMIC;
	pDesc.ByteWidth = static_cast<UINT>(size);
	pDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = m_device->CreateBuffer(&pDesc, NULL, &(cbo->m_buffer));
	if (!SUCCEEDED(hr))
	{
		ERROR_AND_DIE("Error while creating constant buffer");
	}
	cbo->m_size = size;
}

void Renderer::CopyCPUToGPU(const void* data, size_t size, ConstantBuffer* cbo)
{
	if (size > cbo->m_size)
	{
		DX_SAFE_RELEASE(cbo->m_buffer);
		CreateD3DConstantBuffer(size, cbo);
		cbo->m_size = size;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResources = { 0 };
	m_deviceContext->Map(cbo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResources);
	memcpy(mappedResources.pData, data, size);
	m_deviceContext->Unmap(cbo->m_buffer, 0);
}

void Renderer::BindConstantBuffer(int slot, ConstantBuffer* cbo)
{
	m_deviceContext->VSSetConstantBuffers(slot, 1, &(cbo->m_buffer));
	m_deviceContext->PSSetConstantBuffers(slot, 1, &(cbo->m_buffer));
}

void Renderer::BindConstantBufferToComputeShader(int slot, ConstantBuffer* cbo)
{
	m_deviceContext->CSSetConstantBuffers(slot, 1, &(cbo->m_buffer));
}

IndexBuffer* Renderer::CreateIndexBuffer(const size_t size)
{
	IndexBuffer* indexBuffer = new IndexBuffer(size);
	CreateD3DIndexBuffer(size, indexBuffer);
	return indexBuffer;
}

void Renderer::CreateD3DIndexBuffer(const size_t size, IndexBuffer* ibo)
{
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = static_cast<UINT>(size);
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;

	// Create the buffer with the device.
	HRESULT result = m_device->CreateBuffer(&bufferDesc, NULL, &(ibo->m_buffer));
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create index buffer");
	}
}

void Renderer::CreateD3DShaderResource(ID3D11Buffer** pBuffer, const size_t totalBufferSize, const size_t stride, const void* initialData)
{
	D3D11_BUFFER_DESC buffDesc = {};
	buffDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	buffDesc.Usage = D3D11_USAGE_DYNAMIC;
	buffDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	buffDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buffDesc.StructureByteStride = static_cast<unsigned int>(stride);
	buffDesc.ByteWidth = static_cast<unsigned int>(totalBufferSize);

	D3D11_SUBRESOURCE_DATA inputData;
	inputData.pSysMem = initialData;
	HRESULT result = m_device->CreateBuffer(&buffDesc, &inputData, pBuffer);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create shader resource");
	}
}

void Renderer::CreateShaderResourceView(ID3D11ShaderResourceView** pResourceView, ID3D11Buffer* pShaderResource)
{
	HRESULT result = m_device->CreateShaderResourceView(pShaderResource, nullptr, pResourceView);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create SRV for given shader resouce");
	}
}

void Renderer::CreateD3DUnorderedAccessBuffer(ID3D11Buffer** pBuffer, const size_t totalBufferSize, const size_t stride, const void* initialData, bool cpuWriteAccess)
{
	UNUSED(initialData);
	D3D11_BUFFER_DESC buffDesc = {};
	buffDesc.BindFlags = cpuWriteAccess ? D3D11_BIND_SHADER_RESOURCE : (D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);
	buffDesc.Usage = cpuWriteAccess ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	if (cpuWriteAccess)
	{
		buffDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	buffDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	buffDesc.StructureByteStride = static_cast<unsigned int>(stride);
	buffDesc.ByteWidth = static_cast<UINT>(totalBufferSize);

	D3D11_SUBRESOURCE_DATA inputData;
	inputData.pSysMem = initialData;
	HRESULT result = m_device->CreateBuffer(&buffDesc, initialData ? &inputData : nullptr, pBuffer);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create unordered access type buffer");
	}
}

void Renderer::CreateUnorderedAccessView(ID3D11UnorderedAccessView** pResourceView, ID3D11Buffer* pUnorderedResource, UAVType type)
{
	D3D11_BUFFER_DESC resourceDesc;
	pUnorderedResource->GetDesc(&resourceDesc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {  };
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer = { 0 };
	if (type == UAVType::REGULAR)
	{
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = (resourceDesc.ByteWidth / resourceDesc.StructureByteStride);
	}
	else if (type == UAVType::APPEND)
	{
		uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = (resourceDesc.ByteWidth / resourceDesc.StructureByteStride);
	}
	else if (type == UAVType::RAW)
	{
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = 1;		//hard cording this here, not sure if this is right or not
	}
	

	HRESULT result = m_device->CreateUnorderedAccessView(pUnorderedResource, &uavDesc, pResourceView);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create UAV for resource");
	}
}

void Renderer::CreateD3DStagingBuffer(ID3D11Buffer** pBuffer, const size_t totalBufferSize, const size_t stride)
{
	D3D11_BUFFER_DESC desc = { 0 };
	desc.BindFlags = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.StructureByteStride = static_cast<UINT>(stride);
	desc.ByteWidth = static_cast<UINT>(totalBufferSize);

	HRESULT result = m_device->CreateBuffer(&desc, nullptr, pBuffer);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to create staging buffer");
	}
}

void Renderer::CreateD3DIndirectArgsBuffer(ID3D11Buffer** pBuffer, const size_t totalBufferSize, const size_t stride, const void* initialData)
{
	D3D11_BUFFER_DESC desc = { 0 };
	desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.StructureByteStride = static_cast<UINT>(stride);
	desc.ByteWidth = static_cast<UINT>(totalBufferSize);
	desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	D3D11_SUBRESOURCE_DATA subData;
	if (initialData)
	{
		subData.pSysMem = initialData;
	}
	HRESULT result = m_device->CreateBuffer(&desc, initialData ? &subData : nullptr, pBuffer);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Failed to indirect args buffer");
	}

}

void Renderer::BindComputeShader(ID3D11ComputeShader* pComputeShader)
{
	m_deviceContext->CSSetShader(pComputeShader, nullptr, 0);
}

void Renderer::BindComputeShaderSRV(ID3D11ShaderResourceView* pSRV, int slotNum)
{
	m_deviceContext->CSSetShaderResources(slotNum, 1, &pSRV);
}

void Renderer::BindComputeShaderUAV(ID3D11UnorderedAccessView* pUAV, int slotNum, unsigned int initialCount)
{
	m_deviceContext->CSSetUnorderedAccessViews(slotNum, 1, &pUAV, &initialCount);
}

void Renderer::DispatchComputeShader(int numThreadGroupX, int numThreadGroupY, int numThreadGroupZ)
{
	m_deviceContext->Dispatch(numThreadGroupX, numThreadGroupY, numThreadGroupZ);
}

void Renderer::DispatchIndirectComputeShader(ID3D11Buffer* indirectArgsBuffer, unsigned int byteOffset)
{
	m_deviceContext->DispatchIndirect(indirectArgsBuffer, byteOffset);
}

void Renderer::CopyOverResourcesInGPU(ID3D11Buffer* pDestResource, ID3D11Buffer* pSourceResource)
{
	//has some restrictions for copying. can copy only resources which are of the same type, identical dimensions and compatible DXGI formats
	m_deviceContext->CopyResource(pDestResource, pSourceResource);
}

void Renderer::CopyGPUStructCount(ID3D11Buffer* pDestCountBuffer, unsigned int byteOffset, ID3D11UnorderedAccessView* pSrcUAV)
{
	m_deviceContext->CopyStructureCount(pDestCountBuffer, byteOffset, pSrcUAV);
}

void Renderer::CopyGPUToCPU(void* data, const size_t copySize, ID3D11Buffer* pSourceBuffer)
{
	D3D11_MAPPED_SUBRESOURCE mappedResources = { 0 };
	m_deviceContext->Map(pSourceBuffer, 0, D3D11_MAP_READ, 0, &mappedResources);
	memcpy(data, mappedResources.pData, copySize);
	m_deviceContext->Unmap(pSourceBuffer, 0);
}

void Renderer::BindModelConstants()
{
	ModelConstants modelConstants;
	modelConstants.modelMatrix = m_modelMatrix;
	modelConstants.modelColor[0] = m_modelColor[0];
	modelConstants.modelColor[1] = m_modelColor[1];
	modelConstants.modelColor[2] = m_modelColor[2];
	modelConstants.modelColor[3] = m_modelColor[3];
	CopyCPUToGPU(&modelConstants, sizeof(modelConstants), m_modelCBO);
	BindConstantBuffer(modelConstantBufferSlot, m_modelCBO);
}

void Renderer::BindLightConstants()
{
	LightingConstants lightingConstants;
	lightingConstants.sunDirection = m_sunDirection;
	lightingConstants.sunIntensity = m_sunIntensity;
	lightingConstants.ambientIntensity = m_ambientIntensity;
	lightingConstants.spotLight = m_spotlight;
	CopyCPUToGPU(&lightingConstants, sizeof(lightingConstants), m_lightCBO);
	BindConstantBuffer(lightingConstantBufferSlot, m_lightCBO);
}

void Renderer::BindCameraConstants(const Camera& camera)
{
	CameraConstants camConstants;
	camConstants.projectionMatrix = camera.GetProjectionMatrix();
	camConstants.viewMatrix = camera.GetViewMatrix();

	CopyCPUToGPU(&camConstants, sizeof(CameraConstants), m_cameraCBO);
	BindConstantBuffer(cameraConstantBufferSlot, m_cameraCBO);
}

void Renderer::CopyCPUToGPU(const void* data, size_t size, IndexBuffer* ibo)
{
	if (size > ibo->m_size)
	{
		DX_SAFE_RELEASE(ibo->m_buffer);
		CreateD3DIndexBuffer(size, ibo);
		ibo->m_size = size;
	}

	D3D11_MAPPED_SUBRESOURCE mappedResources = { 0 };
	m_deviceContext->Map(ibo->m_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResources);
	memcpy(mappedResources.pData, data, size);
	m_deviceContext->Unmap(ibo->m_buffer, 0);
}

void Renderer::BindIndexBuffer(IndexBuffer* ibo)
{
	m_deviceContext->IASetIndexBuffer(ibo->m_buffer, DXGI_FORMAT_R32_UINT, 0);
}

void Renderer::SetDebugName(ID3D11DeviceChild* object, const char* name)
{
#ifdef ENGINE_DEBUG_RENDER
	object->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#else
	UNUSED(object);
	UNUSED(name);
#endif
}

void Renderer::CreateDepthStencilTextureAndView()
{
	TextureCreateInfo info;
	info.m_name = "DefaultDepth";
	info.m_dimensions = IntVec2(g_theWindow->GetClientDimensions());
	info.m_format = eTextureFormat::R24G8_TYPELESS;
	info.m_bindFlags = TEXTURE_BIND_DEPTH_STENCIL_BIT | TEXTURE_BIND_SHADER_RESOURCE_BIT;
	info.m_memoryHint = eMemoryHint::GPU;

	m_depthBuffer = CreateTexture(info);
}

Texture* Renderer::GetCurrentColorTarget() const
{
	Texture* color = nullptr;
	if(m_currentCamera)
		color = m_currentCamera->GetColorTarget();
	return (color == nullptr) ? m_defaultColorTarget[m_activeColorTargetIndex] : color;
}

RENDERDOC_API_1_1_2* Renderer::GetRenderDocAPIHandle() const
{
	return m_rdoc_api;
}

void Renderer::StartRenderDocFrameCapture()
{
	//since we have only one device and one window, we can pass null to both arguments according to the API
	if (m_rdoc_api)
		m_rdoc_api->StartFrameCapture(nullptr, nullptr);
}

void Renderer::EndRenderDocFrameCapture()
{
	//since we have only one device and one window, we can pass null to both arguments according to the API
	if (m_rdoc_api)
		m_rdoc_api->EndFrameCapture(nullptr, nullptr);
}

Texture* Renderer::GetActiveColorTarget() const
{
	return m_defaultColorTarget[m_activeColorTargetIndex];
}

Texture* Renderer::GetBackupColorTarget() const
{
	return m_defaultColorTarget[(m_activeColorTargetIndex + 1) % 2];
}

Texture* Renderer::GetCurrentDepthTarget() const
{
	Texture* depth = m_currentCamera->GetDepthTarget();
	return  (depth == nullptr) ? m_depthBuffer : depth;
}

void Renderer::CopyTexture(Texture* destination, Texture* source)
{
	m_deviceContext->CopyResource(destination->m_texture, source->m_texture);
}

void Renderer::CopyTextureWithShader(Texture* destination, Texture* source, Shader* effect)
{
	BindShader(effect);
	TextureView* rtv = destination->GetRenderTargetView();
	m_deviceContext->OMSetRenderTargets(1, &(rtv->m_rtv), nullptr);
	m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DefineViewport(AABB2::ZERO_TO_ONE);
	BindTexture(source);
	BindTexture(m_depthBuffer, 1);
	SetSamplerMode(SamplerMode::BILINEARCLAMP);
	m_deviceContext->Draw(3, 0);
	m_deviceContext->ClearState();
}

void Renderer::ApplyEffect(Shader* effect)
{
	Texture* active = GetActiveColorTarget();
	Texture* backup = GetBackupColorTarget();
	
	CopyTextureWithShader(backup, active, effect);
	m_activeColorTargetIndex = (m_activeColorTargetIndex + 1) % 2;
}

void Renderer::SetModelMatrix(const Mat44& modelMatrix)
{
	m_modelMatrix = modelMatrix;
}

void Renderer::SetModelColor(const Rgba8& modelColor)
{
	modelColor.GetAsFloats(m_modelColor);
}

void Renderer::SetRasterizerState(CullMode cullMode, FillMode fillMode, WindingOrder windingOrder)
{
	DX_SAFE_RELEASE(m_rasterizerState);

	D3D11_RASTERIZER_DESC pRasterizerDesc = { };
	pRasterizerDesc.DepthClipEnable = true;
	pRasterizerDesc.AntialiasedLineEnable = true;

	switch (cullMode)
	{
	case CullMode::NONE:
		pRasterizerDesc.CullMode = D3D11_CULL_NONE;
		break;
	case CullMode::FRONT:
		pRasterizerDesc.CullMode = D3D11_CULL_FRONT;
		break;
	case CullMode::BACK:
		pRasterizerDesc.CullMode = D3D11_CULL_BACK;
		break;
	}

	switch (fillMode)
	{
	case FillMode::SOLID:
		pRasterizerDesc.FillMode = D3D11_FILL_SOLID;
		break;
	case FillMode::WIREFRAME:
		pRasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
		break;
	}

	switch (windingOrder)
	{
	case WindingOrder::CLOCKWISE:
		pRasterizerDesc.FrontCounterClockwise = false;
		break;
	case WindingOrder::COUNTERCLOCKWISE:
		pRasterizerDesc.FrontCounterClockwise = true;
		break;
	}

	m_device->CreateRasterizerState(&pRasterizerDesc, &m_rasterizerState);
	m_deviceContext->RSSetState(m_rasterizerState);
}

void Renderer::ClearDepth(float value)
{
	Texture* depth = GetCurrentDepthTarget();
	TextureView* dsv = depth->GetDepthStencilView();
	m_deviceContext->ClearDepthStencilView(dsv->m_dsv, D3D11_CLEAR_DEPTH, value, 0);
}

void Renderer::SetDepthStencilState(DepthTest depthTest, bool writeDepth)
{
	DX_SAFE_RELEASE(m_depthStencilState);

	D3D11_DEPTH_STENCIL_DESC pStencilDesc = { 0 };
	pStencilDesc.DepthEnable = true;
	if (writeDepth)
		pStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	else
		pStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

	switch (depthTest)
	{
	case DepthTest::ALWAYS:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		break;
	case DepthTest::NEVER:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_NEVER;
		break;
	case DepthTest::EQUAL:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
		break;
	case DepthTest::NOTEQUAL:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_NOT_EQUAL;
		break;
	case DepthTest::LESS:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		break;
	case DepthTest::LESSEQUAL:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		break;
	case DepthTest::GREATER:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER;
		break;
	case DepthTest::GREATEREQUAL:
		pStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		break;
	default:
		break;
	}

	HRESULT stencilStateResult = m_device->CreateDepthStencilState(&pStencilDesc, &m_depthStencilState);
	if (!SUCCEEDED(stencilStateResult))
	{
		ERROR_AND_DIE("Error while creating depth stencil state");
	}

	m_deviceContext->OMSetDepthStencilState(m_depthStencilState, 0);
}

void Renderer::SetSamplerMode(SamplerMode samplerMode)
{
	DX_SAFE_RELEASE(m_samplerState);

	D3D11_SAMPLER_DESC pSamplerDesc = {  };

	D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	D3D11_TEXTURE_ADDRESS_MODE addressMode = D3D11_TEXTURE_ADDRESS_CLAMP;
	switch (samplerMode)
	{
	case SamplerMode::POINTCLAMP:
		filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		addressMode = D3D11_TEXTURE_ADDRESS_CLAMP;
		break;
	case SamplerMode::POINTWRAP:
		filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		addressMode = D3D11_TEXTURE_ADDRESS_WRAP;
		break;
	case SamplerMode::BILINEARCLAMP:
		filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		addressMode = D3D11_TEXTURE_ADDRESS_CLAMP;
		break;
	case SamplerMode::BILINEARWRAP:
		filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		addressMode = D3D11_TEXTURE_ADDRESS_WRAP;
		break;
	}
	pSamplerDesc.Filter = filter;
	pSamplerDesc.AddressU = addressMode;
	pSamplerDesc.AddressV = addressMode;
	pSamplerDesc.AddressW = addressMode;
	pSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	pSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT result = m_device->CreateSamplerState(&pSamplerDesc, &m_samplerState);
	if (!SUCCEEDED(result))
	{
		ERROR_AND_DIE("Error while creating sampler state");
	}

	m_deviceContext->PSSetSamplers(0, 1, &m_samplerState);
}

void Renderer::SetOutputRenderTarget(ID3D11RenderTargetView** rtv, ID3D11DepthStencilView* dsv)
{
	m_deviceContext->OMSetRenderTargets(1, rtv, dsv);
}

void Renderer::SetSunDirection(const Vec3& direction)
{
	m_sunDirection = direction;
}

void Renderer::SetSunIntensity(float intensity)
{
	m_sunIntensity = intensity;
}

void Renderer::SetAmbientIntensity(float intensity)
{
	m_ambientIntensity = intensity;
}

void Renderer::SetSpotlightProperties(const SpotLight& info)
{
	m_spotlight = info;
}


