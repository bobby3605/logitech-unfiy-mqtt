#include <iostream>
#include "unify_status.hpp"
#include <thread>
#include <vector>
#include <algorithm>
#include <optional>
#include <hidsdi.h>
#include <setupapi.h>
#include <lmcons.h>
#include "common.hpp"
#include "../external/json.hpp"
using json = nlohmann::json;

void UnifyStatus::print_bytes(std::vector<unsigned char> const& bytes) {
	for (const auto& byte : bytes) {
		debug_log << curr_time() << std::hex << (int)byte << " ";
	}
}
bool UnifyStatus::check_response(HANDLE usb, std::vector<unsigned char> const& bytes_to_check, std::vector<unsigned char>& response) {
		read_usb(usb, response);
		for (int i = 0; i < bytes_to_check.size(); ++i) {
			if (response[i] != bytes_to_check[i]) {
				break;
			}
			if (i == bytes_to_check.size() - 1) {
				return true;
			}
		}
		return false;
	}

std::string UnifyStatus::find_hid_path(LPGUID hid_guid, HIDDevicePath path_to_find) {
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
			debug_log << curr_time() << "null malloc" << std::endl;
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
	return "";
}

void UnifyStatus::find_and_wait_on_receiver() {
	GUID hid_guid;
	HidD_GetHidGuid(&hid_guid);
	// wait until receiver is found
	while (!quit) {
		unify_primary_path = find_hid_path(&hid_guid, unify_hid_primary);
		unify_responder_path = find_hid_path(&hid_guid, unify_hid_responder);
		if (unify_primary_path != "" && unify_responder_path != "") {
			break;
		}
		// check for receiver every second
		// debug log runs if either path check fails
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		debug_log << curr_time() << "waiting on receiver" << std::endl;
	}
}

bool UnifyStatus::read_usb(HANDLE usb, std::vector<unsigned char>& buffer, LPDWORD bytes_read){
	return ReadFile(usb, buffer.data(), buffer.capacity(), bytes_read, NULL);
} 
bool UnifyStatus::write_usb(HANDLE usb, std::vector<unsigned char> const& buffer){
	return WriteFile(usb, buffer.data(), buffer.capacity(), NULL, NULL);
}


void UnifyStatus::enable_wireless_notifications() {
	// Ensure wireless notifications are enabled by writing to 0x00 register
	// https://lekensteyn.nl/files/logitech/logitech_hidpp10_specification_for_Unifying_Receivers.pdf
	const std::vector<unsigned char> enable_notifications_cmd = {0x10, 0xff, 0x80, 0x00, 0x00, 0x01, 0x00};
	write_usb(receiver, enable_notifications_cmd);
	std::vector<unsigned char> response_buffer(7);	
	// Read and check success response from receiver
	const std::vector<unsigned char> enable_notifications_success = {0x10, 0xff, 0x80, 0x00};
	if (!check_response(receiver, enable_notifications_success, response_buffer)) {
		debug_log << curr_time() << "warning: failed to confirm enabled notifications: ";
		print_bytes(response_buffer);
		debug_log << curr_time() << std::endl;
	}
	// For some reason, the HID driver sends a notification read command as well,
	// and so this driver needs to wait for the notification read response
	const std::vector<unsigned char> notification_read_success = {0x10, 0xff, 0x81, 0x00};
	if (!check_response(receiver, notification_read_success, response_buffer)) {
		debug_log << curr_time() << "warning: failed to confirm notification read after enable: ";
		print_bytes(response_buffer);
		debug_log << curr_time() << std::endl;
	}
}

void UnifyStatus::get_paired_devices() {
	const std::vector<unsigned char> get_connected_devices_count_cmd = {0x10, 0xff, 0x81, 0x02, 0x00, 0x00, 0x00};
	write_usb(receiver, get_connected_devices_count_cmd);
	const std::vector<unsigned char> connected_devices_response_check = {0x10, 0xff, 0x81, 0x02, 0x00};
	std::vector<unsigned char> response_buffer(7);
	if (check_response(receiver, connected_devices_response_check, response_buffer)) {
		unsigned int paired_devices_count = (int)response_buffer[5];
		devices_info.resize(paired_devices_count);
		for (int i = 0; i < paired_devices_count; ++i) {
			devices_info[i].name = get_device_name(i);
		}
	}
	else {
		debug_log << curr_time() << "warning: failed to get paired devices: ";
		print_bytes(response_buffer);
		debug_log << curr_time() << std::endl;
	}
}

// NOTE:
// device_id should be zero indexed
std::string UnifyStatus::get_device_name(unsigned int device_id){
	// get name command needs the 7th bit set in a 0 indexed device id
	unsigned char correct_id = device_id | 0x40;
	const std::vector<unsigned char> get_name_cmd = {0x10, 0xff, 0x83, 0xb5, correct_id, 0x00, 0x00};
	write_usb(receiver, get_name_cmd);
	// For some reason, after the device sends a connected message,
	// the receiver sends an undocumented packet before sending the name
	if (devices_info[device_id].status == CONNECTED) {
		const std::vector<unsigned char> undocumented_response_check = { 0x11, 0x01, 0x04 };
		std::vector<unsigned char> undocumented_response(20);
		if (!check_response(responder, undocumented_response_check, undocumented_response)) {
			debug_log << curr_time() << "warning: failed to read undocumented response: ";
			print_bytes(undocumented_response);
			debug_log << curr_time() << std::endl;
		}
	}
	std::vector<unsigned char> name_response_buffer(20);
	const std::vector<unsigned char> name_response_check = { 0x11, 0xff, 0x83, 0xb5 };
	if(check_response(responder, name_response_check, name_response_buffer)) {
		unsigned char name_length = name_response_buffer[5];
		// device name is from the 6th byte to the length in the 5th byte
		std::string name(name_response_buffer.begin() + 6, name_response_buffer.begin() + 6 + name_length);
		return name;
	}
	else {
		debug_log << curr_time() << "warning: failed to find name for device: " << (unsigned int)device_id << " bytes: ";
		print_bytes(name_response_buffer);
		debug_log << curr_time() << std::endl;
		return "";
	}
}

void UnifyStatus::update_mqtt_discovery() {
	json payload;
	payload["dev"] = { {"ids", "logitech-unify-mqtt"}, {"name", "Logitech Unify Receiver"}};
	payload["o"] = { {"name", "logitech-unify-mqtt"}, {"url", "https://github.com/bobby3605/logitech-unfiy-mqtt"}};
	payload["qos"] = 0;
	payload["cmps"] = json::object();
	for (int i = 0; i < devices_info.size(); ++i) {
		std::string dev = "dev" + std::to_string(i);
		payload["cmps"][dev] = {
			{ "p", "sensor" },
			{ "state_topic", mqtt_prefix + dev + "/power_state" },
			{ "unique_id", dev},
			{ "name", devices_info[i].name}
		};
	}
	std::string topic = mqtt_prefix + "config";
	_mqtt->publish(topic, payload.dump(), debug_log);
}

void UnifyStatus::process_device_status(unsigned int device_id){
	std::string topic = mqtt_prefix + "dev" + std::to_string(device_id) + "/power_state";
	_mqtt->publish(topic, status_to_string.at(devices_info[device_id].status), debug_log);
	debug_log << "sent: " << topic << ": " << status_to_string.at(devices_info[device_id].status) << std::endl;
}

void UnifyStatus::read_notifications() {
	receiver = CreateFileA(unify_primary_path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	responder = CreateFileA(unify_responder_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	enable_wireless_notifications();
	std::vector<unsigned char> tmp(1);
	get_paired_devices();
	update_mqtt_discovery();
	std::vector<unsigned char> read_buffer(notification_byte_size);
	DWORD bytes_read;
	// set the start time to 0 to ensure that a first connection isn't an erroneous powersave
	std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
	start_time -= start_time.time_since_epoch();
	for (auto& device : devices_info) {
		device.last_connected_packet_time = start_time;
	}
	while (!quit) {
		// keep reading the device until it has an error
		if (read_usb(receiver, read_buffer, &bytes_read)) {
			// get the time the packet was received
			std::chrono::steady_clock::time_point current_packet_time = std::chrono::steady_clock::now();
			// only process if notification_byte_size bytes were read
			if (bytes_read == read_buffer.capacity()) {
				// check if the data is a device connection status notification
				if (read_buffer[0] == 0x10 && read_buffer[2] == 0x41) {
					// devices are 1 indexed on the receiver,
					if (read_buffer[1] > devices_info.size()) {
						devices_info.resize(read_buffer[1]);
						update_mqtt_discovery();
					}
					// convert the device id to be 0 indexed
					unsigned int device_id = read_buffer[1] - 1;
					DeviceData* device_info = &devices_info[device_id];
					// 0xa1 is device connection
					if (read_buffer[4] == 0xa1) {
						device_info->last_connected_packet_time = std::chrono::steady_clock::now();
						device_info->status = CONNECTED;
						if (device_info->name == "") {
							device_info->name = get_device_name(device_id);
						}
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
					// process_device_status will run twice in the case of a power save
					// first for the connected status,
					// then again for the powersave status
					process_device_status(device_id);
				}
			}
		}
		else {
			// break out of the loop if there's an error
			DWORD error = GetLastError();
			// not connected and aborted are handled error cases
			if (!(error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_OPERATION_ABORTED)) {
				debug_log << curr_time() << "failed to read receiver with error: " << GetLastError() << std::endl;
			}
			break;
		}
	}
	CloseHandle(receiver);
	CloseHandle(responder);
}

void UnifyStatus::run() {
	// Run the driver
	// Keep looping indefinitely,
	// handles the case of the receiver being unplugged and plugged back in
	while (!quit) {
		find_and_wait_on_receiver();
		read_notifications();
	}	
}

UnifyStatus::UnifyStatus() {
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
			WritePrivateProfileStringA("MQTT", "address", "", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "username", "", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "password", "", config_path.c_str());
			WritePrivateProfileStringA("MQTT", "discovery-prefix","homeassistant", config_path.c_str());
		}
		CloseHandle(config_file);
		std::vector<char> config_buffer(64);
		GetPrivateProfileStringA("MQTT","address",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		mqtt_address = std::string(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","username",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		mqtt_username = std::string(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","password",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		mqtt_password = std::string(reinterpret_cast<char*>(config_buffer.data()));
		GetPrivateProfileStringA("MQTT","discovery-prefix",NULL,config_buffer.data(), config_buffer.capacity(), config_path.c_str());
		mqtt_prefix = std::string(reinterpret_cast<char*>(config_buffer.data()));
		mqtt_prefix += "/device/logitech-unify-mqtt/";
		debug_log.open(log_path, std::ios::app);
	}
	else {
		std::cout << "failed to create appdata path" << std::endl;
	}
	_mqtt = new UnifyMQTT(mqtt_address, mqtt_username, mqtt_password, debug_log);
}

UnifyStatus::~UnifyStatus() {
	debug_log.close();
	delete _mqtt;
}
