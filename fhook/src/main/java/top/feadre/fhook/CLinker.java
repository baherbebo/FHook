package top.feadre.fhook;

import java.lang.reflect.Executable;

public class CLinker {
    protected static native boolean dcEnableJdwp(boolean enable); // 常规初始 初始化入口

    protected static native boolean jcJvmtiSuccess(String nameSoFhookAgent,
                                                   boolean is_safe_mode,
                                                   boolean is_debuggable); // 后处理

    protected static native int dcSetRuntimeJavaDebuggable(int debuggable); // 尝试增强初始

    protected static native long dcHook(Executable method, boolean isHEnter, boolean isHExit,
                                        boolean isRunOrigFun);


    protected static native boolean dcIsJdwpAllowed();

    protected static native boolean dcUnhook(long methodId);


    ///  查找接口 或 抽象类 的实现类
    public static native Class<?>[] jcJvmtiFindImpl(Class<?> ifaceOrAbstract);

    ///  查找类实例
    public static native Object[] jcJvmtiFindInstances(Class<?> klass, boolean onlyLive);


}
