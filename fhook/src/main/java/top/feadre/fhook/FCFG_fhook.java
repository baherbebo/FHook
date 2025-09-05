package top.feadre.fhook;

public class FCFG_fhook {
    public final static String TAG_PREFIX = "feadre_";
    public final static boolean IS_DEBUG = true; // 可以通过隐藏功能切换 对应 android 应用调试 log开启

    /*
        Thread thread = Thread.currentThread();
        ClassLoader contextClassLoader = thread.getContextClassLoader();
        Class<?> clazz = contextClassLoader.loadClass("top.feadre.fhook.FHookTool");
        method = clazz.getDeclaredMethod("printStackTrace", int.class);
        method.invoke(null, 5); // 这个是native
     */
    public static final boolean ENABLE_HOOK_CRUX = true; // 开启 hook 框架关键函数
}
