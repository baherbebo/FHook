package top.feadre.fhook;

public class FCFG_fhook {
    public final static String TAG_PREFIX = "feadre_";
    public final static boolean IS_DEBUG = true; // 可以通过隐藏功能切换 对应 android 应用调试 log开启

    /*
        Class.forName
        Method.invoke
        Thread.currentThread
        thread.getContextClassLoader()
        contextClassLoader.loadClass
        class.getDeclaredMethod
     */
    public static final boolean ENABLE_HOOK_CRUX = false; // 开启 hook 框架关键函数
}
