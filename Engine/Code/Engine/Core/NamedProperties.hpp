#pragma once
#include <map>
#include <string>
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/HashedCaseInsensitiveString.hpp"

class NamedPropertyBase
{
public:
	NamedPropertyBase() = default;
	virtual ~NamedPropertyBase() = default;
};

template <typename T>
class TypedNameProperty : public NamedPropertyBase
{
public:
	TypedNameProperty(const T& value)
		:m_value(value)
	{
	}

	virtual ~TypedNameProperty() = default;

public:
	T m_value;
};


class NamedProperties
{
public:
	~NamedProperties();

	template <typename T_PropertyType>
	void SetValue(const std::string& keyName, const T_PropertyType& keyValue);
	void SetValue(const std::string& keyName, const char* keyValue);

	template <typename T_PropertyType>
	T_PropertyType GetValue(const std::string& keyName, const T_PropertyType& defaultValue) const;
	std::string GetValue(const std::string& keyName, const char* defaultValue) const;

private:
	std::map<HashedCaseInsensitiveString, NamedPropertyBase*> m_namedProperties;
};

template <typename T_PropertyType>
void NamedProperties::SetValue(const std::string& keyName, const T_PropertyType& keyValue)
{
	NamedPropertyBase* baseProperty = m_namedProperties[keyName];
	if (baseProperty != nullptr)
	{
		TypedNameProperty<T_PropertyType>* typedProperty = dynamic_cast<TypedNameProperty<T_PropertyType>*>(baseProperty);
		if (typedProperty != nullptr)
		{
			delete baseProperty;
		}
		else
		{
			ERROR_AND_DIE("Trying to set value of a key which is associated to a different type");
		}
	}

	NamedPropertyBase* propertyValue = new TypedNameProperty<T_PropertyType>(keyValue);
	m_namedProperties[keyName] = propertyValue;
}

template <typename T_PropertyType>
T_PropertyType NamedProperties::GetValue(const std::string& keyName, const T_PropertyType& defaultValue) const
{
	std::map<HashedCaseInsensitiveString, NamedPropertyBase*>::const_iterator valueFound = m_namedProperties.find(keyName);
	if (valueFound != m_namedProperties.end())
	{
		TypedNameProperty<T_PropertyType>* typedProperty = dynamic_cast<TypedNameProperty<T_PropertyType>*>(valueFound->second);
		if (typedProperty != nullptr)
		{
			return typedProperty->m_value;
		}
	}

	return defaultValue;
}
