package top.feadre.fhook;

import static top.feadre.fhook.FCFG_fhook.TAG_PREFIX;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;

import top.feadre.fhook.flibs.fsys.FLog;

public class FHookTool {
    private static final String TAG = TAG_PREFIX + "FHookTool";

    /// 判断这个类是否可被重写（与 canHook(Method) 配套）—  canHook 可扩展方法
    public static boolean isHookableClass(Class<?> c) {
        if (c == null) return false;
        if (c.isInterface() || c.isAnnotation()) return false;
        int m = c.getModifiers();
        // 抽象类不能直接重写其方法体（需要对具体子类 hook）
        return !Modifier.isAbstract(m);
    }

    // 查全部同名方法：可选是否包含父类、过滤桥接/合成；并按稳定 key 排序
    public static java.util.List<Method> findMethodAll(
            Class<?> cls, String methodName,
            boolean includeInherited, boolean excludeBridge, boolean excludeSynthetic) {

        if (cls == null || methodName == null) return java.util.Collections.emptyList();
        java.util.List<Method> out = new java.util.ArrayList<>();

        Class<?> it = cls;
        while (it != null) {
            for (Method m : it.getDeclaredMethods()) {
                if (!m.getName().equals(methodName)) continue;
                if (excludeBridge && m.isBridge()) continue;
                if (excludeSynthetic && m.isSynthetic()) continue;
                // 只在需要调用时再 setAccessible，避免全量设置
                out.add(m);
            }
            if (!includeInherited) break;
            it = it.getSuperclass();
        }

        out.sort((a, b) -> methodStableKey(a).compareTo(methodStableKey(b)));
        return java.util.Collections.unmodifiableList(out);
    }

    public static ArrayList<Method> findMethodAll(Class<?> cls, String methodName) {
        java.util.List<Method> list = findMethodAll(cls, methodName, false, false, false);
        return new ArrayList<>(list);
    }

    // 7) findMethod4Index：先排序，再取 index；并提示“请改用 findMethodExact”
    public static Method findMethod4Index(Class<?> cls, String methodName, int index) {
        if (cls == null) return null;
        FLog.d(TAG, "[findMethod4Index] Class: " + cls.getName() + " methodName: " + methodName);
        java.util.List<Method> all = findMethodAll(cls, methodName, false, false, false);
        if (index >= 0 && index < all.size()) {
            Method method = all.get(index);
            if (!method.isAccessible()) method.setAccessible(true);
            FLog.w(TAG, "[findMethod4Index] 命中: " + method.toGenericString()
                    + "（注意：index 可能不稳定，推荐 findMethodExact）");
            return method;
        }
        FLog.e(TAG, "[findMethod4Index] 未找到 method: " + methodName + " size=" + all.size() + " index=" + index);
        return null;
    }

    public static Method findMethod4First(Class<?> cls, String methodName) {
        FLog.d(TAG, "[findMethod4First] Class: " + cls.getName() + " methodName: " + methodName);
        return findMethod4Index(cls, methodName, 0);
    }


    /**
     * 若传入的是接口方法，基于实例解析真实实现；否则直接返回原 method。
     * 解析失败返回 null（已打日志）。
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


    public static void showMethodInfo(Method method) {
        String methodNameStr = method.getName(); // 方法名称
        Class<?> returnType = method.getReturnType();// 返回类型
        // Method modifiers: public static
        FLog.d(TAG, "[showMethod] Method modifiers: " + Modifier.toString(method.getModifiers()));

        // 参数类型 int, int
        Class<?>[] paramTypes = method.getParameterTypes(); // 参数类型
        StringBuilder paramTypesStr = new StringBuilder();
        for (int i = 0; i < paramTypes.length; i++) {
            if (i > 0) paramTypesStr.append(", ");
            paramTypesStr.append(paramTypes[i].getName());
        }
        FLog.d(TAG, "[showMethod] Parameter types: " + paramTypesStr.toString());

        // 没有错误类型
        Class<?>[] exceptionTypes = method.getExceptionTypes();
        if (exceptionTypes.length > 0) {
            StringBuilder exceptionTypesStr = new StringBuilder();
            for (int i = 0; i < exceptionTypes.length; i++) {
                if (i > 0) exceptionTypesStr.append(", ");
                exceptionTypesStr.append(exceptionTypes[i].getName());
            }
            FLog.d(TAG, "[showMethod] Exception types: " + exceptionTypesStr.toString());
        }

        // 完整签名 public static int top.feadre.finject.FInjectHelpUI.add(int,int)
        String methodSignature = method.toGenericString();
        FLog.d(TAG, "[showMethod] 全签名: " + methodSignature);

        // 简单的签名组装 int add(int, int)
        String simpleSignature = returnType.getSimpleName() + " " + methodNameStr + "(" + paramTypesStr + ")";
        FLog.d(TAG, "[showMethod] 简单签名: " + simpleSignature);
    }

    public static String getSimpleSignature(Method method) {
        String methodNameStr = method.getName(); // 方法名称

        Class<?>[] paramTypes = method.getParameterTypes(); // 参数类型
        StringBuilder paramTypesStr = new StringBuilder();
        for (int i = 0; i < paramTypes.length; i++) {
            if (i > 0) paramTypesStr.append(", ");
            paramTypesStr.append(paramTypes[i].getName());
        }

        Class<?> returnType = method.getReturnType();// 返回类型

        String simpleSignature = returnType.getSimpleName() + " " + methodNameStr + "(" + paramTypesStr + ")";

        return simpleSignature;
    }


    public static void printStackTrace() {
        printStackTrace("", 15);
    }

    public static void printStackTrace(int maxDepth) {
        printStackTrace("", maxDepth);
    }

    public static void printStackTrace(String tag, int maxDepth) {
        StackTraceElement[] stack = Thread.currentThread().getStackTrace();
        // 0:getStackTrace,1:printStackTrace,2:重载/调用者，通常从3开始可避开本工具层
        int start = Math.min(3, stack.length);
        int end = (maxDepth > 0) ? Math.min(stack.length, start + maxDepth) : stack.length;

        FLog.d(TAG, "===== " + tag + " 方法调用栈开始 =====");
        for (int i = start; i < end; i++) {
            StackTraceElement e = stack[i];
            FLog.d(TAG, String.format("    #%d %s.%s(%s:%d)",
                    (i - start + 1), e.getClassName(), e.getMethodName(), e.getFileName(), e.getLineNumber()));
        }
        FLog.d(TAG, "===== 方法调用栈结束 =====");
    }

    public static void printStackTrace(Throwable t, String tag, int maxDepth) {
        if (t == null) {
            printStackTrace(tag, maxDepth);
            return;
        }
        StackTraceElement[] stack = t.getStackTrace();
        int end = (maxDepth > 0) ? Math.min(stack.length, maxDepth) : stack.length;
        FLog.d(TAG, "===== " + tag + " 异常栈开始: " + t + " =====");
        for (int i = 0; i < end; i++) {
            StackTraceElement e = stack[i];
            FLog.d(TAG, String.format("    #%d %s.%s(%s:%d)",
                    (i + 1), e.getClassName(), e.getMethodName(), e.getFileName(), e.getLineNumber()));
        }
        FLog.d(TAG, "===== 异常栈结束 =====");
    }



    /**
     * 逐行打印：方法签名、this、每个参数（期望类型=值 | 实际类型）
     */
    /** 逐行打印：方法签名、this、每个参数（期望类型=值 | 实际类型） */
    public static void showOnEnterArgs(String tag, String name_fun,
                                       Method m, Object[] rawArgs) {
        final boolean isStatic = Modifier.isStatic(m.getModifiers());
        final Class<?>[] ps = m.getParameterTypes();
        final int len = rawArgs == null ? 0 : rawArgs.length;

        FLog.i(tag, "[" + name_fun + "] 开始 "
                + m.getDeclaringClass().getName() + "." + m.getName()
                + " (" + ps.length + " params, static=" + isStatic + "), rawLen=" + len
                + ", desc=" + jvmMethodDesc(m));

        Object thisObj = (len > 0) ? rawArgs[0] : null;
        FLog.i(tag, "   --- this = " + (isStatic ? "<static>" : pretty(thisObj))
                + "  | got=" + gotType(thisObj));

        for (int i = 0; i < ps.length; i++) {
            Object a = (len > 1 + i) ? rawArgs[1 + i] : null;
            FLog.i(tag, "   --- #" + i + "  " + ps[i].getName() + " = " + pretty(a)
                    + "  | got=" + gotType(a));
        }
    }

    /** 单行精简：类型=值（便于拼到一条日志） */
    public static String showOnEnterArgs4line(Method m, Object[] rawArgs) {
        Class<?>[] ps = m.getParameterTypes();
        int len = rawArgs == null ? 0 : rawArgs.length;
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < ps.length; i++) {
            if (i > 0) sb.append(",  ");
            Object a = (len > 1 + i) ? rawArgs[1 + i] : null;
            sb.append(ps[i].getSimpleName()).append('=').append(' ').append(pretty(a));
        }
        return sb.toString();
    }

    /* -------------------- 内部辅助 -------------------- */

    /** 稳定排序 key：方法名 + 参数类型 + 返回类型 */
    private static String methodStableKey(Method m) {
        StringBuilder sb = new StringBuilder();
        sb.append(m.getName()).append('#');
        Class<?>[] ps = m.getParameterTypes();
        for (Class<?> p : ps) sb.append(p.getName()).append(',');
        sb.append("->").append(m.getReturnType().getName());
        return sb.toString();
    }

    /** JVM 方法描述符（适合生成/比对 methodId） */
    public static String jvmMethodDesc(Method m) {
        StringBuilder sb = new StringBuilder();
        sb.append('(');
        for (Class<?> p : m.getParameterTypes()) sb.append(jvmTypeDesc(p));
        sb.append(')').append(jvmTypeDesc(m.getReturnType()));
        return sb.toString();
    }

    private static String jvmTypeDesc(Class<?> c) {
        if (c.isPrimitive()) {
            if (c == void.class) return "V";
            if (c == int.class) return "I";
            if (c == boolean.class) return "Z";
            if (c == byte.class) return "B";
            if (c == short.class) return "S";
            if (c == long.class) return "J";
            if (c == float.class) return "F";
            if (c == double.class) return "D";
            if (c == char.class) return "C";
        }
        if (c.isArray()) return c.getName().replace('.', '/');
        return "L" + c.getName().replace('.', '/') + ";";
    }

    private static String pretty(Object a) {
        if (a == null) return "null";
        if (a instanceof String) {
            String s = (String) a;
            return '"' + (s.length() > 120 ? s.substring(0, 120) + "…" : s) + '"';
        }
        if (a instanceof CharSequence || a instanceof Number || a instanceof Boolean) {
            return String.valueOf(a);
        }
        if (a instanceof java.util.Collection) {
            int n = 8, i = 0;
            StringBuilder sb = new StringBuilder("[");
            for (Object e : (java.util.Collection<?>) a) {
                if (i++ > 0) sb.append(", ");
                if (i > n) { sb.append("…").append(((java.util.Collection<?>) a).size() - n).append(" more"); break; }
                sb.append(pretty(e));
            }
            return sb.append(']').toString();
        }
        if (a instanceof java.util.Map) {
            int n = 8, i = 0;
            StringBuilder sb = new StringBuilder("{");
            for (java.util.Map.Entry<?, ?> e : ((java.util.Map<?, ?>) a).entrySet()) {
                if (i++ > 0) sb.append(", ");
                if (i > n) { sb.append("…").append(((java.util.Map<?, ?>) a).size() - n).append(" more"); break; }
                sb.append(pretty(e.getKey())).append('=').append(pretty(e.getValue()));
            }
            return sb.append('}').toString();
        }
        Class<?> c = a.getClass();
        if (c.isArray()) {
            if (a instanceof Object[]) return java.util.Arrays.deepToString((Object[]) a);
            int len = java.lang.reflect.Array.getLength(a), n = Math.min(len, 8);
            StringBuilder sb = new StringBuilder("[");
            for (int i = 0; i < n; i++) {
                if (i > 0) sb.append(", ");
                sb.append(pretty(java.lang.reflect.Array.get(a, i)));
            }
            if (len > n) sb.append(", …").append(len - n).append(" more");
            return sb.append(']').toString();
        }
        return c.getName() + '@' + Integer.toHexString(System.identityHashCode(a));
    }

    private static String gotType(Object a) {
        return a == null ? "null" : a.getClass().getName();
    }


}
