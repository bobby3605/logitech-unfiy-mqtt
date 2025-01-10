#pragma once
#include <map>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>

struct HIDDevicePath {
	char vid[9];
	char pid[9];
	char mi[6];
	char col[6];
};

const HIDDevicePath unify_hid_primary {
	"vid_046d", "pid_c52b", "mi_02", "col01"
};

const HIDDevicePath unify_hid_long_responder {
	"vid_046d", "pid_c52b", "mi_02", "col02"
};

std::string unify_primary_path;
std::string unify_long_responder_path;


// Size of wireless device connection status notification
const unsigned int notification_byte_size = 7;

enum DeviceStatus {
	CONNECTED,
	DISCONNECTED,
	POWERSAVE
};

const std::map<DeviceStatus, std::string> status_to_string{ {CONNECTED, "connected"}, {DISCONNECTED, "disconnected"}, {POWERSAVE, "powersave"}};

struct DeviceData{
	DeviceStatus status;
	std::string name;
	std::chrono::steady_clock::time_point last_connected_packet_time;
};

// receiver supports a maximum of 6 devices
std::vector<DeviceData> devices_info(6);

std::ofstream debug_log;
