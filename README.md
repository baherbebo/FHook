

<p align="right">Language: <b>English</b> · <a href="README.cn.md">Chinese</a></p>

# FHook

**Full-function HOOK framework for the Android Java layer**

* **No Root required** — initialize and use directly inside the app
* **Android 9+ (API 28+)**, including the latest versions
* Intercept and modify **arguments/return values** of any **Java method**
* **Class/instance-wide batch hooks**, covering common **system hotspots** (class loading, device fingerprint, SharedPreferences writes, etc.)
* Three integration options: Gradle dependency (`implementation`), source integration (module/source copy), and (under compliance) app injection (repack or dynamic loading)

> For **lawful** security research, testing, and debugging only. Ensure you have proper authorization for any target.

---

## 1. What problem does it solve?

* **Rapid observation**: print call stacks/args/returns at runtime without touching target code
* **Temporary patching**: tweak args/returns or feed “mock data” to verify branches
* **Batch coverage**: one-click hook for all methods on a class/instance to accelerate debugging and regression
* **System hotspot auditing**: `Class.forName` / `ClassLoader.loadClass` / `Settings.Secure.getString` /
  `System.loadLibrary`, etc. can be intercepted and logged

---

## 2. Scenarios & Environment

* **Environment**: Android 9+ (API 28+); works with Kotlin/Java projects
* **Scenarios**: feature co-debugging, gray-box testing, automated acceptance, critical-path tracing & audit, crash triage
* **No dependency on** Xposed / Magisk / Root

---

## 3. Quick Start

### 3.1 Add JitPack repository

Add to **`settings.gradle`** or the root **`build.gradle`**:

```groovy
dependencyResolutionManagement {
  repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
  repositories {
    mavenCentral()
    maven { url 'https://jitpack.io' }
  }
}
```

> Kotlin DSL: `maven(url = "https://jitpack.io")`

### 3.2 Add dependency

In your **app module**:

```groovy
dependencies {
  implementation "com.github.Rift0911:fhook:+"
}
```

### 3.3 Initialize (auto / manual)

**Option A: Auto init in `attachBaseContext`**

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

**Option B: Manual init anywhere (e.g., button click)**

```java
bt_main_02.setOnClickListener(v -> {
    if (FHook.isInited()) {
        FHook.unInit();
    } else {
        if (!FHook.init(this)) {
            Toast.makeText(this, "Init failed", Toast.LENGTH_LONG).show();
            Log.e(TAG, "Init failed");
        } else {
            Toast.makeText(this, "Init success", Toast.LENGTH_LONG).show();
            Log.i(TAG, "Init success");
        }
    }
});
```

> Handy calls: `FHook.unHookAll()` to remove all hooks; `FHook.showHookInfo()` to view current hook status.

### 3.4 Minimal runnable samples

#### A) Hook a regular app method

Take `THook.fun_I_III(int a, int b, int c): int` as an example — **modify args and return**:

```java
import java.lang.reflect.Method;
import android.util.Log;

Method m = THook.class.getMethod("fun_I_III", int.class, int.class, int.class);

FHook.hook(m)
    .setOrigFunRun(true) // run the original method first
    .setHookEnter((thiz, args, types, hh) -> {
        // change the first argument
        args.set(0, 6666);
        Log.d("FHook", "fun_I_III enter: " + args);
    })
    .setHookExit((ret, type, hh) -> {
        // force the return value
        Log.d("FHook", "fun_I_III exit, origRet=" + ret);
        return 8888;
    })
    .commit();
```

#### B) Hook a system method (device fingerprint sample)

Take `Settings.Secure.getString(ContentResolver, String)` — **forge ANDROID\_ID** selectively:

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
            return "a1b2c3d4e5f6a7b8"; // affect ANDROID_ID only
        }
        return ret; // keep others intact
    })
    .commit();
```

> Tip: for interface/bridge methods (e.g., `SharedPreferences.Editor.commit`), use
> `FHookTool.findMethod4Impl(editor, ifaceMethod)` to locate the **actual implementation method** before hooking for a higher success rate.

---

### 3.5 Constructor Hook Samples (System & Custom)

> Notes: By definition, a **constructor always runs**; `setOrigFunRun(true/false)` **has no effect** on constructors.
> During **enter**, the object is **not fully initialized** — do not touch fields/instance methods.
> On **exit**, `ret` is the newly created instance (same as `thisObject`).

#### A) Hook system constructor: `FileInputStream(FileDescriptor)`

```java
// 1) Bind the constructor
Constructor<FileInputStream> c =
        FileInputStream.class.getDeclaredConstructor(FileDescriptor.class);
c.setAccessible(true);

FHook.hook(c)
    .setOrigFunRun(true) // no-op for constructors, harmless to keep
    .setHookEnter((thiz, args, types, hh) -> {
        // Observe only: reverse path from FileDescriptor (may hit hidden-API limits; use HiddenApiBypass if needed)
        FileDescriptor fdObj = (FileDescriptor) args.get(0);
        Field f = FileDescriptor.class.getDeclaredField("descriptor");
        f.setAccessible(true);
        int fd = (int) f.get(fdObj);
        String path = android.system.Os.readlink("/proc/self/fd/" + fd);
        Log.i("FHook", "[FIS.<init>(fd).enter] fd=" + fd + ", path=" + path);
    })
    .setHookExit((ret, type, hh) -> {
        Log.i("FHook", "[FIS.<init>(fd).exit] new instance=" + ret);
        return ret; // keep constructor return as-is
    })
    .commit();

// 2) Trigger: open your own APK via PFD, then read some Zip entries through FileInputStream(FileDescriptor)
String apkPath = context.getApplicationInfo().sourceDir;
android.os.ParcelFileDescriptor pfd = android.os.ParcelFileDescriptor.open(
        new java.io.File(apkPath),
        android.os.ParcelFileDescriptor.MODE_READ_ONLY);

try (FileInputStream fis = new FileInputStream(pfd.getFileDescriptor());
     java.util.zip.ZipInputStream zis = new java.util.zip.ZipInputStream(fis)) {
    java.util.zip.ZipEntry e; int seen = 0; byte[] buf = new byte[4096];
    while ((e = zis.getNextEntry()) != null && seen++ < 5) {
        Log.i("FHook", "[Zip] " + e.getName());
        while (zis.read(buf) != -1) { /* drain */ }
    }
} finally {
    try { pfd.close(); } catch (Throwable ignore) {}
}
```

#### B) Hook custom class constructors (no-arg & arg-ful)

```java
// No-arg constructor
Constructor<TObject> c0 = TObject.class.getDeclaredConstructor();
c0.setAccessible(true);
FHook.hook(c0)
    .setOrigFunRun(true) // no-op for constructors
    .setHookEnter((thiz, args, types, hh) -> {
        // Mark only; access the instance on exit
        hh.extras.put("watch", true);
    })
    .setHookExit((ret, type, hh) -> {
        if (Boolean.TRUE.equals(hh.extras.get("watch")) && ret instanceof TObject) {
            TObject to = (TObject) ret;
            to.setName("Zhang San").setAge(109);
            Log.i("FHook", "[Ctor0.exit] TObject() -> " + to);
        }
        return ret;
    })
    .commit();

// (String,int) constructor
Constructor<TObject> c2 = TObject.class.getDeclaredConstructor(String.class, int.class);
c2.setAccessible(true);
FHook.hook(c2)
    .setOrigFunRun(true)
    .setHookEnter((thiz, args, types, hh) -> {
        Log.i("FHook", "[Ctor2.enter] name=" + args.get(0) + ", age=" + args.get(1));
        hh.extras.put("watch", true);
    })
    .setHookExit((ret, type, hh) -> {
        if (Boolean.TRUE.equals(hh.extras.get("watch")) && ret instanceof TObject) {
            TObject to = (TObject) ret;
            to.setName("Li Si").setAge(16);
            Log.i("FHook", "[Ctor2.exit] TObject(name,int) -> " + to);
        }
        return ret;
    })
    .commit();
```

> Tips
>
> * Constructor hooks **observe/modify** but do not **block** creation (i.e., `setOrigFunRun` is ineffective).
> * Reading `FileDescriptor.descriptor` may trigger hidden-API restrictions on some ROMs/versions; use **HiddenApiBypass** or NDK helpers if needed.

---

## 4. Sponsorship & Support · Licensing & Collaboration

* **Personal learning & research**: free to use (see **[LICENSE.agent-binary](./LICENSE.agent-binary)** in the repo root).
* **Commercial/enterprise use**: strictly follow **LICENSE.agent-binary**. For **commercial licensing, custom features, technical support, or joint R\&D**, please reach out via **GitHub Issues** or the contact info below.
* **Ways to support**: Star ⭐ the repo, send PRs, or contact us for sponsorship options.

> **Compliance Notice**: FHook is for compliant use only. **Any illegal or abusive use is strictly prohibited**; users bear all associated risks and consequences.

---

### Source Delivery & Core Technical Support

We provide deeper engineering assistance to help you land with lower cost (under lawful compliance):

* **Source delivery options**: choose *full source delivery* or a hybrid of *core agent binary + adapter-layer source* (NDA available)
* **Deployment modes**: Gradle dependency, source import (mono-repo/multi-module), private Maven distribution
* **Compatibility & adaptation**: Android 9–15 deltas, OEM ROMs, stability under obfuscation/packing/VMP
* **Performance & stability**: init timing, deadlock avoidance, bypassing critical bridge paths, callback threading & jank mitigation
* **Training & co-build**: secondary-dev training, code walkthrough, best-practice checklists, co-debugging & triage

> For **source delivery / core support / customization**, contact us below with your scope and requirements.

### Contact

* **Business & Technical Support** — **Feadre (Rift Tech)**:

  * Email: `rift@feadre.top`
  * Email: `zkbutt@hotmail.com`
  * QQ: `27113970`

* **Sponsor us** (thank you!):

| <img src="docs/wx_rift_m.jpg" alt="WeChat Pay" height="180"> | <img src="docs/zfb_rift_m.jpg" alt="Alipay" height="180"> |
| :----------------------------------------------------------: | :-------------------------------------------------------: |
|                          WeChat Pay                          |                           Alipay                          |

**Enjoy hacking (legally) with FHook!**

### Appendix: Notes

* **Known unsupported/high-risk bridges** (see `isBridgeCritical` in the project). The following are **usually not recommended / not hookable**:

  ```
  Thread.currentThread()
  Thread#getContextClassLoader()
  Class#getDeclaredMethod(String, Class[])
  ```

  These are often bridge/reflection entry points. Prefer locating the **actual implementation method** first (e.g., via
  `FHookTool.findMethod4Impl(...)`).

* **Mutual exclusion tip**: avoid hooking both `Class.forName(...)` and `ClassLoader.loadClass(...)` together.
  They may call each other and create recursion → deadlocks/hangs. The framework does not hard-block this; please avoid manually.

* Bridge/reflection bridges (e.g., `Method.invoke`) are not recommended; hook the **real implementation** (`FHookTool.findMethod4Impl`).

* Callbacks run on the caller’s thread; avoid heavy work on the UI thread.

* Hidden-API restrictions vary by Android version; validate system-method hooks across versions.

* If obfuscation hinders debugging, add (as needed):

```pro
-keep class top.feadre.fhook.** { *; }
# or
-keep class top.feadre.fhook.CLinker {*;}
-keep class top.feadre.fhook.FHook {*;}
```

> If you hit a bug or have suggestions, please open an Issue with the **failed method signature and Android version** so we can reproduce and resolve quickly.
