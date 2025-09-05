package top.feadre.fhook;

import java.lang.reflect.Method;

public class CLinker {
    protected static native long dcHook(Method method, boolean isHEnter, boolean isHExit,
                                     boolean isRunOrigFun);

    protected static native int dcSetRuntimeJavaDebuggable(int debuggable);

    protected static native boolean dcEnableJdwp(boolean enable);

    protected static native boolean dcIsJdwpAllowed();

    protected static native boolean dcUnhook(long methodId);

    protected static native boolean jcJvmtiSuccess(String nameSoFhookAgent);

    ///  查找接口 或 抽象类 的实现类
    public static native Class<?>[] jcJvmtiFindImpl(Class<?> ifaceOrAbstract);

    ///  查找类实例
    public static native Object[] jcJvmtiFindInstances(Class<?> klass, boolean onlyLive);


}
