#include "Engine/Renderer/DebugRenderObject.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/Camera.hpp"

extern Renderer* g_theRenderer;

void DebugRenderObject::Startup(Clock& parentClock)
{
	if (m_totalLifeDuration > 0.f)
		m_stopwatch.Start(&parentClock, m_totalLifeDuration);
	else
		m_stopwatch.Stop();

	m_tint = m_startColor;
}

void DebugRenderObject::Update()
{
	if (m_stopwatch.CheckDurationElapsedAndDecrement())
	{
		m_isDead = true;
		return;
	}

	InterpolateColor();

	if (m_isBillboard)
	{
		Vec3 pos;
		EulerAngles ori;
		m_camera->GetTransform(pos, ori);
		m_billboardTransform = m_transform;
		Mat44 cameraOrienationMat = ori.GetAsMatrix_XFwd_YLeft_ZUp();
		Mat44 billboardMat = Mat44(-cameraOrienationMat.GetIBasis3D(), -cameraOrienationMat.GetJBasis3D(), cameraOrienationMat.GetKBasis3D(), Vec3());
		m_billboardTransform.Append(billboardMat);
		m_billboardTransform.Append(Mat44(Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(1.0f, 0.f, 0.f), Vec3()));
		m_billboardTransform.Append(Mat44::CreateTranslation2D(Vec2(-m_billboardTextAlignment.x * m_billboardTextWidth, -m_billboardTextAlignment.y * m_billboardTextHeight)));
		//DebugAddMessage(Stringf("pos = %f", pos.x), 0.1f, Rgba8::WHITE, Rgba8::WHITE);
	}
}

Mat44 DebugRenderObject::GetModelMatrix() const
{
	if (m_isBillboard)
		return m_billboardTransform;

	return m_transform;
}

void DebugRenderObject::InterpolateColor()
{
	m_tint.g = (unsigned char)Interpolate((float)m_startColor.g, (float)m_endColor.g, m_stopwatch.GetElapsedFraction());
	m_tint.b = (unsigned char)Interpolate((float)m_startColor.b, (float)m_endColor.b, m_stopwatch.GetElapsedFraction());
	m_tint.r = (unsigned char)Interpolate((float)m_startColor.r, (float)m_endColor.r, m_stopwatch.GetElapsedFraction());
}
