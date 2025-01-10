#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <iostream>
#include "unify_status.h"
#include <thread>
#include <vector>
#include <optional>
#include <lmcons.h>

std::optional<std::string> find_hid_path(LPGUID hid_guid, HIDDevicePath path_to_find) {
	HDEVINFO info = SetupDiGetClassDevsW(hid_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	SP_DEVICE_INTERFACE_DATA device_data;
	device_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	std::string device_path;
	// SetupDiEnumDeviceInterfaces returns false when i is greater than the number of devices
	for (int i = 0; SetupDiEnumDeviceInterfaces(info, NULL, hid_guid, i, &device_data); ++i) {
		// Get required size
		DWORD required_size;
		SetupDiGetDeviceInterfaceDetailA(info, &device_data, NULL, 0, &required_size, NULL);
		// allocate enough space for the device path
		PSP_DEVICE_INTERFACE_DETAIL_DATA_A device_detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required_size);
		// check for null malloc
		if (device_detail_data == NULL) {
			debug_log << "null malloc" << std::endl;
			exit(1);
		}
		else {
			device_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
			// get the device path
			SetupDiGetDeviceInterfaceDetailA(info, &device_data, device_detail_data, required_size, NULL, NULL);
			device_path = device_detail_data->DevicePath;
			// Check if the device path is correct
			if (device_path.find(path_to_find.vid) != std::string::npos) {
				if (device_path.find(path_to_find.pid) != std::string::npos) {
					if (device_path.find(path_to_find.mi) != std::string::npos) {
						if (device_path.find(path_to_find.col) != std::string::npos) {
							free(device_detail_data);
							return device_path;
						}
					}
				}
			}
			// free the device data so it can be alloced again if needed
			free(device_detail_data);
		}
	}
}

void find_and_wait_on_receiver() {
	GUID hid_guid;
	HidD_GetHidGuid(&hid_guid);
	// wait until receiver is found
	while (true) {
		std::optional<std::string> primary_path = find_hid_path(&hid_guid, unify_hid_primary);
		if (primary_path.has_value()) {
			unify_primary_path = primary_path.value();
			std::optional<std::string> long_responder_path = find_hid_path(&hid_guid, unify_hid_long_responder);
			if (long_responder_path.has_value()) {
				unify_long_responder_path = long_responder_path.value();
				break;
			}
		}
		else {
			// check for receiver every second
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
}

bool read_receiver(HANDLE receiver, std::vector<unsigned char>& buffer, LPDWORD bytes_read = NULL){
	return ReadFile(receiver, buffer.data(), buffer.capacity(), bytes_read, NULL);
} 
bool write_receiver(HANDLE receiver, std::vector<unsigned char> const& buffer){
	return WriteFile(receiver, buffer.data(), buffer.capacity(), NULL, NULL);
}


void enable_wireless_notifications(HANDLE receiver) {
	// Ensure wireless notifications are enabled by writing to 0x00 register
	// https://lekensteyn.nl/files/logitech/logitech_hidpp10_specification_for_Unifying_Receivers.pdf
	const std::vector<unsigned char> enable_notifications_cmd = {0x10, 0xff, 0x80, 0x00, 0x00, 0x01, 0x00};
	if (!write_receiver(receiver, enable_notifications_cmd)) {
		debug_log << "warning: failed to enable notifications on receiver error: " << GetLastError() << std::endl;
	}
	else {
		std::vector<unsigned char> response_buffer(7);	
		// Read and check success response from receiver
		if (read_receiver(receiver, response_buffer)) {
			// If the response isn't the expected response for enable success,
			// print a warning message
			if (!(response_buffer[0] == 0x10 && response_buffer[1] == 0xff && response_buffer[2] == 0x80 && response_buffer[3] == 0x00)) {
				debug_log << "warning: failed to confirm wireless notifications are enabled" << std::endl;
			}
		}
	}
}

// NOTE:
// device_id should be zero indexed
std::string get_device_name(HANDLE receiver, HANDLE long_responder, unsigned char device_id){
	// get name command needs the 7th bit set in a 0 indexed device id
	unsigned char correct_id = device_id | 0x40;
	const std::vector<unsigned char> get_name_cmd = {0x10, 0xff, 0x83, 0xb5, correct_id, 0x00, 0x00};
	write_receiver(receiver, get_name_cmd);
	std::vector<unsigned char> name_response_buffer(20);
	// NOTE:
	// long_responder responds twice to the get_name_cmd
	// the first is unknown data, the second contains the name
	read_receiver(long_responder, name_response_buffer);
	read_receiver(long_responder, name_response_buffer);
	if(name_response_buffer[0] == 0x11 && name_response_buffer[1] == 0xff && name_response_buffer[2] == 0x83 && name_response_buffer[3] == 0xb5) {
		unsigned char name_length = name_response_buffer[5];
		// device name is from the 6th byte to the length in the 5th byte
		std::string name(name_response_buffer.begin() + 6, name_response_buffer.begin() + 6 + name_length);
		return name;
	}
	else {
		debug_log << "warning: failed to find name for device: " << (unsigned int)device_id << std::endl;
		return "unknown_name";
	}
}

void read_notifications(void (*callback)(unsigned int device_id)) {
	HANDLE receiver = CreateFileA(unify_primary_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	HANDLE long_responder = CreateFileA(unify_long_responder_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	enable_wireless_notifications(receiver);
	std::vector<unsigned char> read_buffer(notification_byte_size);
	DWORD bytes_read;
	// set the start time to 0 to ensure that a first connection isn't an erroneous powersave
	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
	start_time -= start_time.time_since_epoch();
	for (auto& device : devices_info) {
		device.last_connected_packet_time = start_time;
	}
	while (true) {
		// keep reading the device until it has an error
		if (read_receiver(receiver, read_buffer, &bytes_read)) {
			// get the time the packet was received
			std::chrono::steady_clock::time_point current_packet_time = std::chrono::steady_clock::now();
			// only process if notification_byte_size bytes were read
			if (bytes_read == read_buffer.capacity()) {
				// check if the data is a device connection status notification
				if (read_buffer[0] == 0x10 && read_buffer[2] == 0x41) {
					// devices are 1 indexed on the receiver,
					// convert the device id to be 0 indexed
					read_buffer[1] -= 1;
					DeviceData* device_info = &devices_info[read_buffer[1]];
					// 0xa1 is device connection
					if (read_buffer[4] == 0xa1) {
						device_info->last_connected_packet_time = std::chrono::steady_clock::now();
						device_info->status = CONNECTED;
						// Get the device name when it connects
						device_info->name = get_device_name(receiver, long_responder, read_buffer[1]);
					}
					// 0x61 is device disconnection
					else if (read_buffer[4] == 0x61) {
						auto packet_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_packet_time - device_info->last_connected_packet_time).count();
						// when the device goes into power saving mode,
						// it will send a connection message,
						// then 400ms later it will send a disconnection message
						// so if the time between two packets is less than 500ms,
						// the keyboard should be in power saving mode
						if (packet_time_ms < 500) {
							device_info->status = POWERSAVE;
						}
						else {
							device_info->status = DISCONNECTED;
						}
					}	
					// NOTE:
					// callback will run twice in the case of a power save
					// first for the connected status,
					// then again for the powersave status
					callback(read_buffer[1]);
				}
			}
		}
		else {
			// break out of the loop if there's an error
			// should only happen when the receiver is unplugged
			DWORD error = GetLastError();
			if (error != ERROR_DEVICE_NOT_CONNECTED) {
				debug_log << "failed to read receiver with error: " << GetLastError() << std::endl;
			}
			break;
		}
	}
	CloseHandle(receiver);
	CloseHandle(long_responder);
}

void process_device_status(unsigned int device_id){
	debug_log << device_id << ": " << devices_info[device_id].name << ": " << status_to_string.at(devices_info[device_id].status) << std::endl;
}

int main() {
	// Setup config.ini and debug.log in appdata
	DWORD username_length = UNLEN + 1;
	char username[UNLEN + 1];
	GetUserNameA(username, &username_length);
	std::string appdata_path("C:\\Users\\" + std::string(username) + "\\AppData\\Local\\logitech-unify-mqtt");
	if (CreateDirectoryA(appdata_path.c_str(), NULL) || ERROR_ALREADY_EXISTS == GetLastError()) {
		std::string config_path = appdata_path + "\\config.ini";
		std::string log_path = appdata_path + "\\debug.log";
		HANDLE config_file = CreateFileA(config_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			WritePrivateProfileStringA("MQTT", "ip", "", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "port", "1883", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "username", "", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "password", "", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "prefix","homeassistant", config_path.c_str());
		}
		CloseHandle(config_file);
		std::vector<char> config_buffer(64);
		GetPrivateProfileStringA("MQTT","ip",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		std::string mqtt_ip(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","port","1883", config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		std::string mqtt_port(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","username",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		std::string mqtt_username(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","password",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		std::string mqtt_password(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","prefix",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		std::string mqtt_prefix(reinterpret_cast<char*>(config_buffer.data()));

		debug_log.open(log_path, std::ios::app);

		// Run the driver
		// Keep looping indefinitely,
		// handles the case of the receiver being unplugged and plugged back in
		while (true) {
			find_and_wait_on_receiver();
			read_notifications(process_device_status);
		}	
		debug_log.close();
	}
	else {
		std::cout << "failed to create appdata path" << std::endl;
		exit(1);
	}
}