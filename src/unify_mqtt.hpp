#pragma once
#include <MQTTClient.h>
#include <string>
#include <fstream>

class UnifyMQTT{
	MQTTClient _client;
public:
	UnifyMQTT(const std::string& address, const std::string& username, const std::string& password, std::ofstream& debug_log);
	~UnifyMQTT();
	void publish(const std::string& topic, const std::string& message, bool const& retained, std::ofstream& debug_log);
};
