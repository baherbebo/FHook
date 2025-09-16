# FHook

**Android 端 Java 层全函数 HOOK 框架**

* **无需 Root**，应用内即可初始化
* **Android 9+（API 28+）**，含最新版本
* 任意 **Java 方法** 的入参/返回值拦截与篡改
* 支持 **按类/实例批量 hook**，覆盖常见 **系统关键点**（类加载、设备指纹、SP 写入等）
* 支持 Gradle 依赖（implementation）、源码集成（module/源码拷贝）、以及在合规前提下的应用注入（重打包或动态加载）三种方式，覆盖不同使用场景

> 仅用于安全研究、测试与调试等 **合规场景**。请确保对目标拥有合法授权。

---

## 1. 解决什么问题

* **快速观测**：无需改目标代码，运行期即可打印调用栈/参数/返回值
* **临时改写**：对入参/返回值做临时修正或“假数据”回灌，便于验证分支
* **批量覆盖**：对类/实例的全部方法一键 hook，提高调试与回归效率
* **系统关键点监控**：`Class.forName` / `ClassLoader.loadClass` / `Settings.Secure.getString` /
  `System.loadLibrary` 等都能拦截记录

---

## 2. 适用场景 & 环境

* **环境**：Android 9+（API 28+），Kotlin/Java 工程均可
* **场景**：功能联调、灰盒测试、自动化验收、关键路径埋点与审计、崩溃定位等
* **不依赖**：Xposed / Magisk / Root

---

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

在 **app 模块**：

```groovy
dependencies {
    implementation "com.github.Rift0911:fhook:1.1.5"
}
```

### 3.3 初始化（自动 / 手动）

**方式 A：`attachBaseContext` 启动自动初始化**（你提供的代码）

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

**方式 B：任意位置手动初始化（示例：按钮点击）**

```java
bt_main_02.setOnClickListener(v ->{
        if(FHook.

isInited()){
        FHook.

unInit();
    }else{
            if(!FHook.

init(this)){
        Toast.

makeText(this,"初始化失败",Toast.LENGTH_LONG).

show();
            Log.

e(TAG, "初始化失败");
        }else{
                Toast.

makeText(this,"初始化成功",Toast.LENGTH_LONG).

show();
            Log.

i(TAG, "初始化成功");
        }
                }
                });
```

> 常用：`FHook.unHookAll()` 解除全部 hook；`FHook.showHookInfo()` 查看当前 hook 状态。

### 3.4 最小可跑示例

#### A) Hook 普通方法（应用内方法）

以 `THook.fun_I_III(int a, int b, int c): int` 为例，演示 **改入参与返回值**：

```java
import java.lang.reflect.Method;

import android.util.Log;

Method m = THook.class.getMethod("fun_I_III", int.class, int.class, int.class);

FHook.

hook(m)
    .

setOrigFunRun(true) // 先跑原函数
    .

setHookEnter((thiz, args, types, hh) ->{
        // 改第一个入参
        args.

set(0,6666);
        Log.

d("FHook","fun_I_III enter: "+args);
    })
            .

setHookExit((ret, type, hh) ->{
        // 强制改返回
        Log.

d("FHook","fun_I_III exit, origRet="+ret);
        return 8888;
                })
                .

commit();
```

#### B) Hook 系统方法（设备指纹示例）

以 `Settings.Secure.getString(ContentResolver, String)` 为例，定向 **伪造 ANDROID\_ID**：

```java
import android.provider.Settings;
import android.content.ContentResolver;

import java.lang.reflect.Method;

import android.util.Log;

Method sysGet = Settings.Secure.class.getMethod(
        "getString", ContentResolver.class, String.class);

FHook.

hook(sysGet)
    .

setOrigFunRun(true)
    .

setHookEnter((thiz, args, types, hh) ->{
String key = (String) args.get(1);
        hh.extras.

put("key",key);
        Log.

d("FHook","Settings.Secure.getString key="+key);
    })
            .

setHookExit((ret, type, hh) ->{
String key = (String) hh.extras.get("key");
        if("android_id".

equalsIgnoreCase(key)){
        return"a1b2c3d4e5f6a7b8"; // 仅对 ANDROID_ID 生效
        }
        return ret; // 其它 key 保持原值
    })
            .

commit();
```

> 提示：接口/桥接方法（如 `SharedPreferences.Editor.commit`）建议用
> `FHookTool.findMethod4Impl(editor, ifaceMethod)` 找到 **真实实现方法** 再 hook，成功率更高。

---

## 4. 赞助与支持 · 授权与合作

* **个人学习与研究**：免费使用（详见仓库根目录 **[LICENSE.agent-binary](./LICENSE.agent-binary)**）。
* **商业/企业使用**：请严格遵守 **LICENSE.agent-binary**；如需 **商用授权、定制功能、技术支持或联合研发
  **，请通过 **GitHub Issues** 或仓库主页的联系方式沟通。
* **赞助方式**：欢迎 Star ⭐、提交 PR、或联系我们获取赞助通道。

> **合规声明**：FHook 仅用于合规场景。**严禁**将本项目用于任何违法违规用途，由此产生的风险与后果由使用者承担。

---

### 联系方式

* **商业合作 & 技术支持**：请通过以下方式联系 **裂痕科技（Feadre）**：

    * 邮箱：`rift@feadre.top`
    * QQ：`27113970`

* **赞助我们**（非常感谢！）：
  将二维码图片放在仓库 `docs/` 目录，并在此处引用即可：

  ```md
  <p align="left">
    <img src="docs/wx_rift_m.jpg" alt="微信赞助" width="180"/>
    <img src="docs/zfb_rift_m.jpg" alt="支付宝赞助" width="180"/>
  </p>
  ```

**尽情享受使用 FHook 的乐趣吧!**

### 附：注意事项

* **已知不支持/高危桥接方法**（详见项目内 `isBridgeCritical`）
  以下方法**通常不建议/无法直接 hook**：

  ```
  Thread.currentThread()
  Thread#getContextClassLoader()
  Class#getDeclaredMethod(String, Class[])
  ```

  这些多为桥接/反射入口，建议改为 **定位到真实实现方法** 再 hook（如先通过
  `FHookTool.findMethod4Impl(...)` 获取实现方法）。

* **互斥建议**：`Class.forName(...)` 与 `ClassLoader.loadClass(...)` **不要同时 hook**。
  两者存在相互调用/递归链路，容易导致死锁或卡死；框架未做强制限制，请自行规避。
* **桥接/反射桥方法**（如 `Method.invoke`）不建议直接 hook，请定位 **真实实现方法**（
  `FHookTool.findMethod4Impl`）。
* 回调运行在调用线程，避免在 UI 线程做重活。
* 不同 Android 版本的隐藏 API 限制不同，系统方法 hook 需做版本兼容验证。
* 若混淆影响调试，可临时添加（按需）：

```pro
-keep class top.feadre.fhook.** { *; }
或
-keep class top.feadre.fhook.CLinker {*;}
-keep class top.feadre.fhook.FHook {*;}
```

> 若使用中发现BUG或是其它意见建议，请随便联系作者或提交到 Issue 中，建议 **反馈失败的方法签名与系统版本
**，便于快速定位为您解决问题。





