#pragma once
#include <string>

class HashedCaseInsensitiveString
{
public:
	HashedCaseInsensitiveString(const std::string& fromStdString);
	HashedCaseInsensitiveString(const char* fromCString);

	bool operator<(const HashedCaseInsensitiveString& compare) const;
	bool operator==(const HashedCaseInsensitiveString& compare) const;

private:
	void CalculateStringHashValue();

public:
	std::string m_originalString;
	unsigned int m_toLowerHash = 0;
};