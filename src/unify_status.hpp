#pragma once
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <windows.h>

enum DeviceStatus {
	CONNECTED,
	DISCONNECTED,
	POWERSAVE
};


struct DeviceData {
	DeviceStatus status = DISCONNECTED;
	std::string name = "";
	std::chrono::steady_clock::time_point last_connected_packet_time;
};



class UnifyStatus {
	struct HIDDevicePath {
		char vid[9];
		char pid[9];
		char mi[6];
		char col[6];
	};

	const HIDDevicePath unify_hid_primary{
		"vid_046d", "pid_c52b", "mi_02", "col01"
	};

	const HIDDevicePath unify_hid_long_responder{
		"vid_046d", "pid_c52b", "mi_02", "col02"
	};

	std::string unify_primary_path;
	std::string unify_long_responder_path;


	// Size of wireless device connection status notification
	const unsigned int notification_byte_size = 7;

	std::string mqtt_ip;
	std::string mqtt_port;
	std::string mqtt_username;
	std::string mqtt_password;
	std::string mqtt_prefix;

	std::ofstream debug_log;

	void find_and_wait_on_receiver();
	bool read_receiver(HANDLE receiver, std::vector<unsigned char>& buffer, LPDWORD bytes_read = NULL);
	bool write_receiver(HANDLE receiver, std::vector<unsigned char> const& buffer);
	void enable_wireless_notifications(HANDLE receiver);
	std::string get_device_name(HANDLE receiver, HANDLE long_responder, unsigned int device_id);
	void read_notifications();
	void process_device_status(unsigned int device_id);

public:
	UnifyStatus();
	~UnifyStatus();
	bool quit = false;
	void run();
	std::optional<std::string> find_hid_path(LPGUID hid_guid, HIDDevicePath path_to_find);
	std::vector<DeviceData> devices_info;
	const std::map<DeviceStatus, std::string> status_to_string{ {CONNECTED, "connected"}, {DISCONNECTED, "disconnected"}, {POWERSAVE, "powersave"} };
};
