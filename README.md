# perf-doctor
手机应用性能测试工具

# 懒得编译，直接运行

[点此下载最新版](https://github.com/taptap/perf-doctor/releases)

## 可视化界面

![](https://user-images.githubusercontent.com/558657/143417411-f257fc80-c1fa-4c51-8d9a-983df5cf22be.png)

## 「Export」性能数据导出

![](https://user-images.githubusercontent.com/558657/144166485-ba706ce1-544d-49be-a426-12fc9db79f42.png)

# 从代码编译
- https://github.com/taptap/perf-doctor
- 依赖 [Cinder 框架](https://github.com/cinder/Cinder)，Cinder/ 与 perf-doctor/ 需位于同一层目录结构
- 使用 Visual Studio 2019 打开 vc2019/perf-doctor.sln

# 联系方式
- vinjn@xd.com


## adb 

adb shell settings list global | grep gpu

    enable_gpu_debug_layers=1
    gpu_debug_app=com.xd.atelier
    gpu_debug_layer_app=com.google.android.gapid.arm64v8a:com.google.pixel.coral.gpuprofiling.vulkanlayer
    gpu_debug_layers=DebugMarker:CPUTiming:VkRenderStagesProducer

Disable settings

    adb shell settings delete global enable_gpu_debug_layers
    adb shell settings delete global gpu_debug_app
    adb shell settings delete global gpu_debug_layers_gles
    adb shell settings delete global gpu_debug_layers
    adb shell settings delete global gpu_debug_layer_app

adb shell getprop | grep profiler

    [debug.graphics.gpu.profiler.perfetto]: [1]
    [graphics.gpu.profiler.support]: [true]
    [graphics.gpu.profiler.vulkan_layer_apk]: [com.google.pixel.coral.gpuprofiling.vulkanlayer]


## Huawei

/data/local/tmp/config.txt