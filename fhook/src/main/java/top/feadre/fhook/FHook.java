package top.feadre.fhook;

import android.content.Context;
import android.os.Build;
import android.os.Debug;

import java.io.IOException;
import java.lang.reflect.Member;
import java.lang.reflect.Method;

import top.feadre.fhook.flibs.fsys.FLog;

public class FHook {
    private static final String TAG = FCFG.TAG_PREFIX + "FHook";

    private static final String name_so_fhook_agent = "libfhook_agent.so";


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

    private static boolean containsAny(String s, String... keys) {
        s = s.toLowerCase();
        for (String k : keys) if (s.contains(k)) return true;
        return false;
    }

    /**
     * 这个必须在  Application attachBaseContext 中调用
     *
     * @param context
     * @return
     */
    public static synchronized InitReport init(Context context) {
        FLog.init(context, false);
        FLog.setLogLevel(FLog.VERBOSE);
        FLog.d(TAG, "[init] start ...");

        InitReport r = new InitReport();

        // 安卓9.0 28 以下不支持
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
            boolean res = CLinker.dcJvmtiSuccess(name_so_fhook_agent);
            if (res) {
                r.note = "attach success Jvmti 第一次就初始化成功";
                r.attachOk = true;
                FLog.i(TAG, "[init] " + r.note);
                isInit = true;
            }else {
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
                boolean res = CLinker.dcJvmtiSuccess(name_so_fhook_agent);
                if (res) {
                    r.attachOk = true;
                    r.note = "attach success after enabling Runtime debuggable";
                    FLog.i(TAG, "[init] 第二次 attachJvmtiAgent success after enabling Runtime debuggable");
                    isInit = true;
                }else {
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


    public static synchronized HookHandle hook(Method method) {
        if (!isInit) {
            throw new IllegalStateException("[hook] 请先初始化 call FHook.init() first");
        }

        if (method == null) {
            throw new IllegalArgumentException("[hook] method can not be null");
        }

        long methodId = CLinker.dcHook(method, true, true, true);

        HookHandle handle = new HookHandle(123, method);

        return handle;
    }


    public static Object[] onEnter4fhook(Object[] args, long methodId) {
        FLog.d(TAG, "onEnter4fhook ...");
        return args;
    }

    public static Object onExit4fhook(Object ret, long methodId) {
        FLog.d(TAG, "onExit4fhook ....");

        return ret;
    }


    @FunctionalInterface
    public interface HookEnterCallback {
        void onEnter(HookParam param) throws Throwable;
    }

    @FunctionalInterface
    public interface HookExitCallback {
        void onExit(HookParam param) throws Throwable;
    }

}
