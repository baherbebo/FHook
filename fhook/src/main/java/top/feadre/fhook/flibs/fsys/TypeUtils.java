package top.feadre.fhook.flibs.fsys;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Arrays;

public final class TypeUtils {
    private TypeUtils() {
    }

    /**
     * 把 Class<?> 转为 DEX/JNI 描述符。
     * 原始类型： V/Z/B/C/S/I/J/F/D
     * 数组类型： Class.getName() 自带 "[I" / "[Ljava.lang.String;"，仅把 '.' → '/'
     * 引用类型： Ljava/lang/String;
     */
    public static String toDescriptor(Class<?> c) {
        if (c.isPrimitive()) {
            if (c == void.class) return "V";
            if (c == boolean.class) return "Z";
            if (c == byte.class) return "B";
            if (c == char.class) return "C";
            if (c == short.class) return "S";
            if (c == int.class) return "I";
            if (c == long.class) return "J";
            if (c == float.class) return "F";
            if (c == double.class) return "D";
        }
        if (c.isArray()) return c.getName().replace('.', '/');
        return "L" + c.getName().replace('.', '/') + ";";
    }

    /**
     * 生成方法描述符，例如：(ILjava/lang/String;)[I
     */
    public static String methodDescriptor(Method m) {
        StringBuilder sb = new StringBuilder();
        sb.append('(');
        for (Class<?> p : m.getParameterTypes()) sb.append(toDescriptor(p));
        sb.append(')').append(toDescriptor(m.getReturnType()));
        return sb.toString();
    }

    /**
     * ART shorty：把类型折叠为单字符。
     * 原始：V/Z/B/C/S/I/J/F/D； 引用和数组均为 L
     */
    public static char shortyChar(Class<?> c) {
        if (c.isPrimitive()) {
            if (c == void.class) return 'V';
            if (c == boolean.class) return 'Z';
            if (c == byte.class) return 'B';
            if (c == char.class) return 'C';
            if (c == short.class) return 'S';
            if (c == int.class) return 'I';
            if (c == long.class) return 'J';
            if (c == float.class) return 'F';
            if (c == double.class) return 'D';
        }
        return 'L';
    }

    /**
     * 生成方法 shorty（首位是返回类型），例如：int foo(long, Object[]) → "IJL"
     */
    public static String shorty(Method m) {
        StringBuilder sb = new StringBuilder();
        sb.append(shortyChar(m.getReturnType()));
        for (Class<?> p : m.getParameterTypes()) sb.append(shortyChar(p));
        return sb.toString();
    }

    // ======== 装箱/拆箱映射 ========
    private static final Map<Class<?>, Class<?>> P2W = new HashMap<>();
    private static final Map<Class<?>, Class<?>> W2P = new HashMap<>();

    static {
        P2W.put(boolean.class, Boolean.class);
        P2W.put(byte.class, Byte.class);
        P2W.put(char.class, Character.class);
        P2W.put(short.class, Short.class);
        P2W.put(int.class, Integer.class);
        P2W.put(long.class, Long.class);
        P2W.put(float.class, Float.class);
        P2W.put(double.class, Double.class);
        for (Map.Entry<Class<?>, Class<?>> e : P2W.entrySet()) W2P.put(e.getValue(), e.getKey());
    }

    public static Class<?> wrap(Class<?> c) {
        return c.isPrimitive() ? P2W.getOrDefault(c, c) : c;
    }

    public static Class<?> unwrap(Class<?> c) {
        return W2P.getOrDefault(c, c);
    }

    // ======== 安全判断/强转/设置 ========

    /**
     * 参数 arg 是否可赋给 paramType（考虑装箱/拆箱；null 允许赋给非原始）。
     * 原始：arg 不能为 null，且必须是对应包装类实例（如 Integer 对应 int.class）。
     * 引用/数组：arg 为 null 也可；否则需 paramType.isInstance(arg)。
     */
    public static boolean isArgAssignable(Class<?> paramType, Object arg) {
        if (paramType.isPrimitive()) {
            if (arg == null) return false;
            return wrap(paramType).isInstance(arg);
        } else {
            return arg == null || paramType.isInstance(arg);
        }
    }

    /**
     * 返回值是否可赋给 returnType（void 仅接受 null）
     */
    public static boolean isRetAssignable(Class<?> returnType, Object ret) {
        if (returnType == void.class) return ret == null;
        if (returnType.isPrimitive()) {
            if (ret == null) return false;
            return wrap(returnType).isInstance(ret);
        } else {
            return ret == null || returnType.isInstance(ret);
        }
    }

    /**
     * 返回值是否可赋给 returnType。
     * void：仅接受 null。
     * 原始：ret 不能为 null，且必须是对应包装类实例。
     * 引用/数组：null 或 isInstance。
     */
    @SuppressWarnings("unchecked")
    public static <T> T arg(Object[] args, int i, Class<T> want) {
        Object v = args[i];
        Class<?> need = wrap(want);
        if (v == null) {
            if (want.isPrimitive())
                throw new ClassCastException("null to primitive " + want);
            return null;
        }
        if (!need.isInstance(v)) {
            throw new ClassCastException("arg[" + i + "] type=" + v.getClass().getName()
                    + " !instanceof " + need.getName());
        }
        return (T) v; // 注意：如果 want 是原始类型，这里返回的是其包装类型对象
    }

    /**
     * 安全设置第 i 个参数：
     * - 按 paramType（通常来自 m.getParameterTypes()[i]）校验兼容性；
     * - 允许 null 赋给非原始类型；数值族需你自行先做转换或使用 autoCast。
     */
    public static void set(Object[] args, int i, Object newVal, Class<?> paramType) {
        if (!isArgAssignable(paramType, newVal)) {
            throw new ClassCastException("set arg[" + i + "] with " +
                    (newVal == null ? "null" : newVal.getClass().getName()) +
                    " not assignable to " + paramType.getName());
        }
        args[i] = newVal;
    }

    // ======== 仅用于调试日志 ========

    /**
     * 打印参数简表：desc/shorty/每个参数的预期类型、实际类型、是否可赋、直观值
     */
    public static String dumpArgs(Method m, Object[] args) {
        Class<?>[] ps = m.getParameterTypes();
        StringBuilder sb = new StringBuilder();
        sb.append("desc=").append(methodDescriptor(m))
                .append(", shorty=").append(shorty(m))
                .append(", params=[");
        for (int i = 0; i < ps.length; i++) {
            if (i > 0) sb.append("; ");
            Class<?> p = ps[i];
            Object a = (args != null && i < args.length) ? args[i] : null;
            sb.append('#').append(i).append('{')
                    .append("expect=").append(p.getName())
                    .append(", shorty=").append(shortyChar(p))
                    .append(", got=").append(a == null ? "null" : a.getClass().getName())
                    .append(", ok=").append(isArgAssignable(p, a))
                    .append(", val=").append(stringifyArg(a))
                    .append('}');
        }
        sb.append(']');
        return sb.toString();
    }

    private static String stringifyArg(Object a) {
        if (a == null) return "null";
        Class<?> c = a.getClass();
        if (c.isArray()) {
            if (a instanceof int[]) return Arrays.toString((int[]) a);
            if (a instanceof long[]) return Arrays.toString((long[]) a);
            if (a instanceof byte[]) return Arrays.toString((byte[]) a);
            if (a instanceof short[]) return Arrays.toString((short[]) a);
            if (a instanceof char[]) return Arrays.toString((char[]) a);
            if (a instanceof boolean[]) return Arrays.toString((boolean[]) a);
            if (a instanceof float[]) return Arrays.toString((float[]) a);
            if (a instanceof double[]) return Arrays.toString((double[]) a);
            if (a instanceof Object[]) return Arrays.toString((Object[]) a);
        }
        return String.valueOf(a);
    }

    /**
     * 强转第 i 个参数为 want 类型：
     * - 若 want 是原始类型，按包装类校验并返回包装对象（如 int.class → Integer）。
     * - 不做数值族之间的转换（不跨包装类），如需自动数值转换用 autoCast/argAuto。
     */
    @SuppressWarnings("unchecked")
    public static <T> T autoCast(Object v, Class<T> targetType) {
        if (targetType == void.class) return null; // 无意义但容忍

        Class<?> want = targetType.isPrimitive() ? wrap(targetType) : targetType;

        if (v == null) {
            if (targetType.isPrimitive())
                throw new ClassCastException("null to primitive " + targetType);
            return null;
        }
        if (want.isInstance(v)) return (T) v;

        // 数值类型之间的转换（例如 Long -> Integer）
        if (Number.class.isAssignableFrom(want) && v instanceof Number) {
            Number n = (Number) v;
            if (want == Byte.class) return (T) Byte.valueOf(n.byteValue());
            if (want == Short.class) return (T) Short.valueOf(n.shortValue());
            if (want == Integer.class) return (T) Integer.valueOf(n.intValue());
            if (want == Long.class) return (T) Long.valueOf(n.longValue());
            if (want == Float.class) return (T) Float.valueOf(n.floatValue());
            if (want == Double.class) return (T) Double.valueOf(n.doubleValue());
        }

        // char <-> 数值（常见场景：int 形参接收 Character/Number）
        if (want == Character.class && v instanceof Number) {
            return (T) Character.valueOf((char) ((Number) v).intValue());
        }
        if (want == Boolean.class && v instanceof Boolean) {
            return (T) v;
        }

        // 引用/数组按正常 Java 规则
        return (T) want.cast(v);
    }

    /**
     * 基于 paramTypes[index] 自动强转 args[index]
     */
    @SuppressWarnings("unchecked")
    public static <T> T argAuto(Object[] args, int index, Class<?>[] paramTypes) {
        return (T) autoCast(args[index], paramTypes[index]);
    }
}
