#pragma once

#include <vector>
#include "../3rdparty/libmixdevice/include/mix_device.h"
#pragma comment(lib, "mix_device.dll.lib")

struct MixDevice
{
    std::vector<mix_device_t*> mMixDevices;
    std::vector<mix_features> mMixFeatures;

    void setup()
    {
        mix_devices_t* List = nullptr;
        mix_list_devices(&List);
        int Count = mix_devices_count(List);
        if (Count > 0)
        {
            for (int Index = 0; Index < Count; Index++)
            {
                auto dev = mix_device_at(List, Index);
                mMixDevices.push_back(dev);
                mix_features features;
                mix_device_get_features(dev, &features, sizeof(features));
                mMixFeatures.push_back(features);
            }
        }
        mix_release_devices(List);
    }
};

