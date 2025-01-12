#include "unify_mqtt.hpp"
#include "common.hpp"

UnifyMQTT::UnifyMQTT(const std::string& address, const std::string& username, const std::string& password, std::ofstream& debug_log) {
	int rc = MQTTClient_create(&_client, address.c_str(), "logitech-unify-mqtt", MQTTCLIENT_PERSISTENCE_NONE, NULL);
	if (rc != MQTTCLIENT_SUCCESS) {
		debug_log << curr_time() << "Failed to create MQTT client: " << rc << std::endl;
	}
	MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;
	options.username = username.c_str();
	options.password = password.c_str();
	options.keepAliveInterval = 60;
	rc = MQTTClient_connect(_client, &options);
	if (rc != MQTTCLIENT_SUCCESS) {
		debug_log << curr_time() << "Failed to connect to MQTT server: " << rc << std::endl;
	}
		
}
UnifyMQTT::~UnifyMQTT() {
	MQTTClient_disconnect(_client, 100);
	MQTTClient_destroy(&_client);
}
void UnifyMQTT::publish(const std::string& topic, const std::string& message, std::ofstream& debug_log) {
	MQTTClient_message mqtt_msg = MQTTClient_message_initializer;
	mqtt_msg.payload = (void*)message.c_str();
	mqtt_msg.payloadlen = message.size();
	mqtt_msg.qos = 0;
	mqtt_msg.retained = false;
	int rc = MQTTClient_publishMessage(_client, topic.c_str(), &mqtt_msg, NULL);
	if (rc != MQTTCLIENT_SUCCESS) {
		debug_log << curr_time() << curr_time << "Failed to publish MQTT message: " << rc << " " << topic << " " << message << std::endl;
	}
}
