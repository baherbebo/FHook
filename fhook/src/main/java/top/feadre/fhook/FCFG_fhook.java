package top.feadre.fhook;

public class FCFG_fhook {
    public final static String TAG_PREFIX = "feadre_";
    public static boolean IS_DEBUG = true; // 框架 log开启

    // 如果某些系统函数无法 hook ，请设置为 true，但有失败的风险
    public static boolean IS_SAFE_MODE = true; // 安全模式|黑名单 提升 hook 系统函数的稳定性
}
