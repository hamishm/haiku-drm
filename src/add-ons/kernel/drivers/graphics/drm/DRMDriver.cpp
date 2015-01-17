/*
 * Copyright 2015, Hamish Morrison <hamishm53@gmail.com>
 * Distributed under the terms of the MIT License.
 */


#include <file_systems/ram_disk/ram_disk.h>

#include <device_manager.h>
#include <Drivers.h>


//#define TRACE_CHECK_SUM_DEVICE
#ifdef TRACE_CHECK_SUM_DEVICE
#	define TRACE(x...)	dprintf(x)
#else
#	define TRACE(x) do {} while (false)
#endif


// parameters for the DMA resource
static const uint32 kDMAResourceBufferCount			= 16;
static const uint32 kDMAResourceBounceBufferCount	= 16;

static const char* const kDriverModuleName = "graphics/drm";

struct device_manager_info* sDeviceManager;



static float
ram_disk_driver_supports_device(device_node* parent)
{
	const char* bus = NULL;
	if (sDeviceManager->get_attr_string(parent, B_DEVICE_BUS, &bus, false)
			== B_OK
		&& strcmp(bus, "generic") == 0) {
		return 0.8;
	}

	return -1;
}


static status_t
ram_disk_driver_register_device(device_node* parent)
{
	device_attr attrs[] = {
		{B_DEVICE_PRETTY_NAME, B_STRING_TYPE,
			{string: "RAM Disk Control Device"}},
		{NULL}
	};

	return sDeviceManager->register_node(parent, kDriverModuleName, attrs, NULL,
		NULL);
}


static status_t
ram_disk_driver_init_driver(device_node* node, void** _driverCookie)
{
	uint64 deviceSize;
	if (sDeviceManager->get_attr_uint64(node, kDeviceSizeItem, &deviceSize,
			false) == B_OK) {
		int32 id = -1;
		sDeviceManager->get_attr_uint32(node, kDeviceIDItem, (uint32*)&id,
			false);
		if (id < 0)
			return B_ERROR;

		const char* filePath = NULL;
		sDeviceManager->get_attr_string(node, kFilePathItem, &filePath, false);

		RawDevice* device = new(std::nothrow) RawDevice(node);
		if (device == NULL)
			return B_NO_MEMORY;

		status_t error = device->Init(id, filePath, deviceSize);
		if (error != B_OK) {
			delete device;
			return error;
		}

		*_driverCookie = (Device*)device;
	} else {
		ControlDevice* device = new(std::nothrow) ControlDevice(node);
		if (device == NULL)
			return B_NO_MEMORY;

		*_driverCookie = (Device*)device;
	}

	return B_OK;
}


static void
ram_disk_driver_uninit_driver(void* driverCookie)
{
	Device* device = (Device*)driverCookie;
	if (RawDevice* rawDevice = dynamic_cast<RawDevice*>(device))
		free_raw_device_id(rawDevice->ID());
	delete device;
}


static status_t
ram_disk_driver_register_child_devices(void* driverCookie)
{
	Device* device = (Device*)driverCookie;
	return device->PublishDevice();
}


//	#pragma mark - control device


static status_t
ram_disk_control_device_init_device(void* driverCookie, void** _deviceCookie)
{
	*_deviceCookie = driverCookie;
	return B_OK;
}


static void
ram_disk_control_device_uninit_device(void* deviceCookie)
{
}


static status_t
ram_disk_control_device_open(void* deviceCookie, const char* path, int openMode,
	void** _cookie)
{
	*_cookie = deviceCookie;
	return B_OK;
}


static status_t
ram_disk_control_device_close(void* cookie)
{
	return B_OK;
}


static status_t
ram_disk_control_device_free(void* cookie)
{
	return B_OK;
}


static status_t
ram_disk_control_device_read(void* cookie, off_t position, void* buffer,
	size_t* _length)
{
	return B_BAD_VALUE;
}


static status_t
ram_disk_control_device_write(void* cookie, off_t position, const void* data,
	size_t* _length)
{
	return B_BAD_VALUE;
}


static status_t
ram_disk_control_device_control(void* cookie, uint32 op, void* buffer,
	size_t length)
{
	ControlDevice* device = (ControlDevice*)cookie;

	switch (op) {
		case RAM_DISK_IOCTL_REGISTER:
			return handle_ioctl(device, &ioctl_register, buffer);

		case RAM_DISK_IOCTL_UNREGISTER:
			return handle_ioctl(device, &ioctl_unregister, buffer);
	}

	return B_BAD_VALUE;
}


//	#pragma mark - raw device


static status_t
ram_disk_raw_device_init_device(void* driverCookie, void** _deviceCookie)
{
	RawDevice* device = static_cast<RawDevice*>((Device*)driverCookie);

	status_t error = device->Prepare();
	if (error != B_OK)
		return error;

	*_deviceCookie = device;
	return B_OK;
}


static void
ram_disk_raw_device_uninit_device(void* deviceCookie)
{
	RawDevice* device = (RawDevice*)deviceCookie;
	device->Unprepare();
}


static status_t
ram_disk_raw_device_open(void* deviceCookie, const char* path, int openMode,
	void** _cookie)
{
	RawDevice* device = (RawDevice*)deviceCookie;

	RawDeviceCookie* cookie = new(std::nothrow) RawDeviceCookie(device,
		openMode);
	if (cookie == NULL)
		return B_NO_MEMORY;

	*_cookie = cookie;
	return B_OK;
}


static status_t
ram_disk_raw_device_close(void* cookie)
{
	return B_OK;
}


static status_t
ram_disk_raw_device_free(void* _cookie)
{
	RawDeviceCookie* cookie = (RawDeviceCookie*)_cookie;
	delete cookie;
	return B_OK;
}


static status_t
ram_disk_raw_device_read(void* _cookie, off_t pos, void* buffer,
	size_t* _length)
{
	RawDeviceCookie* cookie = (RawDeviceCookie*)_cookie;
	RawDevice* device = cookie->Device();

	size_t length = *_length;

	if (pos >= device->DeviceSize())
		return B_BAD_VALUE;
	if (pos + (off_t)length > device->DeviceSize())
		length = device->DeviceSize() - pos;

	IORequest request;
	status_t status = request.Init(pos, (addr_t)buffer, length, false, 0);
	if (status != B_OK)
		return status;

	status = device->DoIO(&request);
	if (status != B_OK)
		return status;

	status = request.Wait(0, 0);
	if (status == B_OK)
		*_length = length;
	return status;
}


static status_t
ram_disk_raw_device_write(void* _cookie, off_t pos, const void* buffer,
	size_t* _length)
{
	RawDeviceCookie* cookie = (RawDeviceCookie*)_cookie;
	RawDevice* device = cookie->Device();

	size_t length = *_length;

	if (pos >= device->DeviceSize())
		return B_BAD_VALUE;
	if (pos + (off_t)length > device->DeviceSize())
		length = device->DeviceSize() - pos;

	IORequest request;
	status_t status = request.Init(pos, (addr_t)buffer, length, true, 0);
	if (status != B_OK)
		return status;

	status = device->DoIO(&request);
	if (status != B_OK)
		return status;

	status = request.Wait(0, 0);
	if (status == B_OK)
		*_length = length;

	return status;
}


static status_t
ram_disk_raw_device_io(void* _cookie, io_request* request)
{
	RawDeviceCookie* cookie = (RawDeviceCookie*)_cookie;
	RawDevice* device = cookie->Device();

	return device->DoIO(request);
}


static status_t
ram_disk_raw_device_control(void* _cookie, uint32 op, void* buffer,
	size_t length)
{
	RawDeviceCookie* cookie = (RawDeviceCookie*)_cookie;
	RawDevice* device = cookie->Device();

	switch (op) {
		case B_GET_DEVICE_SIZE:
		{
			size_t size = device->DeviceSize();
			return user_memcpy(buffer, &size, sizeof(size_t));
		}

		case B_SET_NONBLOCKING_IO:
		case B_SET_BLOCKING_IO:
			return B_OK;

		case B_GET_READ_STATUS:
		case B_GET_WRITE_STATUS:
		{
			bool value = true;
			return user_memcpy(buffer, &value, sizeof(bool));
		}

		case B_GET_GEOMETRY:
		case B_GET_BIOS_GEOMETRY:
		{
			device_geometry geometry;
			geometry.bytes_per_sector = B_PAGE_SIZE;
			geometry.sectors_per_track = 1;
			geometry.cylinder_count = device->DeviceSize() / B_PAGE_SIZE;
				// TODO: We're limited to 2^32 * B_PAGE_SIZE, if we don't use
				// sectors_per_track and head_count.
			geometry.head_count = 1;
			geometry.device_type = B_DISK;
			geometry.removable = true;
			geometry.read_only = false;
			geometry.write_once = false;

			return user_memcpy(buffer, &geometry, sizeof(device_geometry));
		}

		case B_GET_MEDIA_STATUS:
		{
			status_t status = B_OK;
			return user_memcpy(buffer, &status, sizeof(status_t));
		}

		case B_SET_UNINTERRUPTABLE_IO:
		case B_SET_INTERRUPTABLE_IO:
		case B_FLUSH_DRIVE_CACHE:
			return B_OK;

		case RAM_DISK_IOCTL_FLUSH:
		{
			status_t error = device->Flush();
			if (error != B_OK) {
				dprintf("ramdisk: flush: Failed to flush device: %s\n",
					strerror(error));
				return error;
			}

			return B_OK;
		}

		case RAM_DISK_IOCTL_INFO:
			return handle_ioctl(device, &ioctl_info, buffer);
	}

	return B_BAD_VALUE;
}


// #pragma mark -


module_dependency module_dependencies[] = {
	{B_DEVICE_MANAGER_MODULE_NAME, (module_info**)&sDeviceManager},
	{}
};


static const struct driver_module_info sChecksumDeviceDriverModule = {
	{
		kDriverModuleName,
		0,
		NULL
	},

	ram_disk_driver_supports_device,
	ram_disk_driver_register_device,
	drm_driver_init_driver,
	drm_driver_uninit_driver,
	drm_driver_register_child_devices
};

static const struct device_module_info sChecksumControlDeviceModule = {
	{
		kControlDeviceModuleName,
		0,
		NULL
	},

	ram_disk_control_device_init_device,
	ram_disk_control_device_uninit_device,
	NULL,

	ram_disk_control_device_open,
	ram_disk_control_device_close,
	ram_disk_control_device_free,

	ram_disk_control_device_read,
	ram_disk_control_device_write,
	NULL,	// io

	ram_disk_control_device_control,

	NULL,	// select
	NULL	// deselect
};

static const struct device_module_info sChecksumRawDeviceModule = {
	{
		kRawDeviceModuleName,
		0,
		NULL
	},

	ram_disk_raw_device_init_device,
	ram_disk_raw_device_uninit_device,
	NULL,

	ram_disk_raw_device_open,
	ram_disk_raw_device_close,
	ram_disk_raw_device_free,

	ram_disk_raw_device_read,
	ram_disk_raw_device_write,
	ram_disk_raw_device_io,

	ram_disk_raw_device_control,

	NULL,	// select
	NULL	// deselect
};

const module_info* modules[] = {
	(module_info*)&sChecksumDeviceDriverModule,
	(module_info*)&sChecksumControlDeviceModule,
	(module_info*)&sChecksumRawDeviceModule,
	NULL
};
