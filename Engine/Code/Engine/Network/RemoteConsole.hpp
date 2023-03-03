#pragma once
#include "Engine/Network/NetAddress.hpp"
#include <vector>
#include <string>

class DevConsole;
class Renderer;
class TCPServer;
class TCPConnection;
struct AABB2;
class BitmapFont;

enum class ConnectionState
{
	DISCONNECTED,
	HOSTING,
	CLIENT
};

struct RemoteConsoleConfig
{
	DevConsole* m_console = nullptr;
};

class RemoteConsole
{
public:
	RemoteConsole(const RemoteConsoleConfig& config);

	void Startup();
	void Shutdown();

	void Update();
	void Render(Renderer& renderer, const AABB2& bounds, BitmapFont& font);
	void ProcessConnections();
	void ProcessMessage(TCPConnection* connection, int connectionIndex);
	void AttemptToHost(uint16_t port, bool loopbackIfFailed);
	void ConnectToHost(NetAddress hostAddress);
	void SendCommand(int connIdx, const std::string& cmd, bool isEcho);
	void BroadcastCommand(const std::string& cmd);
	void CloseAllConnections();
	void CloseConnectionAtIndex(int connectionIndex, bool addToKickList = false);
	void StopHosting();
	void AddCommandToBlacklist(const std::string& command);
	void SetAutoJoinOrHostState(bool state);

private:
	ConnectionState m_connectionState = ConnectionState::DISCONNECTED;
	DevConsole* m_devConsole = nullptr;
	TCPServer* m_server = nullptr;
	std::string m_serverExternalAddess;
	std::vector<TCPConnection*> m_connections = {};
	std::vector<NetAddress> m_kickList = {};
	std::vector<std::string> m_commandBlacklist = {};
	bool m_autoJoinOrHost = true;

private:
	std::string GetCurrentStateString() const;
	bool IsIncomingConnectionOnKickList(NetAddress addr) const;
	bool CheckIfCommandIsAllowed(const char* command);
};