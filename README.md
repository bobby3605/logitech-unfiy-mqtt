## Logitech Unify MQTT

logitech-unify-mqtt is a user-mode driver for sending connection status information from a logitech unify receiver to an mqtt broker.\
This is intended to be used to track the power state of logitech unify devices so that home assistant can perform actions from that status.\
It currently only supports Windows, but Linux is possible if there's a demand for it.

It can handle unplugging and plugging back in a unify receiver, along with 6 devices paired to that receiver (maximum that a receiver allows).\
It can only handle a single receiver.\
It will report the name and power state of each device to MQTT when the power state changes.\
There are 3 power states: disconnected, connected, and power save.\
Power save occurs when a device turns itself off to save power (k400 plus does this after 5 minutes).\
Currently, it only displays the status to the console.

### TODO:
MQTT integration\
Conversion to a service, or maybe a task tray application\
