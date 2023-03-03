#pragma once
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

template <typename T>
struct AnimatedValueKey
{
public:
	AnimatedValueKey();
	AnimatedValueKey(T value, float time);

	float GetTime() const { return m_time; }
	T GetValue() const { return m_value; }

	void SetTime(float time) { m_time = time; }
	void SetValue(T value) { m_value = value; }

	bool operator<(const AnimatedValueKey& right)
	{
		return m_time < right.GetTime();
	}

private:
	float m_time = 0.f;
	T m_value;
};

template <typename T>
AnimatedValueKey<T>::AnimatedValueKey()
	:m_time(0.f), m_value(T())
{
}

template <typename T>
AnimatedValueKey<T>::AnimatedValueKey(T value, float time)
	: m_time(time), m_value(value)
{
}

template <typename T>
void GetAnimationFramesGivenNormalizedAge(float normalizedAge, int& prevFrame, int& nextFrame, const std::vector<AnimatedValueKey<T>>& keys)
{
	ASSERT_OR_DIE(keys.size() > 1, "Less than 2 animated keys");
	prevFrame = 0;
	nextFrame = int(keys.size() - 1);

	for (int i = 0; i < keys.size(); i++)
	{
		if (normalizedAge < keys[i].GetTime())
		{
			nextFrame = i;
			prevFrame = Clamp<int>(i - 1, 0, int(keys.size() - 1));
			break;
		}
	}
}

//------------------------------------------------------------------------------

template <typename T>
struct AnimatedCurve
{
public:
	AnimatedCurve() = default;
	AnimatedCurve(bool randBetweenTwoCurves);
	void AddKeyToCurveOne(AnimatedValueKey<T> key);
	void AddKeyToCurveTwo(AnimatedValueKey<T> key);
	void GetAnimationFramesGivenNormalizedAge(float normalizedAge, int& curveOnePrevFrame, int& curveOneNextFrame, int& curveTwoPrevFrame, int& curveTwoNextFrame) const;
	void GetAnimationFramesGivenNormalizedAge(float normalizedAge, int& curveOnePrevFrame, int& curveOneNextFrame) const;
	void SetToRandomBetweenCurves(bool set);
	bool IsToRandomBetweenCurves() const { return m_randomBetweenTwoCurves; };
	T GetInterpolatedAnimValueFromCurve(float normalizedAge, const RandomNumberGenerator& rng, int indexForNoiseFunction) const;

public:
	std::vector<AnimatedValueKey<T>> m_curveOneKeys;
	std::vector<AnimatedValueKey<T>> m_curveTwoKeys;

private:
	bool m_randomBetweenTwoCurves = false;
};

template <typename T>
T AnimatedCurve<T>::GetInterpolatedAnimValueFromCurve(float normalizedAge, const RandomNumberGenerator& rng, int indexForNoiseFunction) const
{
	int curveOnePrevFrame = 0, curveOneNextFrame = 0;
	int curveTwoPrevFrame = 0, curveTwoNextFrame = 0;
	float fromValue = 0.f, toValue = 0.f;
	if (IsToRandomBetweenCurves())
	{
		GetAnimationFramesGivenNormalizedAge(normalizedAge, curveOnePrevFrame, curveOneNextFrame,
			curveTwoPrevFrame, curveTwoNextFrame);
		fromValue = rng.GetRandomFloatInRange(m_curveOneKeys[curveOnePrevFrame].GetValue(),
			m_curveTwoKeys[curveTwoPrevFrame].GetValue(), indexForNoiseFunction);
		toValue = rng.GetRandomFloatInRange(m_curveOneKeys[curveOneNextFrame].GetValue(),
			m_curveTwoKeys[curveTwoNextFrame].GetValue(), indexForNoiseFunction);
	}
	else
	{
		GetAnimationFramesGivenNormalizedAge(normalizedAge, curveOnePrevFrame, curveOneNextFrame);
		fromValue = m_curveOneKeys[curveOnePrevFrame].GetValue();
		toValue = m_curveOneKeys[curveOneNextFrame].GetValue();
	}

	float t = GetFractionWithin(normalizedAge, m_curveOneKeys[curveOnePrevFrame].GetTime(), m_curveOneKeys[curveOneNextFrame].GetTime());
	return Interpolate(fromValue, toValue, t);
}

template <typename T>
void AnimatedCurve<T>::SetToRandomBetweenCurves(bool set)
{
	m_randomBetweenTwoCurves = set;
}

template <typename T>
void AnimatedCurve<T>::GetAnimationFramesGivenNormalizedAge(float normalizedAge, int& curveOnePrevFrame, int& curveOneNextFrame) const
{
	ASSERT_OR_DIE(m_curveOneKeys.size() > 1, "Less than 2 animated keys");
	if (m_randomBetweenTwoCurves)
	{
		ASSERT_OR_DIE(m_curveTwoKeys.size() > 1, "Less than 2 animated keys in curve two");
	}

	//get keys for curve one
	curveOnePrevFrame = 0;
	curveOneNextFrame = int(m_curveOneKeys.size() - 1);

	for (int i = 0; i < m_curveOneKeys.size(); i++)
	{
		if (normalizedAge < m_curveOneKeys[i].GetTime())
		{
			curveOneNextFrame = i;
			curveOnePrevFrame = Clamp<int>(i - 1, 0, int(m_curveOneKeys.size() - 1));
			break;
		}
	}
}

template <typename T>
void AnimatedCurve<T>::GetAnimationFramesGivenNormalizedAge(float normalizedAge, int& curveOnePrevFrame, int& curveOneNextFrame, int& curveTwoPrevFrame, int& curveTwoNextFrame) const
{
	ASSERT_OR_DIE(m_curveOneKeys.size() > 1, "Less than 2 animated keys");
	if (m_randomBetweenTwoCurves)
	{
		ASSERT_OR_DIE(m_curveTwoKeys.size() > 1, "Less than 2 animated keys in curve two");
	}

	//get keys for curve one
	GetAnimationFramesGivenNormalizedAge(normalizedAge, curveOnePrevFrame, curveOneNextFrame);

	//get keys for curve two
	curveTwoPrevFrame = 0;
	curveTwoNextFrame = int(m_curveTwoKeys.size() - 1);

	for (int i = 0; i < m_curveTwoKeys.size(); i++)
	{
		if (normalizedAge < m_curveTwoKeys[i].GetTime())
		{
			curveTwoNextFrame = i;
			curveTwoPrevFrame = Clamp<int>(i - 1, 0, int(m_curveTwoKeys.size() - 1));
			break;
		}
	}
}

template <typename T>
void AnimatedCurve<T>::AddKeyToCurveTwo(AnimatedValueKey<T> key)
{
	if (!m_randomBetweenTwoCurves)
	{
		ERROR_AND_DIE("trying to add key to curve two, in single curve mode");
	}

	m_curveTwoKeys.push_back(key);
}

template <typename T>
void AnimatedCurve<T>::AddKeyToCurveOne(AnimatedValueKey<T> key)
{
	m_curveOneKeys.push_back(key);
}

template <typename T>
AnimatedCurve<T>::AnimatedCurve(bool randBetweenTwoCurves)
	:m_randomBetweenTwoCurves(randBetweenTwoCurves)
{
}
