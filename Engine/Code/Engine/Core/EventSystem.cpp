#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

EventSystem* g_theEventSystem = nullptr;

EventSystem::EventSystem(const EventSystemConfig& config)
{
	m_config = config;
}

EventSystem::~EventSystem()
{

}

void EventSystem::Startup()
{

}

void EventSystem::Shutdown()
{

}

void EventSystem::BeginFrame()
{

}

void EventSystem::EndFrame()
{

}

void EventSystem::SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
	std::map<HashedCaseInsensitiveString, SubscriptionList>::const_iterator iter;
	m_subscriptionListMutex.lock();
	iter = m_subscriptionListByEventName.find(eventName);
	if (iter == m_subscriptionListByEventName.end())
	{
		SubscriptionList subList;
		EventSubscriptionBase* eventSub = new Function_EventSubscription(functionPtr);
		subList.push_back(eventSub);
		m_subscriptionListByEventName[eventName] = subList;
	}
	else
	{
		SubscriptionList subList = iter->second;
		EventSubscriptionBase* eventSub = new Function_EventSubscription(functionPtr);
		subList.push_back(eventSub);
	}
	m_subscriptionListMutex.unlock();
}

void EventSystem::UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
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
			EventSubscriptionBase* callbackBase = subList[i];
			if (callbackBase->IsForFunction(functionPtr))
			{
				delete callbackBase;
				std::swap(callbackBase, subList[subList.size() - 1]);
				subList.pop_back();
			}
		}
		unsubscribeSuccess = true;
	}
	m_subscriptionListMutex.unlock();

	if(!unsubscribeSuccess)
	{
		GUARANTEE_OR_DIE(false, "Trying to unsubscribe from event that doesn't exist!");
	}
}

void EventSystem::FireEvent(const std::string& eventName, EventArgs& args)
{
	std::map<HashedCaseInsensitiveString, SubscriptionList>::const_iterator iter;
	bool fireEventSuccess = false;
	SubscriptionList subList;
	m_subscriptionListMutex.lock();
	iter = m_subscriptionListByEventName.find(eventName);
	if (iter != m_subscriptionListByEventName.end())
	{
		subList = iter->second;
	}
	m_subscriptionListMutex.unlock();

	for (int i = 0; i < subList.size(); i++)
	{
		fireEventSuccess = true;
		EventSubscriptionBase* callback = subList[i];
		bool consumed = callback->Execute(args);
		if (consumed)
			break;
	}

	if(!fireEventSuccess)
	{
		DebuggerPrintf(Stringf("Trying to fire event %s which doesn't exist", eventName.c_str()).c_str());
	}
}

void EventSystem::FireEvent(const std::string& eventName)
{
	EventArgs args;
	FireEvent(eventName, args);
}

void EventSystem::GetRegisteredEventNames(std::vector<std::string>& outNames) const
{
	m_subscriptionListMutex.lock();
	for (auto itr = m_subscriptionListByEventName.begin(); itr != m_subscriptionListByEventName.end(); ++itr)
	{
		SubscriptionList subList = itr->second;
		if (subList.size() > 0)
		{
			outNames.push_back(itr->first.m_originalString);
		}
	}
	m_subscriptionListMutex.unlock();
}

void EventSystem::UnsubcribeObjectFromAllEvents(const void* object)
{
	for (auto iter = m_subscriptionListByEventName.begin(); iter != m_subscriptionListByEventName.end(); ++iter)
	{
		SubscriptionList& subList = iter->second;
		for (int i = 0; i < subList.size(); i++)
		{
			m_subscriptionListMutex.lock();
			if (subList[i]->IsForObject(object))
			{
				delete subList[i];
				std::swap(subList[i], subList[subList.size() - 1]);
				subList.pop_back();
			}
			m_subscriptionListMutex.unlock();
		}
	}
}

void SubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->SubscribeEventCallbackFunction(eventName, functionPtr);
	}
}

void UnsubscribeEventCallbackFunction(const std::string& eventName, EventCallbackFunction functionPtr)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->UnsubscribeEventCallbackFunction(eventName, functionPtr);
	}
}

void FireEvent(const std::string& eventName, EventArgs& args)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->FireEvent(eventName, args);
	}
}

void FireEvent(const std::string& eventName)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->FireEvent(eventName);
	}
}

void UnsubscribeObjectFromAllEvents(const void* object)
{
	if (g_theEventSystem)
	{
		g_theEventSystem->UnsubcribeObjectFromAllEvents(object);
	}
}
