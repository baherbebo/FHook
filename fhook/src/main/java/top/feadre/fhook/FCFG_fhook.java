package top.feadre.fhook;

public class FCFG_fhook {
    public final static String TAG_PREFIX = "feadre_";
    public static boolean IS_DEBUG = false; // 框架 log开启

    /*
    已知框架不支持的函数: 详见 isBridgeCritical
        Thread.currentThread()
        thread.getContextClassLoader()
        lass.getDeclaredMethod(String, Class[])

     Class.forName  和 classLoader.loadClass 不能同时hook,未限制
     */

    // 如果某些系统函数无法 hook 可尝试, 请设置为 true ，但仍有失败的风险,可以建议将函数反馈给作者
    public static boolean IS_SAFE_MODE = false; // 安全模式|黑名单 提升 hook 系统函数的稳定性
}
