#pragma once

#ifdef MIX_DEVICE_ENABLED
#include <vector>
#include "../3rdparty/libmixdevice/include/mix_device.h"
#pragma comment(lib, "mix_device.dll.lib")

void gpu_counter_callback(double timestamp, size_t num_counters, const float* counter_buffer, void* usr_data);
void gpu_event_callback(size_t num_events, const mix_gpu_event* events_buffer, void* usr_data);

struct MixDevice
{
    struct Device
    {
        mix_device_t* dev;
        mix_features features;
        mix_instruments_t* ins;
        mix_gpu_tap_agent_t* gpu_tap;
        mix_gpu_device_info gpuInfo = {};
    };
    Device mixDevice;
    std::vector<mix_gpu_event> events;

    void setup()
    {
        mix_devices_t* List = nullptr;
        mix_list_devices(&List);
        int Count = mix_devices_count(List);
        if (Count > 0)
        {
            for (int Index = 0; Index < Count; Index++)
            {
                mixDevice.dev = mix_device_at(List, Index);
                mix_device_get_features(mixDevice.dev, &mixDevice.features, sizeof(mixDevice.features));
                if (mixDevice.features.support_instruments)
                {
                    mixDevice.ins = mix_create_instruments(mixDevice.dev);
                    mix_instruments_start(mixDevice.ins);
                    mixDevice.gpu_tap = mix_instruments_create_gpu_tap_agent(mixDevice.ins);
#if 0
                    mix_instruments_gpu_tap_get_gpu_info(mixDevice.gpu_tap, &mixDevice.gpuInfo, sizeof(mixDevice.gpuInfo));
#endif
                    mix_instruments_gpu_tap_register_callbacks(mixDevice.gpu_tap,
                        gpu_counter_callback,
                        gpu_event_callback,
                        this);

                    mix_gpu_config c = { 0.0, 0.0, -1 };
                    mix_instruments_gpu_tap_set_config(mixDevice.gpu_tap, &c, sizeof(c));
                }
            }
        }
        mix_release_devices(List);
    }

    void startGpuTrace()
    {
        if (mixDevice.gpu_tap)
            mix_instruments_gpu_tap_start_capture(mixDevice.gpu_tap);
    }

    void stopGpuTrace()
    {
        if (mixDevice.gpu_tap)
            mix_instruments_gpu_tap_stop_capture(mixDevice.gpu_tap);
    }

    void stop()
    {
        mix_instruments_stop(mixDevice.ins);
    }

    void shutdown()
    {
        if (mixDevice.gpu_tap)
        {
            mix_instruments_release_gpu_tap_agent(mixDevice.gpu_tap);
        }

        mix_instruments_stop(mixDevice.ins);
        mix_release_instruments(mixDevice.ins);
    }
};

void gpu_counter_callback(double timestamp, size_t num_counters, const float* counter_buffer, void* usr_data)
{
    int a = 0;
}

void gpu_event_callback(size_t num_events, const mix_gpu_event* events_buffer, void* usr_data)
{
    auto self = (MixDevice*)usr_data;
    for (int i = 0; i < num_events; i++)
    {
        self->events.push_back(events_buffer[i]);
    }
}

#else
struct MixDevice
{
    void setup()
    {
    }
};
#endif