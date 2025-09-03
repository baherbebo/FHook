package top.feadre.fhook;

import java.lang.reflect.Method;

public class CLinker {
    public static native long dcHook(Method method, boolean isHEnter, boolean isHExit,
                                     boolean isRunOrigFun);

    public static native int dcSetRuntimeJavaDebuggable(int debuggable);

    public static native boolean dcEnableJdwp(boolean enable);

    public static native boolean dcIsJdwpAllowed();

    public static native boolean dcJvmtiSuccess(String nameSoFhookAgent);

    public static native boolean dcUnhook(long methodId);
}
