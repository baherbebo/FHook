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


    /**
     * 这个必须在  Application attachBaseContext 中调用
     *
     * @param context
     * @return
     */
    public static synchronized boolean init(Context context) {
        FLog.init(context, false);
        FLog.i(TAG, "FCFG.IS_DEBUG= " + FCFG_fhook.IS_DEBUG);
        FLog.setLogLevel(FCFG_fhook.IS_DEBUG ? FLog.VERBOSE : FLog.INFO);
        FLog.d(TAG, "[init] start ...");

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            FLog.e(TAG, "Android 9.0 28 以下不支持");
            return false;
        }
        if (isInit) {
            FLog.w(TAG, "已经初始化过了");
            return true;
        }

        // （可选）受配置保护是否允许开启 JDWP/RuntimeDebuggable
        boolean jdwpToggleOk = CLinker.dcEnableJdwp(true);
        boolean jdwpNow = CLinker.dcIsJdwpAllowed();
        FLog.i(TAG, "JDWP enable tried=" + jdwpToggleOk + ", now=" + jdwpNow);

        try {
            Debug.attachJvmtiAgent("libfhook_agent.so", null, context.getClassLoader());
            boolean ok = CLinker.jcJvmtiSuccess("libfhook_agent.so", FCFG_fhook.IS_SAFE_MODE);
            if (ok) {
                isInit = true;
                FLog.i(TAG, "[init] attach success (first try)");
                return true;
            } else {
                FLog.w(TAG, "[init] first attach ok but agent check failed");
            }
        } catch (IOException e1) {
            FLog.w(TAG, "[init] first attach failed: " + e1.getMessage() + e1);
            int rtState = CLinker.dcSetRuntimeJavaDebuggable(1); // 1=强制开
            FLog.d(TAG, "[init] setJavaDebuggable(true) -> " + rtState + " (1=T,0=F,-1=no symbol,-2=libart fail)");
            try {
                Debug.attachJvmtiAgent("libfhook_agent.so", null, context.getClassLoader());
                boolean ok2 = CLinker.jcJvmtiSuccess("libfhook_agent.so", FCFG_fhook.IS_SAFE_MODE);
                if (ok2) {
                    isInit = true;
                    FLog.i(TAG, "[init] attach success after enabling Runtime debuggable");
                    return true;
                } else {
                    FLog.w(TAG, "[init] second attach ok but agent check failed");
                }
            } catch (IOException e2) {
                FLog.e(TAG, "[init] second attach failed: " + e2.getMessage(), e2);
            }
        }
        return false;
    }

    /**
     * 关闭 FHook：卸载全部 hook，并（可选）关闭调试桥接能力（JDWP / Runtime JavaDebuggable）。
     *
     * @param disableDebugBridge true=尝试关闭 JDWP，并把 Runtime JavaDebuggable 复位为 0；false=保留当前调试状态
     */
    public static synchronized void unInit(boolean disableDebugBridge) {
        FLog.d(TAG, "[unInit] start ... disableDebugBridge=" + disableDebugBridge);

        // 1) 卸载所有 hook（内部已做快照遍历）
        try {
            unHookAll();
        } catch (Throwable t) {
            FLog.e(TAG, "[unInit] unHookAll exception", t);
        }

        // 2) 清理句柄表（unHookAll 会逐个移除，这里再次确保）
        try {
            sHandles.clear();
        } catch (Throwable ignore) { }

        // 3) 可选：关闭调试桥接（注意不同 ROM 可能无效或被策略拦截）
        if (disableDebugBridge) {
            try {
                int rt = CLinker.dcSetRuntimeJavaDebuggable(0); // 0=关；-1/-2 见你之前的约定
                FLog.i(TAG, "[unInit] setJavaDebuggable(false) -> " + rt);
            } catch (Throwable t) {
                FLog.w(TAG, "[unInit] setJavaDebuggable(false) failed", t);
            }
            try {
                boolean ok = CLinker.dcEnableJdwp(false);
                boolean now = CLinker.dcIsJdwpAllowed();
                FLog.i(TAG, "[unInit] disable JDWP tried=" + ok + ", now=" + now);
            } catch (Throwable t) {
                FLog.w(TAG, "[unInit] disable JDWP failed", t);
            }
        }

        // 4) 置初始化标志
        isInit = false;

        FLog.i(TAG, "[unInit] done. isInit=" + isInit + ", handles=" + sHandles.size());
    }

    /** 无参便捷版：默认关闭调试桥接能力 */
    public static synchronized void unInit() {
        unInit(true);
    }

    /** 可选：对外暴露当前初始化状态 */
    public static boolean isInited() {
        return isInit;
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
        //使用快照处理
        java.util.ArrayList<Long> ids = new java.util.ArrayList<>(sHandles.keySet());
        for (long mid : ids) {
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

            // 去重
            java.util.LinkedHashMap<Method, HookHandle> uniq = new java.util.LinkedHashMap<>();
            for (HookHandle h : handles) {
                if (h != null && h.method != null) uniq.put(h.method, h);
            }
            handles.clear();
            handles.addAll(uniq.values());

            for (HookHandle h : handles) {
                try {
                    FHook.installOnce(h);
                } catch (Throwable t) {
                    FLog.e(TAG, "[GroupHandle.commit] install failed: " + h, t);
                }
            }
            committed = true;
            final String cname = (targetClass == null ? "null" : targetClass.getName());
            FLog.i(TAG, "[GroupHandle.commit] done. class=" + cname + " total=" + handles.size());
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
    public static synchronized GroupHandle hookClassAllFuns(Class<?> cls) {
        FLog.d(TAG, "[hook(Class)] start ... class=" + (cls == null ? "null" : cls.getName()));
        return hookClassInternal(cls, null, false);
    }

    /**
     * 按对象批量 hook（向上寻找可 hook 的具体实现类；仅本类声明的方法）
     */
    public static synchronized GroupHandle hookObjAllFuns(Object obj) {
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
    public static synchronized GroupHandle hookOverloads(Class<?> clazz, String name_fun) {
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
//        FLog.d(TAG, "[isBridgeCritical] " + cn + "#" + mn + "(" + ps.length + ")");

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
