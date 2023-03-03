#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "Engine/Core/NamedProperties.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/HashedCaseInsensitiveString.hpp"

typedef NamedProperties EventArgs;
typedef bool (*EventCallbackFunction) (EventArgs& args);

extern EventSystem* g_theEventSystem;

void SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
void UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
void FireEvent(const std::string& eventName, EventArgs& args);
void FireEvent(const std::string& eventName);

template <typename T_ObjectType, typename T_MethodType>
void SubscribeEventCallbackFunctionObjectMethod(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtr);

template <typename T_ObjectType, typename T_MethodType>
void UnsubscribeMethodEventCallbackFunctionForObject(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtrToUnsub);

template <typename T_ObjectType, typename T_MethodType>
void UnsubscribeMethodEventCallbackFunctionForObject(T_ObjectType& object, T_MethodType methodPtrToUnsub);

void UnsubscribeObjectFromAllEvents(const void* object);

class EventRecepient
{
public:
	virtual ~EventRecepient()
	{
		UnsubscribeObjectFromAllEvents(reinterpret_cast<const void*>(this));
	}
};

struct EventSubscriptionBase
{
public:
	EventSubscriptionBase() = default;
	virtual ~EventSubscriptionBase() = default;
	virtual bool Execute(EventArgs& args) = 0;
	virtual bool IsForObject(const void* objectToCompare) const { UNUSED(objectToCompare); return false; }
	virtual bool IsForFunction(const void* functionPtr) const { UNUSED(functionPtr); return false; }
};

template <typename T_ObjectType>
struct Methods_EventSubscription : public EventSubscriptionBase
{
	typedef bool (T_ObjectType::*MethodPtr)(EventArgs& args);

public:
	Methods_EventSubscription(T_ObjectType& object, MethodPtr methodCallback)
		:m_methodcallback(methodCallback), m_object(object)
	{
	}

	virtual bool Execute(EventArgs& args) override
	{
		return (m_object.*m_methodcallback)(args);
	}
	virtual bool IsForObject(const void* objectToCompare) const override
	{
		if (objectToCompare == reinterpret_cast<void*>(&m_object))
			return true;

		return false;
	}
	bool IsForMethod(MethodPtr methodPtr) const 
	{
		if (m_methodcallback == methodPtr)
			return true;

		return false;
	}

public:
	T_ObjectType& m_object;
	MethodPtr m_methodcallback = nullptr;
};


struct Function_EventSubscription : public EventSubscriptionBase
{
public:
	Function_EventSubscription(EventCallbackFunction eventCallback)
		:m_eventCallBack(eventCallback)
	{
	}

	virtual bool Execute(EventArgs& args) override
	{
		return m_eventCallBack(args);
	}

	virtual bool IsForFunction(const void* functionPtr) const override
	{
		if (m_eventCallBack == functionPtr)
			return true;

		return false;
	}

public:
	EventCallbackFunction m_eventCallBack = nullptr;
};

struct EventSystemConfig
{

};

typedef std::vector<EventSubscriptionBase*> SubscriptionList;

class EventSystem
{
public:
	EventSystem(const EventSystemConfig& config);
	~EventSystem();
	void Startup();
	void Shutdown();
	void BeginFrame();
	void EndFrame();

	void SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
	void UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr);
	void FireEvent(const std::string& eventName, EventArgs& args);
	void FireEvent(const std::string& eventName);
	void GetRegisteredEventNames(std::vector<std::string>& outNames) const;

	template <typename T_ObjectType, typename T_MethodType>
	void SubscribeEventCallbackFunctionObjectMethod(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtr);

	template <typename T_ObjectType, typename T_MethodType>
	void UnsubscribeMethodEventCallbackFunctionForObject(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtrToUnsub);

	template <typename T_ObjectType, typename T_MethodType>
	void UnsubscribeMethodEventCallbackFunctionForObject(T_ObjectType& object, T_MethodType methodPtrToUnsub);

	void UnsubcribeObjectFromAllEvents(const void* object);

protected:
	EventSystemConfig m_config;
	std::map<HashedCaseInsensitiveString, SubscriptionList> m_subscriptionListByEventName;
	mutable std::mutex m_subscriptionListMutex;
};

template <typename T_ObjectType, typename T_MethodType>
void EventSystem::SubscribeEventCallbackFunctionObjectMethod(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtr)
{
	std::map<HashedCaseInsensitiveString, SubscriptionList>::iterator iter;
	m_subscriptionListMutex.lock();
	iter = m_subscriptionListByEventName.find(eventName);
	if (iter == m_subscriptionListByEventName.end())
	{
		SubscriptionList subList;
		EventSubscriptionBase* eventSub = new Methods_EventSubscription<T_ObjectType>(object, methodPtr);
		subList.push_back(eventSub);
		m_subscriptionListByEventName[eventName] = subList;
	}
	else
	{
		SubscriptionList& subList = iter->second;
		EventSubscriptionBase* eventSub = new Methods_EventSubscription<T_ObjectType>(object, methodPtr);
		subList.push_back(eventSub);
	}
	m_subscriptionListMutex.unlock();
}

template <typename T_ObjectType, typename T_MethodType>
void EventSystem::UnsubscribeMethodEventCallbackFunctionForObject(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtrToUnsub)
{
	std::map<HashedCaseInsensitiveString, SubscriptionList>::iterator iter;
	bool unsubscribeSuccess = false;
	m_subscriptionListMutex.lock();
	iter = m_subscriptionListByEventName.find(eventName);
	if (iter != m_subscriptionListByEventName.end())
	{
		SubscriptionList& subList = iter->second;
		for (int i = 0; i < subList.size(); i++)
		{
			if (subList[i]->IsForObject(reinterpret_cast<void*>(&object)))
			{
				Methods_EventSubscription<T_ObjectType>* methodSub = dynamic_cast<Methods_EventSubscription<T_ObjectType>*>(subList[i]);
				if (methodSub && methodSub->IsForMethod(methodPtrToUnsub))
				{
					delete subList[i];
					std::swap(subList[i], subList[subList.size() - 1]);
					subList.pop_back();
				}
			}
		}
		unsubscribeSuccess = true;
	}
	m_subscriptionListMutex.unlock();

	if (!unsubscribeSuccess)
	{
		GUARANTEE_OR_DIE(false, "Trying to unsubscribe from event that doesn't exist!");
	}
}

template <typename T_ObjectType, typename T_MethodType>
void EventSystem::UnsubscribeMethodEventCallbackFunctionForObject(T_ObjectType& object, T_MethodType methodPtrToUnsub)
{
	m_subscriptionListMutex.lock();
	for (auto iter = m_subscriptionListByEventName.begin(); iter != m_subscriptionListByEventName.end(); ++iter)
	{
		SubscriptionList& subList = iter->second;
		for (int i = 0; i < subList.size(); i++)
		{
			if (subList[i]->IsForObject(reinterpret_cast<void*>(&object)))
			{
				Methods_EventSubscription<T_ObjectType>* methodSub = dynamic_cast<Methods_EventSubscription<T_ObjectType>*>(subList[i]);
				if (methodSub && methodSub->IsForMethod(methodPtrToUnsub))
				{
					delete subList[i];
					std::swap(subList[i], subList[subList.size() - 1]);
					subList.pop_back();
				}
			}
		}
	}
	m_subscriptionListMutex.unlock();
}

template <typename T_ObjectType, typename T_MethodType>
void SubscribeEventCallbackFunctionObjectMethod(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtr)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->SubscribeEventCallbackFunctionObjectMethod(eventName, object, methodPtr);
	}
}

template <typename T_ObjectType, typename T_MethodType>
void UnsubscribeMethodEventCallbackFunctionForObject(const std::string& eventName, T_ObjectType& object, T_MethodType methodPtrToUnsub)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->UnsubscribeMethodEventCallbackFunctionForObject(eventName, object, methodPtrToUnsub);
	}
}

template <typename T_ObjectType, typename T_MethodType>
void UnsubscribeMethodEventCallbackFunctionForObject(T_ObjectType& object, T_MethodType methodPtrToUnsub)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->UnsubscribeMethodEventCallbackFunctionForObject(object, methodPtrToUnsub);
	}
}
