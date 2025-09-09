package top.feadre.fhook.activitys;

import java.lang.reflect.Method;

import top.feadre.fhook.FCFG_fhook;
import top.feadre.fhook.FHook;
import top.feadre.fhook.flibs.fsys.FLog;

public class MainActivityHelp {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "MainActivityHelp";


    private final MainActivity mMainActivity;

    public MainActivityHelp(MainActivity mainActivity) {
        mMainActivity = mainActivity;
    }

    public void do_init_views() {


    }

    /*
        进程控制 / 退出 / 执行外部命令

java.lang.System.exit(int) → (I)V
java.lang.Runtime.exit(int) → (I)V
java.lang.Runtime.halt(int) → (I)V
java.lang.Runtime.exec(String) → (Ljava/lang/String;)Ljava/lang/Process;
java.lang.Runtime.exec(String, String[]) → (Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/Process;
java.lang.Runtime.exec(String[], String[]) → ([Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/Process;
java.lang.Runtime.exec(String, String[], java.io.File) → (Ljava/lang/String;[Ljava/lang/String;Ljava/io/File;)Ljava/lang/Process;
java.lang.Runtime.exec(String[], String[], java.io.File) → ([Ljava/lang/String;[Ljava/lang/String;Ljava/io/File;)Ljava/lang/Process;
java.lang.ProcessBuilder.start() → ()Ljava/lang/Process;
android.os.Process.killProcess(int) → (I)V
android.os.Process.sendSignal(int, int) → (II)V


        动态加载 / 代码注入相关
        java.lang.System.load(String) → (Ljava/lang/String;)V
java.lang.System.loadLibrary(String) → (Ljava/lang/String;)V
java.lang.Runtime.load(String) → (Ljava/lang/String;)V
java.lang.Runtime.loadLibrary(String) → (Ljava/lang/String;)V
java.lang.Class.forName(String) → (Ljava/lang/String;)Ljava/lang/Class;
java.lang.Class.forName(String, boolean, ClassLoader) → (Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;
java.lang.ClassLoader.loadClass(String) → (Ljava/lang/String;)Ljava/lang/Class;
java.lang.ClassLoader.loadClass(String, boolean) → (Ljava/lang/String;Z)Ljava/lang/Class;
dalvik.system.DexClassLoader.<init>(String,String,String,ClassLoader) → (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V
dalvik.system.PathClassLoader.<init>(String,ClassLoader) → (Ljava/lang/String;Ljava/lang/ClassLoader;)V
dalvik.system.DexFile.loadDex(String,String,int) (static) → (Ljava/lang/String;Ljava/lang/String;I)Ldalvik/system/DexFile;
dalvik.system.DexFile.loadClass(String,ClassLoader) → (Ljava/lang/String;Ljava/lang/ClassLoader;)Ljava/lang/Class;
dalvik.system.InMemoryDexClassLoader.<init>(java.nio.ByteBuffer,ClassLoader) → (Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V

反射调用：
java.lang.Class.getDeclaredMethod(String, Class[]) → (Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;
java.lang.reflect.Method.invoke(Object, Object[]) → (Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;
java.lang.reflect.Constructor.newInstance(Object[]) → ([Ljava/lang/Object;)Ljava/lang/Object;
java.lang.reflect.Proxy.newProxyInstance(ClassLoader, Class[], InvocationHandler) → (Ljava/lang/ClassLoader;[Ljava/lang/Class;Ljava/lang/reflect/InvocationHandler;)Ljava/lang/Object;


系统属性 / 设备指纹
（隐藏 API）android.os.SystemProperties.get(String) → (Ljava/lang/String;)Ljava/lang/String;
android.os.SystemProperties.get(String, String) → (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
android.os.SystemProperties.getInt(String, int) → (Ljava/lang/String;I)I
android.os.SystemProperties.getLong(String, long) → (Ljava/lang/String;J)J
android.os.SystemProperties.getBoolean(String, boolean) → (Ljava/lang/String;Z)Z
android.provider.Settings$Secure.getString(ContentResolver, String) → (Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;
android.provider.Settings$System.getString(ContentResolver, String) → (Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;
android.os.Build.getSerial() → ()Ljava/lang/String;（需要权限）
android.telephony.TelephonyManager.getDeviceId() → ()Ljava/lang/String;（旧）
android.telephony.TelephonyManager.getDeviceId(int) → (I)Ljava/lang/String;
android.telephony.TelephonyManager.getImei() → ()Ljava/lang/String;
android.telephony.TelephonyManager.getImei(int) → (I)Ljava/lang/String;
android.telephony.TelephonyManager.getMeid() / getMeid(int) → ()Ljava/lang/String; / (I)Ljava/lang/String;
android.telephony.TelephonyManager.getSubscriberId()（IMSI）→ ()Ljava/lang/String;
android.telephony.TelephonyManager.getSimSerialNumber() → ()Ljava/lang/String;

     */
    public void do_init_hooks() {
        try {
            hook_System_exit();              // 进程控制
            hook_Runtime_exec_1();           // 外部命令
            hook_System_loadLibrary();       // so 加载
            hook_SystemProperties_get();     // 系统属性
            hook_Settings_Secure_getString();// 设备指纹
            FLog.i(TAG, "All system hooks installed.");
        } catch (Throwable t) {
            FLog.e(TAG, "installAll failed", t);
        }

    }

    // Hook: java.lang.System.exit(int)  → (I)V
    private static void hook_System_exit() throws Exception {
        Method m = System.class.getDeclaredMethod("exit", int.class);
        FHook.hook(m)
                .setOrigFunRun(false) // 阻断退出
                .setHookEnter((thiz, args, types, hh) -> {
                    int code = (args.get(0) instanceof Integer) ? (Integer) args.get(0) : -1;
                    FLog.w("FHook", "[System.exit] blocked, code=" + code);
                })
                .commit();
    }

    // Hook: java.lang.Runtime.exec(String) → (Ljava/lang/String;)Ljava/lang/Process;
    private static void hook_Runtime_exec_1() throws Exception {
        Method m = Runtime.class.getDeclaredMethod("exec", String.class);
        FHook.hook(m)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    String cmd = (String) args.get(0);
                    FLog.i("FHook", "[Runtime.exec] cmd=" + cmd);
                    // 示例：拦截高危命令
                    // if (cmd != null && cmd.startsWith("su")) {
                    //     hh.setOrigFunRun(false); // 如果你的 HookHandle 支持动态改
                    // }
                })
                .setHookExit((ret, type, hh) -> {
                    // ret: java.lang.Process
                    return ret;
                })
                .commit();
    }

    // Hook: java.lang.System.loadLibrary(String) → (Ljava/lang/String;)V
    private static void hook_System_loadLibrary() throws Exception {
        Method m = System.class.getDeclaredMethod("loadLibrary", String.class);
        FHook.hook(m)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    String lib = (String) args.get(0);
                    FLog.i("FHook", "[System.loadLibrary] lib=" + lib);
                    // 示例：拦截特定 so
                    // if ("badlib".equals(lib)) { hh.setOrigFunRun(false); FLog.w("FHook","blocked badlib"); }
                })
                .commit();
    }

    // Hook: android.os.SystemProperties.get(String) → (Ljava/lang/String;)Ljava/lang/String;
    private static void hook_SystemProperties_get() throws Exception {
        Class<?> sp = Class.forName("android.os.SystemProperties");
        Method m = sp.getDeclaredMethod("get", String.class);
        m.setAccessible(true);

        FHook.hook(m)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    String key = (String) args.get(0);
                    FLog.d("FHook", "[SystemProperties.get] key=" + key);
                    hh.putExtra("key", key);
                })
                .setHookExit((ret, type, hh) -> {
                    String key = (String) hh.getExtra("key");
                    String val = (String) ret;

                    if ("ro.build.version.release".equals(key)) {
                        return "14"; // 例：伪装 Android 14
                    }
                    if ("ro.product.model".equals(key)) {
                        return "Pixel 9 Pro";
                    }
                    // 其它 key 保持原值
                    return val;
                })
                .commit();
    }


    // Hook: android.provider.Settings.Secure.getString(ContentResolver,String)
// → (Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;
    private static void hook_Settings_Secure_getString() throws Exception {
        Method m = android.provider.Settings.Secure.class
                .getDeclaredMethod("getString",
                        android.content.ContentResolver.class, String.class);

        FHook.hook(m)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    String key = (String) args.get(1);
                    if ("android_id".equals(key)) {
                        FLog.i("FHook", "[Settings.Secure.getString] android_id requested");
                    }
                    hh.putExtra("key", key);
                })
                .setHookExit((ret, type, hh) -> {

                    String key = (String) hh.getExtra("key");
                    if ("android_id".equals(key)) {
                        return "a1b2c3d4e5f6a7b8"; // 自定义伪造 ID
                    }
                    return ret;
                })
                .commit();
    }


}