## Logitech Unify MQTT

logitech-unify-mqtt is a user-mode driver for sending connection status information from a logitech unify receiver to an mqtt broker.\
This is intended to be used to track the power state of logitech unify devices so that home assistant can perform actions from that status.\
It currently only supports Windows, but Linux is possible if there's a demand for it.

It can handle unplugging and plugging back in a unify receiver, along with 6 devices paired to that receiver (maximum that a receiver allows).\
It can only handle a single receiver.\
It will report the name and power state of each device to MQTT when the power state changes.\
There are 3 power states: disconnected, connected, and power save.\
Power save occurs when a device turns itself off to save power (k400 plus does this after 5 minutes).

The status is shown in a task tray icon by right clicking on it.\
There are 'Reload' and 'Exit' options on the popup menu.\
The config file and debug logs are stored in C:\Users\username\AppData\Local\logitech-unify-mqtt\

### TODO:
MQTT integration

### Known limitations:
When the driver starts, if a device is connected, it will display as disconnected.\
The driver can get the name of the device at the start, but it will show as disconnected.\
So whenever the driver is started/reloaded, devices will have to be disconnected and re-connected to the receiver in order to get the correct status.\
The receiver does not have a command (at least not a documented one) that will give the connected/disconnected status of a device.\
The receiver only sends connection status information when the device connects or disconnects.
