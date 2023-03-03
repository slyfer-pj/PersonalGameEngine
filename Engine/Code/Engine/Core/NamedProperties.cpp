#include "Engine/Core/NamedProperties.hpp"

NamedProperties::~NamedProperties()
{
	for (auto iter = m_namedProperties.begin(); iter != m_namedProperties.end(); ++iter)
	{
		NamedPropertyBase*& propertyBase = iter->second;
		if (propertyBase)
		{
			delete propertyBase;
			propertyBase = nullptr;
		}
	}

	m_namedProperties.clear();
}

void NamedProperties::SetValue(const std::string& keyName, const char* keyValue)
{
	std::string keyValueAsString = keyValue;
	SetValue(keyName, keyValueAsString);
}

std::string NamedProperties::GetValue(const std::string& keyName, const char* defaultValue) const
{
	std::string defaultValueAsString = defaultValue;
	return GetValue(keyName, defaultValueAsString);
}

