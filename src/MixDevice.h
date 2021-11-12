#pragma once

#ifdef MIX_DEVICE_ENABLED
#include <vector>
#include "../3rdparty/libmixdevice/include/mix_device.h"
#pragma comment(lib, "mix_device.dll.lib")

struct MixDevice
{
    struct Device
    {
        mix_device_t* dev;
        mix_features features;
        mix_instruments_t* ins;
        mix_gpu_tap_agent_t* tap;
        mix_gpu_device_info gpuInfo;
    };
    std::vector<Device> mixDevices;
    
    void setup()
    {
        mix_devices_t* List = nullptr;
        mix_list_devices(&List);
        int Count = mix_devices_count(List);
        if (Count > 0)
        {
            for (int Index = 0; Index < Count; Index++)
            {
                Device device;
                device.dev = mix_device_at(List, Index);
                mix_device_get_features(device.dev, &device.features, sizeof(device.features));
                if (device.features.support_instruments)
                {
                    device.ins = mix_create_instruments(device.dev);
                    device.tap = mix_instruments_create_gpu_tap_agent(device.ins);
                    mix_instruments_gpu_tap_get_gpu_info(device.tap, &device.gpuInfo);
                }

                mixDevices.push_back(device);
            }
        }
        mix_release_devices(List);
    }
};

#else
struct MixDevice
{
    void setup()
    {
    }
};
#endif