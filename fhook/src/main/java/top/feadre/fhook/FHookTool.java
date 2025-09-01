package top.feadre.fhook;

import static top.feadre.fhook.FCFG.TAG_PREFIX;

import java.lang.reflect.Method;
import java.util.ArrayList;

import top.feadre.fhook.flibs.fsys.FLog;

public class FHookTool {
    private static final String TAG = TAG_PREFIX + "FHookTool";

    public static ArrayList<Method> findMethodAll(Class<?> cls, String methodName) {
        Method[] methods = cls.getDeclaredMethods(); // 拿到类的 所有方法
        ArrayList<Method> res_methods = new ArrayList<>();

        for (Method method : methods) {
            if (!method.getName().equals(methodName)) {
                continue; // 只 hook 同名方法，避免全部乱 hook
            }

            method.setAccessible(true); // 确保私有方法也能访问
            String methodSignature = method.toGenericString();
            FLog.d(TAG, "[findMethodAll] 找到方法: " + methodSignature);
            res_methods.add(method);
        }

        return res_methods;
    }

    public static Method findMethod4Index(Class<?> cls, String methodName, int index) {
        FLog.d(TAG, "[findMethod4Index] Class: " + cls.getName() + " methodName: " + methodName);
        ArrayList<Method> methodAll = findMethodAll(cls, methodName);
        if (methodAll.size() > index) {
            Method method = methodAll.get(index);

            method.setAccessible(true); // 确保私有方法也能访问
            String methodSignature = method.toGenericString();
            FLog.w(TAG, "[findMethod4Index] 找到 method: " + methodSignature);
            return method;
        } else {
            FLog.e(TAG, "[findMethod4Index] 未找到 method: " + methodName + " " + methodAll.size() + " index= " + index);
            return null;
        }
    }

    public static Method findMethod4First(Class<?> cls, String methodName) {
        FLog.d(TAG, "[findMethod4First] Class: " + cls.getName() + " methodName: " + methodName);
        return findMethod4Index(cls, methodName, 0);
    }
}
