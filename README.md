# FHook - Android Java Layer Full-Function HOOK Framework

<div style="text-align: right; margin-bottom: 15px;">
  <button onclick="toggleLanguage()" style="padding: 6px 12px; cursor: pointer; background: #f5f5f5; border: 1px solid #ddd; border-radius: 4px;">
    Switch to 中文
  </button>
</div>

<!-- English Content -->
<div id="english-content">

## Overview
A full-function HOOK framework for the **Android Java layer** with the following core features:
- **Root-free**: Initialize and use directly within the app
- **Android 9+ (API 28+)**: Supports the latest Android versions
- Intercept and tamper with **input parameters/return values** of any Java method
- Support **batch HOOK by class/instance**, covering common **system key points** (class loading, device fingerprinting, SP writing, etc.)
- Three integration methods: Gradle dependency (implementation), source code integration (module/source copy), and app injection (repackaging or dynamic loading) under compliance, covering different usage scenarios

> For **compliant scenarios** only: security research, testing, debugging. Ensure you have legal authorization for the target.


## 1. What Problems It Solves
- **Rapid Observation**: Print call stacks/parameters/return values at runtime without modifying target code
- **Temporary Modification**: Temporarily correct input parameters/return values or inject "fake data" to verify code branches
- **Bulk Coverage**: One-click HOOK for all methods of a class/instance to improve debugging and regression efficiency
- **System Key Point Monitoring**: Intercept and record operations like `Class.forName`, `ClassLoader.loadClass`, `Settings.Secure.getString`, and `System.loadLibrary`


## 2. Applicable Scenarios & Environment
- **Environment**: Android 9+ (API 28+), compatible with Kotlin/Java projects
- **Scenarios**: Function joint debugging, gray-box testing, automated acceptance, key path burying & auditing, crash location, etc.
- **No Dependencies**: Xposed / Magisk / Root


## 3. Quick Installation & Usage

### 3.1 Add JitPack Repository
Add the following to **`settings.gradle`** or the root **`build.gradle`**:
```groovy
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        mavenCentral()
        maven { url 'https://jitpack.io' }
    }
}
```

> For Kotlin DSL: Replace with `maven(url = "https://jitpack.io")`

### 3.2 Add Dependency
Add to the **app module**:
```groovy
dependencies {
    implementation "com.github.Rift0911:fhook:1.1.5"
}
```

### 3.3 Initialization (Auto / Manual)

#### Method A: Auto-initialization via `attachBaseContext`
```java
@Override
protected void attachBaseContext(Context base) {
    Log.d(TAG, "attachBaseContext");
    if (FCFG.IS_APP_INIT_AUTO) {
        Log.i(TAG, "attachBaseContext FHook.init= " + FHook.init(base));
    }
    super.attachBaseContext(base);
}
```

#### Method B: Manual Initialization Anywhere (Example: Button Click)
```java
bt_main_02.setOnClickListener(v -> {
    if (FHook.isInited()) {
        FHook.unInit();
    } else {
        if (!FHook.init(this)) {
            Toast.makeText(this, "Initialization failed", Toast.LENGTH_LONG).show();
            Log.e(TAG, "Initialization failed");
        } else {
            Toast.makeText(this, "Initialization succeeded", Toast.LENGTH_LONG).show();
            Log.i(TAG, "Initialization succeeded");
        }
    }
});
```

> Common APIs:
> - `FHook.unHookAll()`: Remove all HOOKs
> - `FHook.showHookInfo()`: View current HOOK status

### 3.4 Minimal Working Examples

#### A) HOOK Ordinary Methods (In-app Methods)
Take `THook.fun_I_III(int a, int b, int c): int` as an example to demonstrate **modifying input parameters and return values**:
```java
import java.lang.reflect.Method;
import android.util.Log;

Method m = THook.class.getMethod("fun_I_III", int.class, int.class, int.class);

FHook.hook(m)
    .setOrigFunRun(true) // Execute the original function first
    .setHookEnter((thiz, args, types, hh) -> {
        // Modify the first input parameter
        args.set(0, 6666);
        Log.d("FHook", "fun_I_III enter: " + args);
    })
    .setHookExit((ret, type, hh) -> {
        // Force modify the return value
        Log.d("FHook", "fun_I_III exit, origRet=" + ret);
        return 8888;
    })
    .commit();
```

#### B) HOOK System Methods (Device Fingerprint Example)
Take `Settings.Secure.getString(ContentResolver, String)` as an example to **forge ANDROID_ID**:
```java
import android.provider.Settings;
import android.content.ContentResolver;
import java.lang.reflect.Method;
import android.util.Log;

Method sysGet = Settings.Secure.class.getMethod(
        "getString", ContentResolver.class, String.class);

FHook.hook(sysGet)
    .setOrigFunRun(true)
    .setHookEnter((thiz, args, types, hh) -> {
        String key = (String) args.get(1);
        hh.extras.put("key", key);
        Log.d("FHook", "Settings.Secure.getString key=" + key);
    })
    .setHookExit((ret, type, hh) -> {
        String key = (String) hh.extras.get("key");
        if ("android_id".equalsIgnoreCase(key)) {
            return "a1b2c3d4e5f6a7b8"; // Only effective for ANDROID_ID
        }
        return ret; // Keep original value for other keys
    })
    .commit();
```

> Tip: For interface/bridge methods (e.g., `SharedPreferences.Editor.commit`), it is recommended to use `FHookTool.findMethod4Impl(editor, ifaceMethod)` to find the **real implementation method** before HOOKing for higher success rate.


## 4. Sponsorship & Support · Authorization & Cooperation
- **Personal Learning & Research**: Free to use (see **[LICENSE.agent-binary](./LICENSE.agent-binary)** in the root directory of the repository).
- **Commercial/Enterprise Use**: Strictly comply with **LICENSE.agent-binary**; for **commercial authorization, custom functions, technical support, or joint R&D**, contact via **GitHub Issues** or the contact information on the repository homepage.
- **Sponsorship**: Welcome to Star ⭐, submit PRs, or contact us for sponsorship channels.

> **Compliance Statement**: FHook is for compliant scenarios only. **Strictly prohibited** from using this project for any illegal or non-compliant purposes. The user shall bear all risks and consequences arising therefrom.


### Contact Information
- **Commercial Cooperation & Technical Support**: Contact **Feadre Technology** via:
  - Email: `rift@feadre.top`
  - QQ: `27113970`

- **Sponsor Us** (Thank you very much!):

<!-- Responsive Sponsorship QR Code Container: Side-by-Side on Wide Screens, Wrap on Narrow Screens -->
<div style="display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin: 20px 0;">
  <!-- WeChat Sponsorship -->
  <div style="min-width: 180px; text-align: center;">
    <img src="docs/wx_rift_m.jpg" alt="WeChat Sponsorship" height="180" style="object-fit: contain;">
    <p style="margin-top: 8px; font-size: 14px;">WeChat Payment</p>
  </div>
  <!-- Alipay Sponsorship -->
  <div style="min-width: 180px; text-align: center;">
    <img src="docs/zfb_rift_m.jpg" alt="Alipay Sponsorship" height="180" style="object-fit: contain;">
    <p style="margin-top: 8px; font-size: 14px;">Alipay Payment</p>
  </div>
</div>


**Enjoy using FHook!**


### Appendix: Notes
- **Known Unsupported/High-Risk Bridge Methods** (see `isBridgeCritical` in the project):
  The following methods are **usually not recommended/cannot be directly HOOKed**:
  ```
  Thread.currentThread()
  Thread#getContextClassLoader()
  Class#getDeclaredMethod(String, Class[])
  ```
  These are mostly bridge/reflection entrances. It is recommended to **locate the real implementation method** first (e.g., get the implementation method via `FHookTool.findMethod4Impl(...)`) before HOOKing.

- **Mutual Exclusion Recommendation**: Do **not HOOK `Class.forName(...)` and `ClassLoader.loadClass(...)` at the same time**.
  They have mutual calling/recursive links, which may easily cause deadlocks or freezes. The framework does not impose mandatory restrictions; please avoid this scenario yourself.

- For **bridge/reflection bridge methods** (e.g., `Method.invoke`), direct HOOK is not recommended. Locate the **real implementation method** (via `FHookTool.findMethod4Impl`).

- Callbacks run in the calling thread; avoid heavy operations in the UI thread.

- Different Android versions have different hidden API restrictions; verify version compatibility when HOOKing system methods.

- If obfuscation affects debugging, temporarily add (as needed):
```pro
-keep class top.feadre.fhook.** { *; }
or
-keep class top.feadre.fhook.CLinker {*;}
-keep class top.feadre.fhook.FHook {*;}
```

> If you find bugs or have other suggestions during use, feel free to contact the author or submit to Issues. It is recommended to **feedback the failed method signature and system version** to facilitate quick troubleshooting.

</div>

<!-- Chinese Content (Hidden by Default) -->
<div id="chinese-content" style="display: none;">

## 概述
一款针对 **Android Java 层** 的全函数 HOOK 框架，核心特性如下：
- **无需 Root**：应用内随时初始化使用
- **Android 9+（API 28+）**：支持最新 Android 版本
- 拦截并篡改任意 **Java 方法** 的入参/返回值
- 支持 **按类/实例批量 hook**，覆盖常见 **系统关键点**（类加载、设备指纹、SP 写入等）
- 三种集成方式：Gradle 依赖（implementation）、源码集成（module/源码拷贝）、合规前提下的应用注入（重打包或动态加载），覆盖不同使用场景

> 仅用于 **合规场景**：安全研究、测试、调试。请确保对目标拥有合法授权。


## 1. 解决什么问题
- **快速观测**：无需修改目标代码，运行期即可打印调用栈/参数/返回值
- **临时改写**：对入参/返回值做临时修正或“假数据”回灌，便于验证代码分支
- **批量覆盖**：对类/实例的全部方法一键 hook，提升调试与回归效率
- **系统关键点监控**：可拦截记录 `Class.forName`、`ClassLoader.loadClass`、`Settings.Secure.getString`、`System.loadLibrary` 等操作


## 2. 适用场景 & 环境
- **环境**：Android 9+（API 28+），兼容 Kotlin/Java 工程
- **场景**：功能联调、灰盒测试、自动化验收、关键路径埋点与审计、崩溃定位等
- **无依赖**：不依赖 Xposed / Magisk / Root


## 3. 快速安装与使用

### 3.1 添加 JitPack 仓库
在 **`settings.gradle`** 或根 **`build.gradle`** 中添加：
```groovy
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        mavenCentral()
        maven { url 'https://jitpack.io' }
    }
}
```

> Kotlin DSL 请改为：`maven(url = "https://jitpack.io")`

### 3.2 引入依赖
在 **app 模块** 中添加：
```groovy
dependencies {
    implementation "com.github.Rift0911:fhook:1.1.5"
}
```

### 3.3 初始化（自动 / 手动）

#### 方式 A：通过 `attachBaseContext` 自动初始化
```java
@Override
protected void attachBaseContext(Context base) {
    Log.d(TAG, "attachBaseContext");
    if (FCFG.IS_APP_INIT_AUTO) {
        Log.i(TAG, "attachBaseContext FHook.init= " + FHook.init(base));
    }
    super.attachBaseContext(base);
}
```

#### 方式 B：任意位置手动初始化（示例：按钮点击）
```java
bt_main_02.setOnClickListener(v -> {
    if (FHook.isInited()) {
        FHook.unInit();
    } else {
        if (!FHook.init(this)) {
            Toast.makeText(this, "初始化失败", Toast.LENGTH_LONG).show();
            Log.e(TAG, "初始化失败");
        } else {
            Toast.makeText(this, "初始化成功", Toast.LENGTH_LONG).show();
            Log.i(TAG, "初始化成功");
        }
    }
});
```

> 常用 API：
> - `FHook.unHookAll()`：解除全部 hook
> - `FHook.showHookInfo()`：查看当前 hook 状态

### 3.4 最小可跑示例

#### A) Hook 普通方法（应用内方法）
以 `THook.fun_I_III(int a, int b, int c): int` 为例，演示 **修改入参与返回值**：
```java
import java.lang.reflect.Method;
import android.util.Log;

Method m = THook.class.getMethod("fun_I_III", int.class, int.class, int.class);

FHook.hook(m)
    .setOrigFunRun(true) // 先执行原函数
    .setHookEnter((thiz, args, types, hh) -> {
        // 修改第一个入参
        args.set(0, 6666);
        Log.d("FHook", "fun_I_III enter: " + args);
    })
    .setHookExit((ret, type, hh) -> {
        // 强制修改返回值
        Log.d("FHook", "fun_I_III exit, origRet=" + ret);
        return 8888;
    })
    .commit();
```

#### B) Hook 系统方法（设备指纹示例）
以 `Settings.Secure.getString(ContentResolver, String)` 为例，定向 **伪造 ANDROID_ID**：
```java
import android.provider.Settings;
import android.content.ContentResolver;
import java.lang.reflect.Method;
import android.util.Log;

Method sysGet = Settings.Secure.class.getMethod(
        "getString", ContentResolver.class, String.class);

FHook.hook(sysGet)
    .setOrigFunRun(true)
    .setHookEnter((thiz, args, types, hh) -> {
        String key = (String) args.get(1);
        hh.extras.put("key", key);
        Log.d("FHook", "Settings.Secure.getString key=" + key);
    })
    .setHookExit((ret, type, hh) -> {
        String key = (String) hh.extras.get("key");
        if ("android_id".equalsIgnoreCase(key)) {
            return "a1b2c3d4e5f6a7b8"; // 仅对 ANDROID_ID 生效
        }
        return ret; // 其他 key 保持原值
    })
    .commit();
```

> 提示：对于接口/桥接方法（如 `SharedPreferences.Editor.commit`），建议使用 `FHookTool.findMethod4Impl(editor, ifaceMethod)` 找到 **真实实现方法** 后再 hook，成功率更高。


## 4. 赞助与支持 · 授权与合作
- **个人学习与研究**：免费使用（详见仓库根目录 **[LICENSE.agent-binary](./LICENSE.agent-binary)**）。
- **商业/企业使用**：需严格遵守 **LICENSE.agent-binary**；如需 **商用授权、定制功能、技术支持或联合研发**，请通过 **GitHub Issues** 或仓库主页联系方式沟通。
- **赞助方式**：欢迎 Star ⭐、提交 PR，或联系获取赞助通道。

> **合规声明**：FHook 仅用于合规场景。**严禁**将本项目用于任何违法违规用途，由此产生的风险与后果由使用者自行承担。


### 联系方式
- **商业合作 & 技术支持**：通过以下方式联系 **裂痕科技（Feadre）**：
  - 邮箱：`rift@feadre.top`
  - QQ：`27113970`

- **赞助我们**（非常感谢！）：

<!-- 响应式赞助二维码容器：宽屏并排，窄屏换行 -->
<div style="display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; margin: 20px 0;">
  <!-- 微信赞助 -->
  <div style="min-width: 180px; text-align: center;">
    <img src="docs/wx_rift_m.jpg" alt="微信赞助" height="180" style="object-fit: contain;">
    <p style="margin-top: 8px; font-size: 14px;">微信支付</p>
  </div>
  <!-- 支付宝赞助 -->
  <div style="min-width: 180px; text-align: center;">
    <img src="docs/zfb_rift_m.jpg" alt="支付宝赞助" height="180" style="object-fit: contain;">
    <p style="margin-top: 8px; font-size: 14px;">支付宝支付</p>
  </div>
</div>


**尽情享受使用 FHook 的乐趣吧！**


### 附：注意事项
- **已知不支持/高危桥接方法**（详见项目内 `isBridgeCritical`）：
  以下方法 **通常不建议/无法直接 hook**：
  ```
  Thread.currentThread()
  Thread#getContextClassLoader()
  Class#getDeclaredMethod(String, Class[])
  ```
  此类方法多为桥接/反射入口，建议先 **定位真实实现方法**（如通过 `FHookTool.findMethod4Impl(...)` 获取实现方法）后再 hook。

- **互斥建议**：**不要同时 hook `Class.forName(...)` 与 `ClassLoader.loadClass(...)`**。
  两者存在相互调用/递归链路，易导致死锁或卡死；框架未做强制限制，请自行规避该场景。

- 对于 **桥接/反射桥方法**（如 `Method.invoke`），不建议直接 hook，需定位 **真实实现方法**（通过 `FHookTool.findMethod4Impl`）。

- 回调运行在调用线程，避免在 UI 线程执行耗时操作。

- 不同 Android 版本的隐藏 API 限制不同，hook 系统方法时需验证版本兼容性。

- 若混淆影响调试，可临时添加（按需）：
```pro
-keep class top.feadre.fhook.** { *; }
或
-keep class top.feadre.fhook.CLinker {*;}
-keep class top.feadre.fhook.FHook {*;}
```

> 使用中若发现 Bug 或有其他建议，欢迎联系作者或提交至 Issues。建议 **反馈失败的方法签名与系统版本**，以便快速定位问题。

</div>

<!-- Language Switch Script -->
<script>
function toggleLanguage() {
  const englishContent = document.getElementById('english-content');
  const chineseContent = document.getElementById('chinese-content');
  const button = document.querySelector('button');

  if (englishContent.style.display !== 'none') {
    // Switch to Chinese
    englishContent.style.display = 'none';
    chineseContent.style.display = 'block';
    button.textContent = 'Switch to English';
  } else {
    // Switch to English
    englishContent.style.display = 'block';
    chineseContent.style.display = 'none';
    button.textContent = 'Switch to 中文';
  }
}
</script>