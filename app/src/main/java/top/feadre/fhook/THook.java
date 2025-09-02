package top.feadre.fhook;

import top.feadre.fhook.flibs.fsys.FLog;

public class THook {
    private static final String TAG = FCFG.TAG_PREFIX + "THook";

    // ========= 实例：无参/单参/多参/重载 =========
    public void fun_V_V() {
        FLog.d(TAG, "fun_V_V ...");
    }

    public void fun_V_I(int a) {
        FLog.d(TAG, "fun_V_I ... a=" + a);
    }

    public int fun_I_I(int a) {
        int ret = a + 1;
        FLog.d(TAG, "fun_I_I ... a=" + a + " -> ret=" + ret);
        return ret;
    }

    public int fun_I_II(int a, int b) {
        int ret = a + b;
        FLog.d(TAG, "fun_I_II ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public int fun_I_III(int a, int b, int c) {
        int ret = a + b + c;
        FLog.d(TAG, "fun_I_III ... a=" + a + ", b=" + b + ", c=" + c + " -> ret=" + ret);
        return ret;
    }

    // ========= 实例：系统类 / 字符串 =========
    public String fun_String_String2(String a,String b) {
        String ret = a + b;
        FLog.d(TAG, "fun_String_String2 ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public String fun_String_StringArray(String[] arr) {
        StringBuilder sb = new StringBuilder();
        if (arr != null) for (int i = 0; i < arr.length; i++) {
            if (i > 0) sb.append(",");
            sb.append(arr[i]);
        }
        String ret = sb.toString();
        FLog.d(TAG, "fun_String_StringArray ... len=" + (arr == null ? 0 : arr.length) + " -> ret=" + ret);
        return ret;
    }

    // ========= 实例：引用类型（自定义类） =========
    public TObject fun_TObject_T(TObject obj) {
        FLog.d(TAG, "fun_TObject_T ... in=" + obj);
        if (obj != null) {
            obj.setAge(obj.getAge() + 1).setName(obj.getName() + "_m");
        }
        FLog.d(TAG, "fun_TObject_T ... out=" + obj);
        return obj;
    }

    public void fun_V_TObjectArray(TObject[] list) {
        FLog.d(TAG, "fun_V_TObjectArray ... len=" + (list == null ? 0 : list.length));
    }

    // ========= 实例：数组 / varargs =========
    public int fun_I_IArray(int[] arr) {
        int sum = 0;
        if (arr != null) for (int v : arr) sum += v;
        FLog.d(TAG, "fun_I_IArray ... len=" + (arr == null ? 0 : arr.length) + " -> sum=" + sum);
        return sum;
    }

    public int fun_I_Varargs(int... xs) {
        int sum = 0;
        if (xs != null) for (int v : xs) sum += v;
        FLog.d(TAG, "fun_I_Varargs ... len=" + (xs == null ? 0 : xs.length) + " -> sum=" + sum);
        return sum;
    }

    // ========= 实例：宽类型（long/double） =========
    public long fun_J_J(long a) {
        long ret = a + 10L;
        FLog.d(TAG, "fun_J_J ... a=" + a + " -> ret=" + ret);
        return ret;
    }

    public double fun_D_D(double a) {
        double ret = a * 1.5;
        FLog.d(TAG, "fun_D_D ... a=" + a + " -> ret=" + ret);
        return ret;
    }

    // 宽类型与窄类型交错（寄存器/栈位对齐验证）
    public int fun_I_ILJ(int x, long y, int z) {
        int ret = x + (int) (y & 0xffff) + z;
        FLog.d(TAG, "fun_I_ILJ ... x=" + x + ", y=" + y + ", z=" + z + " -> ret=" + ret);
        return ret;
    }

    public void fun_V_LI(long a, int b) {
        FLog.d(TAG, "fun_V_LI ... a=" + a + ", b=" + b);
    }

    public void fun_V_IL(int a, long b) {
        FLog.d(TAG, "fun_V_IL ... a=" + a + ", b=" + b);
    }

    public long fun_J_DIJ(double d, int i, long j) {
        long ret = (long) Math.round(d) + i + j;
        FLog.d(TAG, "fun_J_DIJ ... d=" + d + ", i=" + i + ", j=" + j + " -> ret=" + ret);
        return ret;
    }

    public double fun_D_IDI(int a, double b, int c) {
        double ret = a + b + c;
        FLog.d(TAG, "fun_D_IDI ... a=" + a + ", b=" + b + ", c=" + c + " -> ret=" + ret);
        return ret;
    }

    // ========= 实例：其它基本类型 =========
    public boolean fun_Z_Z(boolean b) {
        boolean ret = !b;
        FLog.d(TAG, "fun_Z_Z ... b=" + b + " -> ret=" + ret);
        return ret;
    }

    public char fun_C_C(char c) {
        char ret = Character.toUpperCase(c);
        FLog.d(TAG, "fun_C_C ... c=" + c + " -> ret=" + ret);
        return ret;
    }

    public short fun_S_S(short s) {
        short ret = (short) (s + 1);
        FLog.d(TAG, "fun_S_S ... s=" + s + " -> ret=" + ret);
        return ret;
    }

    public byte fun_B_B(byte b) {
        byte ret = (byte) (b + 1);
        FLog.d(TAG, "fun_B_B ... b=" + b + " -> ret=" + ret);
        return ret;
    }

    public float fun_F_F(float f) {
        float ret = f * 2f;
        FLog.d(TAG, "fun_F_F ... f=" + f + " -> ret=" + ret);
        return ret;
    }

    // ========= 实例：同步、抛异常 =========
    public synchronized void fun_V_Sync(int a) {
        FLog.d(TAG, "fun_V_Sync ... a=" + a + ", thread=" + Thread.currentThread().getName());
    }

    public void fun_V_Throw() {
        FLog.d(TAG, "fun_V_Throw ... about to throw");
        throw new IllegalStateException("fun_V_Throw boom");
    }

    // ========= 静态：对应一组静态方法 =========
    public static void jc_fun_V_V() {
        FLog.d(TAG, "jc_fun_V_V ...");
    }

    public static void jc_fun_V_I(int a) {
        FLog.d(TAG, "jc_fun_V_I ... a=" + a);
    }

    public static void jtFun_V_II(int a, int b) {
        FLog.d(TAG, "jc_fun_V_II ... a=" + a + ", b=" + b);
    }

    public static int jtFun_I_II(int a, int b) {
        int ret = a + b;
        FLog.d(TAG, "jc_I_II ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public static long jc_J_JJ(long a, long b) {
        long ret = a ^ b;
        FLog.d(TAG, "jc_J_JJ ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public static double jc_D_DD(double a, double b) {
        double ret = a + b;
        FLog.d(TAG, "jc_D_DD ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public static String jc_String_String(String s) {
        String ret = "[" + s + "]";
        FLog.d(TAG, "jc_String_String ... s=" + s + " -> ret=" + ret);
        return ret;
    }

    public static TObject jc_TObject_T(TObject t) {
        FLog.d(TAG, "jc_TObject_T ... in=" + t);
        if (t != null) {
            t.setAge(t.getAge() + 10);
        }
        TObject.incStaticCount();
        FLog.d(TAG, "jc_TObject_T ... out=" + t + ", staticCount=" + TObject.getStaticCount());
        return t;
    }

    public static int jc_I_IntArray(int[] arr) {
        int sum = 0;
        if (arr != null) for (int v : arr) sum += v;
        FLog.d(TAG, "jc_I_IntArray ... len=" + (arr == null ? 0 : arr.length) + " -> sum=" + sum);
        return sum;
    }

    public static int jc_I_Varargs(int... xs) {
        int sum = 0;
        if (xs != null) for (int v : xs) sum += v;
        FLog.d(TAG, "jc_I_Varargs ... len=" + (xs == null ? 0 : xs.length) + " -> sum=" + sum);
        return sum;
    }

    public static double jc_D_DID(double a, int b, double c) {
        double ret = a + b + c;
        FLog.d(TAG, "jc_D_DID ... a=" + a + ", b=" + b + ", c=" + c + " -> ret=" + ret);
        return ret;
    }
}
