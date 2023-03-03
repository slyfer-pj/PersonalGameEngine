#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

SpriteAnimDefinition::SpriteAnimDefinition(const SpriteSheet& spriteSheet, int startSpriteIndex, int endSpriteIndex, float durationSeconds, SpriteAnimPlaybackType playbackType)
	:m_spriteSheet(spriteSheet),
	 m_startSpriteIndex(startSpriteIndex),
	 m_endSpriteIndex(endSpriteIndex),
	 m_durationSeconds(durationSeconds),
	 m_playbackType(playbackType)
{

}

const SpriteDefinition& SpriteAnimDefinition::GetSpriteDefAtTime(float seconds) const
{
	if (m_playbackType == SpriteAnimPlaybackType::ONCE)
	{
		int numFrames = m_endSpriteIndex - m_startSpriteIndex + 1;
		int spriteIndex = static_cast<int>((seconds / m_durationSeconds) * numFrames);
		return m_spriteSheet.GetSpriteDefinition(Clamp(m_startSpriteIndex + spriteIndex, m_startSpriteIndex, m_endSpriteIndex));
	}
	else if (m_playbackType == SpriteAnimPlaybackType::LOOP)
	{
		int numFrames = m_endSpriteIndex - m_startSpriteIndex + 1;
		seconds = fmodf(seconds, m_durationSeconds);
		int spriteIndex = static_cast<int>((seconds / m_durationSeconds) * numFrames);
		return m_spriteSheet.GetSpriteDefinition(m_startSpriteIndex + spriteIndex);
	}
	else
	{
		ERROR_AND_DIE("Not implemented yet");
	}
}
