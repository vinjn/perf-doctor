# perf-doctor
A mobile game profiler.

[Download prebuit binary](https://github.com/taptap/perf-doctor/releases)

## GUI interface

![](https://user-images.githubusercontent.com/558657/143417411-f257fc80-c1fa-4c51-8d9a-983df5cf22be.png)

## Export perf data

![](https://user-images.githubusercontent.com/558657/144166485-ba706ce1-544d-49be-a426-12fc9db79f42.png)

# Build from scratch
- clone https://github.com/taptap/perf-doctor
- clone [Cinder framework](https://github.com/cinder/Cinder), `Cinder/` and `perf-doctor/` should be put in the same folder.
- Open `vc2019/perf-doctor.sln` with `Visual Studio 2019`

# Contact
- vinjn@xd.com

## adb commands for gpu profiling

### adb shell settings list global | grep gpu
```
enable_gpu_debug_layers=1
gpu_debug_app=com.xd.atelier
gpu_debug_layer_app=com.google.android.gapid.arm64v8a:com.google.pixel.coral.gpuprofiling.vulkanlayer
gpu_debug_layers=DebugMarker:CPUTiming:VkRenderStagesProducer
```

### Disable settings
```
adb shell settings delete global enable_gpu_debug_layers
adb shell settings delete global gpu_debug_app
adb shell settings delete global gpu_debug_layers_gles
adb shell settings delete global gpu_debug_layers
adb shell settings delete global gpu_debug_layer_app
```

### adb shell getprop | grep profiler
```
[debug.graphics.gpu.profiler.perfetto]: [1]
[graphics.gpu.profiler.support]: [true]
[graphics.gpu.profiler.vulkan_layer_apk]: [com.google.pixel.coral.gpuprofiling.vulkanlayer]
```

