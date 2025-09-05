package top.feadre.fhook;


import android.content.Context;
import android.os.Build;
import android.os.Debug;

import java.io.IOException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;


import top.feadre.fhook.flibs.fsys.FLog;

public class FHook {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "FHook";

    private static final String name_so_fhook_agent = "libfhook_agent.so";

    // 记录每个 methodId 对应的 HookHandle，供 onEnter/onExit 分发
    private static final ConcurrentHashMap<Long, HookHandle> sHandles = new ConcurrentHashMap<>();


//    static {
//        System.loadLibrary("ft002");
//    }

//    public static native int jcFt001T02(int a, int b);

//    public static void ft01() {
//        System.out.println("Hello FHook!");
//        Log.i("Ft001", "jcFt001T01: -------------------" + jcFt001T02(4, 5));
//    }

    static {
        System.loadLibrary("fhook");
    }

    private static boolean isInit = false;

    public static final class InitReport {
        public boolean jdwpEnabledTried;   // 尝试过开 JDWP
        public boolean jdwpEnabledNow;     // 当前是否允许 JDWP（native 读取）
        public boolean rtDebugTried;       // 尝试过置 Runtime debuggable
        public Boolean rtDebugNow;         // 当前 Runtime debuggable（可能为 null：符号不可得）
        public boolean attachOk;           // attach 成功
        public String firstError;         // 第一次 attach 失败的报错
        public String secondError;        // 第二次 attach 失败的报错
        public String note;               // 说明/提示

        @Override
        public String toString() {
            return "InitReport{" +
                    "jdwpEnabledNow=" + jdwpEnabledNow +
                    ", rtDebugNow=" + rtDebugNow +
                    ", attachOk=" + attachOk +
                    ", firstError='" + firstError + '\'' +
                    ", secondError='" + secondError + '\'' +
                    ", note='" + note + '\'' +
                    '}';
        }
    }


    /**
     * 这个必须在  Application attachBaseContext 中调用
     *
     * @param context
     * @return
     */
    public static synchronized InitReport init(Context context) {
        FLog.init(context, false);
        FLog.i(TAG, "FCFG.IS_DEBUG= " + FCFG_fhook.IS_DEBUG);

        if (FCFG_fhook.IS_DEBUG) {
            FLog.i(TAG, "debug mode");
            FLog.setLogLevel(FLog.VERBOSE);
        } else {
            FLog.i(TAG, "INFO mode");
            FLog.setLogLevel(FLog.INFO);
        }
        FLog.d(TAG, "[init] start ...");

        InitReport r = new InitReport();

        // 安卓9.0（ART） 28 以下不支持  Android 8.0+ 才有 JVMTI
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            FLog.e(TAG, "Android 9.0 28 以下不支持");
            r.note = "API<28 not supported";
            return r;
        }

        if (isInit) {
            FLog.w(TAG, "已经初始化过了");
            r.note = "already inited";
            r.attachOk = true; // 如果你想表达“已完成初始化”
            return r;
        }

        // 1) 只开 JDWP（最小改动）
        r.jdwpEnabledTried = true;
        // 恢复初始化
//        CLinker.dcEnableJdwp(false);
//        CLinker.dcSetRuntimeJavaDebuggable(0);
        boolean jdwpToggleOk = CLinker.dcEnableJdwp(true);
        r.jdwpEnabledNow = CLinker.dcIsJdwpAllowed();
        FLog.i(TAG, "JDWP enable tried=" + jdwpToggleOk + ", now=" + r.jdwpEnabledNow);

        try {
            // 注册代理
            Debug.attachJvmtiAgent(name_so_fhook_agent, null, context.getClassLoader());

            // c++ 加载 name_so_fhook_agent
            boolean res = CLinker.jcJvmtiSuccess(name_so_fhook_agent);
            if (res) {
                r.note = "attach success Jvmti 第一次就初始化成功";
                r.attachOk = true;
                FLog.i(TAG, "[init] " + r.note);
                isInit = true;
            } else {
                r.note = "dcJvmtiSuccess 第一次就初始化成功 加载lib失败 failed Jvmti 第1次就初始化失败";
            }
            return r;

        } catch (IOException e1) {
            // 第二次尝试 attach
            String msg = String.valueOf(e1.getMessage());
            FLog.w(TAG, "[init] attachJvmtiAgent 第一次加载失败: " + msg + "\n" + e1);

            // 如果像“非 debuggable 不允许”这类拒绝，再把 Runtime 设为 debuggable
            r.rtDebugTried = true;
            int rtState = CLinker.dcSetRuntimeJavaDebuggable(1); // 1=强制开

            r.rtDebugNow = (rtState == 1) ? Boolean.TRUE
                    : (rtState == 0 ? Boolean.FALSE : null);
            FLog.d(TAG, "[init] Runtime.setJavaDebuggable(true) -> " + rtState + " (1=T,0=F,-1=no symbol,-2=libart fail)");

            // 第二次尝试 attach
            FLog.w(TAG, "[init] attachJvmtiAgent 尝试第二次加载");
            try {
                Debug.attachJvmtiAgent("libfhook_agent.so", null, context.getClassLoader());

                // c++ 加载 name_so_fhook_agent
                boolean res = CLinker.jcJvmtiSuccess(name_so_fhook_agent);
                if (res) {
                    r.attachOk = true;
                    r.note = "attach success after enabling Runtime debuggable";
                    FLog.i(TAG, "[init] 第二次 attachJvmtiAgent success after enabling Runtime debuggable");
                    isInit = true;
                } else {
                    r.note = "[init] 第二次初始化成功 dcJvmtiSuccess 加载lib失败 failed Jvmti 第1次就初始化失败";
                }

                return r;
            } catch (IOException e2) {
                r.note = "attach failed again (check ROM policy / symbols / agent)";
                FLog.e(TAG, "[init] attachJvmtiAgent 第二次加载失败 " + e2.getMessage(), e2);
            }
            return r;
        }
    }


    static synchronized void installOnce(HookHandle h) {
        if (h == null || h.method == null) return;
        if (h.isHooked) return; // 已安装就别重复

        if (h.disabledByPrecheck) {
            FLog.e(TAG, "[installOnce] skip disabled handle: " + h.method);
            return;
        }

        boolean hasEnter = (h.enterCb != null);
        boolean hasExit = (h.exitCb != null);
        boolean runOrig = h.runOriginalByDefault;

        long mid = CLinker.dcHook(h.method, hasEnter, hasExit, runOrig);
        if (mid < 0) {
            FLog.e(TAG, "[installOnce] dcHook failed: " + mid);
            h.markNotHooked();
            return;
        }
        h.nativeHandle = mid;
        h.setHooked(true);
        sHandles.put(mid, h);
        FLog.i(TAG, "[installOnce] success: mid=" + mid +
                " enter=" + hasEnter + " exit=" + hasExit + " runOrig=" + runOrig);
    }


    public final static Object[] onEnter4fhook(Object[] rawArgs, long methodId) {
        FLog.d(TAG, "");
        FLog.d(TAG, "[onEnter4fhook] start ...");

        HookHandle hh = sHandles.get(methodId);
        if (hh == null) return rawArgs;

        hh.thisObject = rawArgs[0]; // 缓存 this 对象

        if (hh.enterCb == null) return rawArgs;

        // 参数引用转为 List
        java.util.List<Object> argsView =
                java.util.Arrays.asList(rawArgs).subList(1, rawArgs.length);
        final Class<?>[] paramTypes = hh.method.getParameterTypes();

        // 调试
        FHookTool.showOnEnterArgs(TAG, "onEnter4fhook", hh.method, rawArgs);

        try {
            hh.enterCb.onEnter(rawArgs[0], argsView, paramTypes, hh); // 允许直接修改 args[i]
        } catch (Throwable t) {
            FLog.e(TAG, "[onEnter4fhook] callback error", t);
        }

        FLog.i(TAG, "[onEnter4fhook] 完成 --- 改后数据= " + FHookTool.showOnEnterArgs4line(hh.method, rawArgs));
        return rawArgs;
    }

    public final static Object onExit4fhook(Object ret, long methodId) {
        FLog.d(TAG, "");
        FLog.d(TAG, "[onExit4fhook] start ...");
        HookHandle hh = sHandles.get(methodId);
        if (hh == null || hh.exitCb == null) return ret;

        final Class<?> returnType = hh.method.getReturnType();
        try {
            FLog.i(TAG, "[onExit4fhook] 开始 ---minfo " + hh.method.getDeclaringClass().getName() + "." + hh.method.getName());
            FLog.i(TAG, "[onExit4fhook] 开始 --- 原返回值= " + ret + ", 类型= " + returnType);

            Object res = hh.exitCb.onExit(ret, returnType, hh);
            FLog.i(TAG, "[onExit4fhook] 完成 --- 改后返回值= " + res + ", 类型= " + returnType);
            return res;
        } catch (Throwable t) {
            FLog.e(TAG, "[onExit4fhook] callback error", t);
            return ret;
        }
    }


    @FunctionalInterface
    public interface HookEnterCallback {
        void onEnter(Object thiz, List<Object> args, Class<?>[] paramTypes, HookHandle hh) throws Throwable;
    }

    @FunctionalInterface
    public interface HookExitCallback {
        Object onExit(Object ret, Class<?> returnType, HookHandle hh) throws Throwable;
    }

    public static void unHook(long methodId) {
        FLog.d(TAG, "[unHook] start ... methodId=" + methodId);
        if (!sHandles.containsKey(methodId)) {
            FLog.e(TAG, "[unHook] not found: " + methodId);
            return;
        }

        // 这里要调native 方法
        boolean b = CLinker.dcUnhook(methodId);
        if (!b) {
            FLog.e(TAG, "[unHook] dcUnhook failed: " + methodId);
            return;
        }
        sHandles.remove(methodId);
    }

    // 按类卸载这个类“声明的所有方法”的 hook
    public static void unHook(Class<?> cls) {
        if (cls == null) {
            FLog.e(TAG, "[unHook(Class)] cls = null");
            return;
        }
        FLog.d(TAG, "[unHook(Class)] start ... class=" + cls.getName());

        // 先收集，避免遍历时并发修改
        java.util.List<Long> toRemove = new java.util.ArrayList<>();
        for (java.util.Map.Entry<Long, HookHandle> e : sHandles.entrySet()) {
            HookHandle h = e.getValue();
            if (h == null || h.method == null) continue;
            if (h.method.getDeclaringClass() == cls) { // 只匹配该类自己声明的方法
                toRemove.add(e.getKey());
            }
        }

        if (toRemove.isEmpty()) {
            FLog.w(TAG, "[unHook(Class)] no hooks found for " + cls.getName());
            return;
        }

        int ok = 0, fail = 0;
        for (long mid : toRemove) {
            try {
                boolean b = CLinker.dcUnhook(mid);
                if (b) {
                    sHandles.remove(mid);
                    ok++;
                } else {
                    FLog.e(TAG, "[unHook(Class)] dcUnhook failed: mid=" + mid);
                    fail++;
                }
            } catch (Throwable t) {
                FLog.e(TAG, "[unHook(Class)] exception on mid=" + mid, t);
                fail++;
            }
        }
        FLog.i(TAG, "[unHook(Class)] done. class=" + cls.getName() + " ok=" + ok + " fail=" + fail);
    }


    public static void unHookAll() {
        FLog.d(TAG, "[unHookAll] start ...");
        for (long mid : sHandles.keySet()) {
            unHook(mid);
        }
    }

    public static void showHookInfo() {
        FLog.d(TAG, "[showHookInfo] start ...");
        if (sHandles.isEmpty()) {
            FLog.w(TAG, "[showHookInfo] no hook ...");
            return;
        }
        for (long mid : sHandles.keySet()) {
            HookHandle hh = sHandles.get(mid);
            FLog.d(TAG, "[showHookInfo] " + mid + " " + hh.method);
        }
    }

    ///  接口/抽象/native/<clinit> 等不支持的；  桥接方法
    public static boolean canHook(Method method) {
//        FLog.d(TAG, "[canHook] start ... method=" + method);
        if (method == null) {
            FLog.e(TAG, "[canHook] method == null");
            return false;
        }
        Class<?> declaring = method.getDeclaringClass();
        int mods = method.getModifiers();
        if (declaring.isInterface() || declaring.isAnnotation()) {
            FLog.e(TAG, "[canHook] interface/annotation/abstract/native/<clinit> 不支持: " + method);
            return false;
        }
        if (Modifier.isAbstract(mods)) {
            FLog.e(TAG, "[canHook] abstract 不支持: " + method);
            return false;
        }
        if (Modifier.isNative(mods)) {
            FLog.e(TAG, "[canHook] native 不支持: " + method);
            return false;
        }
        if ("<clinit>".equals(method.getName())) {
            FLog.e(TAG, "[canHook] <clinit> 不支持: " + method);
            return false;
        }

        if (isBridgeCritical(method)) return false;

        return true;
    }


    /// 批量 hook
    public static final class GroupHandle {
        private final Class<?> targetClass;
        private final java.util.List<HookHandle> handles = new java.util.ArrayList<>();
        private boolean committed = false;

        GroupHandle(Class<?> targetClass) {
            this.targetClass = targetClass;
        }

        // 批量设置：默认是否调用原方法
        public GroupHandle setOrigFunRun(boolean runOrig) {
            for (HookHandle h : handles) {
                try {
                    h.setOrigFunRun(runOrig);
                } catch (Throwable ignore) {
                }
            }
            return this;
        }

        // 批量设置：进入回调
        public GroupHandle setHookEnter(HookEnterCallback cb) {
            for (HookHandle h : handles) {
                try {
                    h.setHookEnter(cb);
                } catch (Throwable ignore) {
                }
            }
            return this;
        }

        // 批量设置：退出回调
        public GroupHandle setHookExit(HookExitCallback cb) {
            for (HookHandle h : handles) {
                try {
                    h.setHookExit(cb);
                } catch (Throwable ignore) {
                }
            }
            return this;
        }

        // 安装本组所有 hook
        public GroupHandle commit() {
            if (committed) return this;
            for (HookHandle h : handles) {
                try {
                    FHook.installOnce(h);
                } catch (Throwable t) {
                    top.feadre.fhook.flibs.fsys.FLog.e(TAG, "[GroupHandle.commit] install failed: " + h, t);
                }
            }
            committed = true;
            top.feadre.fhook.flibs.fsys.FLog.i(TAG, "[GroupHandle.commit] done. class=" + targetClass.getName()
                    + " total=" + handles.size());
            return this;
        }

        // 可选：返回本组的每个 HookHandle
        public java.util.List<HookHandle> getHandles() {
            return handles;
        }
    }


    /**
     * !canHook(m)：接口/抽象/native/<clinit> 等不支持的；
     *
     * @param cls
     * @return
     */
    public static synchronized GroupHandle hook(Class<?> cls) {
        FLog.d(TAG, "[hook(Class)] start ... class=" + cls.getName());
        if (!isInit) {
            FLog.e(TAG, "[hook(Class)] 请先初始化 call FHook.init() first");
            return new GroupHandle(cls); // 返回空组，避免 NPE
        }
        if (cls == null) {
            FLog.e(TAG, "[hook(Class)] cls = null");
            return new GroupHandle(null);
        }
        if (cls.isInterface() || cls.isAnnotation()) {
            FLog.e(TAG, "[hook(Class)] 传入的是接口/注解： " + cls.getName() + "，不能直接重写；请先找实现类再批量 hook");
            return new GroupHandle(cls);
        }

        GroupHandle group = new GroupHandle(cls);

        // 只枚举“本类声明”的方法；如需包含父类可换 getMethods()
        Method[] methods;
        try {
            methods = cls.getDeclaredMethods();
        } catch (Throwable t) {
            FLog.e(TAG, "[hook(Class)] getDeclaredMethods 失败: " + cls.getName(), t);
            return group;
        }

        int ok = 0, skip = 0;
        for (Method m : methods) {
            // 过滤掉不支持/不需要的
            int mods = m.getModifiers();
            if (!canHook(m)) {
                skip++;
                continue;
            }
            if (m.isSynthetic() || m.isBridge()) {
                skip++;
                continue;
            } // 降噪
            // 避免破坏锁边界（可选，仅提示，不跳过）
            if (Modifier.isSynchronized(mods)) {
                FLog.w(TAG, "[hook(Class)] synchronized 方法：" + cls.getName() + "#" + m.getName());
            }
            try {
                if (!m.isAccessible()) m.setAccessible(true);
            } catch (Throwable ignore) {
            }

            // 复用你已有的单方法入口
            HookHandle h = hook(m);
            // 失败的会被 markNotHooked，这里仍然收集，方便统一设置/commit
            group.handles.add(h);
            ok++;
        }

        // collected：实际被加入 group.handles 的方法数
        FLog.i(TAG, "[hook(Class)] 收集完成 class=" + cls.getName()
                + " methods=" + methods.length + " collected=" + ok + " skipped=" + skip);
        return group;
    }

    /// FHook.hook(o1).setOrigFunRun(false).setHookEnter(...).commit(); kipped：被跳过的不处理的方法数。
    public static synchronized GroupHandle hook(Object obj) {
        FLog.d(TAG, "[hook(Object)] start ... " + obj);
        if (!isInit) {
            FLog.e(TAG, "[hook(Object)] 请先初始化 call FHook.init() first");
            return new GroupHandle(null);
        }
        if (obj == null) {
            FLog.e(TAG, "[hook(Object)] obj = null");
            return new GroupHandle(null);
        }

        Class<?> cls = obj.getClass();

        // 一些提示：匿名/合成/代理类可能不是你真正想要的实现类
        try {
            if (cls.isAnonymousClass() || cls.isSynthetic()) {
                FLog.w(TAG, "[hook(Object)] 目标类是匿名/合成类：" + cls.getName() + "，将直接尝试对其声明的方法进行 hook");
            }
            // Proxy 场景仅提示（Android 上少见对业务对象用动态代理）
            try {
                if (java.lang.reflect.Proxy.isProxyClass(cls)) {
                    FLog.w(TAG, "[hook(Object)] 目标是 JDK 动态代理类：" + cls.getName()
                            + "（通常应考虑 hook InvocationHandler 或具体实现类）");
                }
            } catch (Throwable ignore) {
            }
        } catch (Throwable ignore) {
        }

        // 如果当前类不可直接 hook（抽象/接口等），向上找一个可 hook 的具体类
        Class<?> cur = cls;
        while (cur != null && !FHookTool.isHookableClass(cur)) {
            cur = cur.getSuperclass();
        }
        if (cur == null || cur == Object.class) {
            FLog.e(TAG, "[hook(Object)] 未找到可 hook 的具体实现类，原始：" + cls.getName());
            return new GroupHandle(null);
        }

        // 复用你现有的“按类批量 hook”
        return hook(cur);
    }

    /**
     * JVMTI 不支持类型
     * 接口/注解接口：declaringClass.isInterface() / isAnnotation()。
     * 抽象方法：Modifier.isAbstract(method.getModifiers())（没有 CodeItem）。
     * native 方法：Modifier.isNative(...)（没有 dex code，JVMTI 也不能给它换实现）。
     * 类初始化方法 <clinit>：理论上可以改字节码，但在 ART 上高风险（容易触发初始化时机/验证问题），通常当作不支持处理。
     * 类/方法产生的变更涉及结构性修改（增删方法/字段、改签名/继承结构等）：ART 仅支持不改变类结构的“换方法体”型重转换。
     *
     * @param method
     * @return
     */
    public static synchronized HookHandle hook(Method method) {
        if (!isInit) {
            FLog.e(TAG, "[hook] 请先初始化 call FHook.init() first");
            return new HookHandle(-1, method).markNotHooked();
        }

        if (method == null) {
            FLog.e(TAG, "[hook] method = null 没有找到 ...");
            return new HookHandle(-1, method).markNotHooked();
        }

        if (!canHook(method)) {
            FLog.e(TAG, "[hook] 不支持的 hook 方法：" + method);
            return new HookHandle(-1, method).markNotHooked();
        }

        // 判断 JVMTI  是否支持 Retransform
        final Class<?> declaring = method.getDeclaringClass();
        final int mods = method.getModifiers();
        if (declaring.isInterface() || declaring.isAnnotation()) {
            FLog.e(TAG, "[hook] 目标是接口/注解接口：" + declaring.getName() + "#" + method.getName() + " 不支持 Retransform");
            return new HookHandle(-1, method).markNotHooked();
        }

        if (Modifier.isAbstract(mods)) {
            FLog.e(TAG, "[hook] 抽象方法无代码：" + declaring.getName() + "#" + method.getName());
            return new HookHandle(-1, method).markNotHooked();
        }

        if (Modifier.isNative(mods)) {
            FLog.e(TAG, "[hook] native 方法无法用字节码重写：" + declaring.getName() + "#" + method.getName());
            return new HookHandle(-1, method).markNotHooked();
        }

        // （保守）避免 <clinit>
        if (method.getName().equals("<clinit>")) {
            FLog.e(TAG, "[hook] 不建议 hook <clinit>（类初始化器）");
            return new HookHandle(-1, method).markNotHooked();
        }

        if (declaring.getClassLoader() == null) { // Bootstrap/BootClassPath
            FLog.w(TAG, "[hook] 警告：" + declaring.getName() + " 属于 BootClassPath，部分 ROM/策略可能禁止重转换");
        }

        if (Modifier.isSynchronized(mods)) {
            FLog.w(TAG, "[hook] 警告：" + declaring.getName() + "#" + method.getName()
                    + " 是 synchronized，若hook破坏锁边界可能验证失败");
        }

        return new HookHandle(-1, method);
    }


    /**
     * 桥接方法（JVMTI 不支持）
     *
     * @param m
     * @return
     */
    private static boolean isBridgeCritical(Method m) {
        if (m == null) return false;
        final String cn = m.getDeclaringClass().getName();
        final String mn = m.getName();
        final Class<?>[] ps = m.getParameterTypes();
        FLog.d(TAG, "[isBridgeCritical] " + cn + "#" + mn + "(" + ps.length + ")");

        if (!FCFG_fhook.ENABLE_HOOK_CRUX) {
            // Thread.currentThread()
            if (cn.equals("java.lang.Thread") && mn.equals("currentThread") && ps.length == 0) {
                FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn);
                return true;
            }

            // Thread.getContextClassLoader()
            if (cn.equals("java.lang.Thread") && mn.equals("getContextClassLoader") && ps.length == 0) {
                FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn);
                return true;
            }

            // ClassLoader.loadClass(String)  —— 我们桥接里用的是这个重载
            if (cn.equals("java.lang.ClassLoader") && mn.equals("loadClass")
                    && ps.length == 1 && ps[0] == String.class) {
                FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn + "(String)");
                return true;
            }

            // Class.getDeclaredMethod(String, Class[])
            if (cn.equals("java.lang.Class") && mn.equals("getDeclaredMethod")
                    && ps.length == 2 && ps[0] == String.class && ps[1] == Class[].class) {
                FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn + "(String,Class[])");
                return true;
            }

            // Method.invoke(Object, Object[])  native 方法，不支持 JVMTI
            if (cn.equals("java.lang.reflect.Method") && mn.equals("invoke")
                    && ps.length == 2 && ps[1] == Object[].class) {
                FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn + "(Object,Object[])");
                return true;
            }
        }


        if (cn.startsWith("top.feadre.fhook.")) {
            FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn);
            return true;
        }

        return false;
    }


}
