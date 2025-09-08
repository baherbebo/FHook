package top.feadre.fhook;

public class FCFG_fhook {
    public final static String TAG_PREFIX = "feadre_";
    public static boolean IS_DEBUG = true; // 可以通过隐藏功能切换 对应 android 应用调试 log开启

    /*
    FHook.hook(ClassLoader.class, "loadClass")
     */
    public static boolean IS_SAFE_MODE = false; // 安全模式|黑名单
}
