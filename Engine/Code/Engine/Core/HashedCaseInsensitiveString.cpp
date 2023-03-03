#include "Engine/Core/HashedCaseInsensitiveString.hpp"

HashedCaseInsensitiveString::HashedCaseInsensitiveString(const std::string& fromStdString)
	:m_originalString(fromStdString)
{
	CalculateStringHashValue();
}

HashedCaseInsensitiveString::HashedCaseInsensitiveString(const char* fromCString)
	:m_originalString(fromCString)
{
	CalculateStringHashValue();
}

bool HashedCaseInsensitiveString::operator<(const HashedCaseInsensitiveString& compare) const
{
	if (m_toLowerHash < compare.m_toLowerHash)
		return true;
	else if (m_toLowerHash == compare.m_toLowerHash)
	{
		int compareValue = _stricmp(m_originalString.c_str(), compare.m_originalString.c_str());
		return compareValue < 0;
	}

	return false;
}

bool HashedCaseInsensitiveString::operator==(const HashedCaseInsensitiveString& compare) const
{
	if (m_toLowerHash != compare.m_toLowerHash)
		return false;

	return !_stricmp(m_originalString.c_str(), compare.m_originalString.c_str());
}

void HashedCaseInsensitiveString::CalculateStringHashValue()
{
	m_toLowerHash = 0;
	for (int i = 0; i < m_originalString.size(); i++)
	{
		m_toLowerHash *= 31;
		m_toLowerHash += static_cast<unsigned int>(tolower(m_originalString[i]));
	}
}
