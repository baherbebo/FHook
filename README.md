<p align="right">Language: <a href="README.cn.md">中文</a> · <b>English</b></p>

# FHook 
- Android Java Layer Full-Function HOOK Framework


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
    implementation "com.github.Rift0911:fhook:+"
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


| <img src="docs/wx_rift_m.jpg" alt="微信赞助" height="180"> | <img src="docs/zfb_rift_m.jpg" alt="支付宝赞助" height="180"> |
|:--:|:--:|
| WeChat Payment | Alipay Payment |

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




