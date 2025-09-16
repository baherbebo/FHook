# FHook

Android 字节码插桩（Dex IR）与 JVMTI 动态挂载的一体化框架。
- `libfhook.so`：JNI + IR 改写（CMake 构建）
- `libfhook_agent.so`：JVMTI Agent（二进制随 AAR 提供，闭源）

## 安装
### Maven Central（推荐）或 JitPack（简便）
```gradle
repositories { mavenCentral() /* or jitpack */ }
dependencies { implementation "top.feadre:fhook:1.0.0" }

