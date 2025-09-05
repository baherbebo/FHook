package top.feadre.fhook;

import static top.feadre.fhook.FCFG.TAG_PREFIX;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
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


    /**
     * 查找实例的方法 对像
     * 若 method 来自接口/注解接口，则尝试用 instance 的真实类解析实现方法。
     * 解析成功返回实现类 Method；失败返回 null（内部已打日志）。
     * 对非接口方法，原样返回 method。
     */
    public static Method findMethod4Impl(Object instance, Method method) {
        if (method == null) return null;

        final Class<?> decl = method.getDeclaringClass();

        // 非接口/注解接口：直接返回
        if (!decl.isInterface() && !decl.isAnnotation()) {
            return method;
        }

        // 接口方法但没给实例，无法解析实现
        if (instance == null) {
            FLog.e(TAG, "[findMethod4Impl] instance == null for interface method: "
                    + decl.getName() + "#" + method.getName());
            return null;
        }

        final Class<?> impl = instance.getClass();

        // 1) 先按完全相同签名查 public 方法
        try {
            Method m = impl.getMethod(method.getName(), method.getParameterTypes());
            if (!m.getDeclaringClass().isInterface()) {
                return m;
            }
            // 极少数 ROM 可能把桥接方法挂在接口上，继续兜底
        } catch (NoSuchMethodException ignore) {
            // 走兜底
        }

        // 2) 兜底：遍历所有 public 方法按“名称 + 参数完全一致”匹配
        //    （如果你想更激进，也可以允许 isAssignableFrom 做“兼容匹配”）
        for (Method m : impl.getMethods()) {
            if (m.getName().equals(method.getName())) {
                Class<?>[] a = m.getParameterTypes();
                Class<?>[] b = method.getParameterTypes();
                if (a.length == b.length) {
                    boolean same = true;
                    for (int i = 0; i < a.length; i++) {
                        if (a[i] != b[i]) {
                            same = false;
                            break;
                        }
                    }
                    if (same && !m.getDeclaringClass().isInterface()) {
                        return m;
                    }
                }
            }
        }

        // 失败：打日志并返回 null
        FLog.e(TAG, "[findMethod4Impl] 无法解析实现类方法："
                + decl.getName() + "#" + method.getName()
                + " on impl=" + impl.getName());
        return null;
    }


    /// 判断这个类是否可被重写（与 canHook(Method) 配套）—
    public static boolean isHookableClass(Class<?> c) {
        if (c == null) return false;
        if (c.isInterface() || c.isAnnotation()) return false;
        int m = c.getModifiers();
        // 抽象类不能直接重写其方法体（需要对具体子类 hook）
        return !Modifier.isAbstract(m);
    }

}
