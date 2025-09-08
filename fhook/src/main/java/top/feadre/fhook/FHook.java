package top.feadre.fhook;


import android.content.Context;
import android.os.Build;
import android.os.Debug;

import java.io.IOException;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
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
    public static final MethodHandle MH_ENTER;
    public static final MethodHandle MH_EXIT;


    static {
        System.loadLibrary("fhook");

        try {
            //  用于 hook 系统API 的全局方法变量
            var mtEnter = MethodType.methodType(Object[].class, Object[].class, long.class);
            var mtExit = MethodType.methodType(Object.class, Object.class, long.class);
            var lk = MethodHandles.lookup();
            MH_ENTER = lk.findStatic(FHook.class, "onEnter4fhook", mtEnter);
            MH_EXIT = lk.findStatic(FHook.class, "onExit4fhook", mtExit);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
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
            boolean res = CLinker.jcJvmtiSuccess(name_so_fhook_agent, FCFG_fhook.IS_SAFE_MODE);
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
                boolean res = CLinker.jcJvmtiSuccess(name_so_fhook_agent, FCFG_fhook.IS_SAFE_MODE);
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


    // 统一收集并包装：在 clazz 中按 nameFilter（可空）筛选方法；
    // searchInherited=true 时，声明方法没命中则回退到 getMethods()（含父类 public）。
    private static GroupHandle hookClassInternal(Class<?> clazz,
                                                 String nameFilter,
                                                 boolean searchInherited) {
        final String where = "[hookClassInternal]";
        if (!isInit) {
            FLog.e(TAG, where + " 请先初始化 call FHook.init() first");
            return new GroupHandle(clazz);
        }
        if (clazz == null) {
            FLog.e(TAG, where + " clazz = null");
            return new GroupHandle(null);
        }
        if (clazz.isInterface() || clazz.isAnnotation()) {
            FLog.e(TAG, where + " 传入的是接口/注解： " + clazz.getName() + "，不能直接重写；请先找实现类");
            return new GroupHandle(clazz);
        }

        GroupHandle group = new GroupHandle(clazz);

        // 1) 本类声明的方法
        Method[] declared;
        try {
            declared = clazz.getDeclaredMethods();
        } catch (Throwable t) {
            FLog.e(TAG, where + " getDeclaredMethods 失败: " + clazz.getName(), t);
            return group;
        }

        java.util.List<Method> targets = new java.util.ArrayList<>();
        for (Method m : declared) {
            if (nameFilter == null || nameFilter.equals(m.getName())) {
                targets.add(m);
            }
        }

        // 2) 回退：继承而来的 public（仅在需要时）
        boolean usedFallback = false;
        if (targets.isEmpty() && searchInherited) {
            try {
                for (Method m : clazz.getMethods()) {
                    if (nameFilter == null || nameFilter.equals(m.getName())) {
                        targets.add(m);
                    }
                }
                if (!targets.isEmpty()) {
                    usedFallback = true;
                    FLog.w(TAG, where + " 未在 " + clazz.getName()
                            + " 的声明方法中找到匹配；改用 public(可能来自父类/接口) 的重载，共 "
                            + targets.size() + " 个。注意：实际 hook 的是各自声明类的方法。");
                }
            } catch (Throwable t) {
                FLog.e(TAG, where + " getMethods 回退失败: " + clazz.getName(), t);
            }
        }

        if (targets.isEmpty()) {
            FLog.e(TAG, where + " 未找到匹配方法： " + clazz.getName()
                    + (nameFilter == null ? " 〈全部〉" : ("#" + nameFilter)));
            return group;
        }

        int collected = 0, skipped = 0;
        for (Method m : targets) {
            try {
                // 降噪 / 黑名单
                if (m.isSynthetic() || m.isBridge() || isBridgeCritical(m)) {
                    skipped++;
                    continue;
                }
                try {
                    if (!m.isAccessible()) m.setAccessible(true);
                } catch (Throwable ignore) {
                }

                if (!canHook(m)) { // 接口/抽象/native/<clinit> 等直接跳过
                    skipped++;
                    continue;
                }

                if (Modifier.isSynchronized(m.getModifiers())) {
                    FLog.w(TAG, where + " synchronized 方法："
                            + m.getDeclaringClass().getName() + "#" + m.getName());
                }

                // 复用单方法入口（其中仍会再次做轻量校验与提示）
                HookHandle h = hook(m);
                group.getHandles().add(h); // 即使 markNotHooked 也纳入，便于统一 set()/commit()
                collected++;
            } catch (Throwable t) {
                skipped++;
                FLog.e(TAG, where + " 处理方法失败："
                        + m.getDeclaringClass().getName() + "#" + m.getName(), t);
            }
        }

        FLog.i(TAG, where + " 完成 class=" + clazz.getName()
                + " filter=" + (nameFilter == null ? "<ALL>" : nameFilter)
                + " found=" + targets.size()
                + (usedFallback ? " (via getMethods fallback)" : "")
                + " collected=" + collected + " skipped=" + skipped);
        return group;
    }

    /**
     * 按类批量 hook（仅本类声明的方法）
     */
    public static synchronized GroupHandle hook(Class<?> cls) {
        FLog.d(TAG, "[hook(Class)] start ... class=" + (cls == null ? "null" : cls.getName()));
        return hookClassInternal(cls, null, false);
    }

    /**
     * 按对象批量 hook（向上寻找可 hook 的具体实现类；仅本类声明的方法）
     */
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
        // 找到可 hook 的具体类
        Class<?> cur = cls;
        while (cur != null && !FHookTool.isHookableClass(cur)) {
            cur = cur.getSuperclass();
        }
        if (cur == null || cur == Object.class) {
            FLog.e(TAG, "[hook(Object)] 未找到可 hook 的具体实现类，原始：" + cls.getName());
            return new GroupHandle(null);
        }
        return hookClassInternal(cur, null, false);
    }

    /**
     * 单方法入口（真正的可用性以 canHook/isBridgeCritical 为准）
     */
    public static synchronized HookHandle hook(Method method) {
        if (!isInit) {
            FLog.e(TAG, "[hook(Method)] 请先初始化 call FHook.init() first");
            return new HookHandle(-1, method).markNotHooked();
        }
        if (method == null) {
            FLog.e(TAG, "[hook(Method)] method = null");
            return new HookHandle(-1, null).markNotHooked();
        }

        // 不可 hook 的类型（已在 canHook 内部覆盖了绝大部分）
        if (!canHook(method)) {
            FLog.e(TAG, "[hook(Method)] 不支持的 hook 方法：" + method);
            return new HookHandle(-1, method).markNotHooked();
        }
        if (isBridgeCritical(method)) {
            FLog.e(TAG, "[hook(Method)] 桥接关键方法（禁止）： " + method);
            return new HookHandle(-1, method).markNotHooked();
        }

        // 只做轻提示，不再重复业务判断
        final Class<?> declaring = method.getDeclaringClass();
        final int mods = method.getModifiers();
        if (declaring.getClassLoader() == null) { // Bootstrap/BootClassPath
            FLog.w(TAG, "[hook(Method)] 警告：" + declaring.getName()
                    + " 属于 BootClassPath，部分 ROM/策略可能禁止重转换");
        }
        if (Modifier.isSynchronized(mods)) {
            FLog.w(TAG, "[hook(Method)] 警告：synchronized 方法，若破坏锁边界可能验证失败 — "
                    + declaring.getName() + "#" + method.getName());
        }

        return new HookHandle(-1, method);
    }

    /**
     * 指定类与方法名：hook 其所有重载（先本类声明，未命中再回退到继承的 public）
     */
    public static synchronized GroupHandle hook(Class<?> clazz, String name_fun) {
        FLog.d(TAG, "[hook(Class,String)] start ... class=" + (clazz == null ? "null" : clazz.getName())
                + " name=" + name_fun);
        if (name_fun == null || name_fun.length() == 0) {
            FLog.e(TAG, "[hook(Class,String)] 方法名为空");
            return new GroupHandle(clazz);
        }
        return hookClassInternal(clazz, name_fun, true);
    }


    /**
     * A组  不能做任何操作
     * Thread thread = Thread.currentThread();
     * ClassLoader contextClassLoader = thread.getContextClassLoader();
     * B组  能hook，强制不能清除方法
     * Class<?> clazz = contextClassLoader.loadClass("top.feadre.fhook.FHookTool");
     * method = clazz.getDeclaredMethod("printStackTrace", int.class);
     * Object res = method.invoke(null, 5);
     * C组  能hook，可清除（但不能同时hookB组） -
     * Class<?> clazz = Class.forName("top.feadre.fhook.FHookTool", true, contextClassLoader);
     * MethodHandles.Lookup lk = MethodHandles.publicLookup();
     * MethodType mt = MethodType.methodType(void.class, int.class);
     * MethodHandle mh = lk.findStatic(clazz, "printStackTrace", mt);
     * Object res = mh.invokeWithArguments(5);
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

        ///  这两个方法全局禁止使用
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

//        // ClassLoader.loadClass(String)  —— 我们桥接里用的是这个重载
//        if (cn.equals("java.lang.ClassLoader") && mn.equals("loadClass")
//                && ps.length == 1 && ps[0] == String.class) {
//            FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn + "(String)");
//            return true;
//        }
//
//        // Class.getDeclaredMethod(String, Class[])
//        if (cn.equals("java.lang.Class") && mn.equals("getDeclaredMethod")
//                && ps.length == 2 && ps[0] == String.class && ps[1] == Class[].class) {
//            FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn + "(String,Class[])");
//            return true;
//        }
//
//        // Method.invoke(Object, Object[])  native 方法，不支持 JVMTI
//        if (cn.equals("java.lang.reflect.Method") && mn.equals("invoke")
//                && ps.length == 2 && ps[1] == Object[].class) {
//            FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn + "(Object,Object[])");
//            return true;
//        }


        if (cn.startsWith("top.feadre.fhook.FHook")) {
            FLog.e(TAG, "[isBridgeCritical] 桥接方法：" + cn + "#" + mn);
            return true;
        }

        return false;
    }


}
