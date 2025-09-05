package top.feadre.fhook;

import top.feadre.fhook.flibs.fsys.FLog;

public class THook {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "THook";

    // ----------------------------  initAppBt01 ---------------------------------

    public int fun_I_III(int a, int b, int c) {
        int ret = a + b + c;
        FLog.d(TAG, "原方法输出 fun_I_III ... a=" + a + ", b=" + b + ", c=" + c + " -> ret=" + ret);
        return ret;
    }

    public String fun_String_String2(String a, String b) {
        String ret = a + b;
        FLog.d(TAG, "原方法输出 fun_String_String2 ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public static int jtFun_I_II(int a, int b) {
        int ret = a + b;
        FLog.d(TAG, "原方法输出 jtFun_I_II ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }


    // ----------------------------  initAppBt02 ---------------------------------


    public void fun_V_V() {
        FLog.d(TAG, "原方法输出 fun_V_V ...");
    }

    public static void jtFun_V_V() {
        FLog.d(TAG, "原方法输出 jtFun_V_V ...");
    }

    // ----------------------------  initAppBt03 ---------------------------------

    public TObject fun_TObject_TObject(TObject obj) {
        FLog.d(TAG, "原方法输出 fun_TObject_TObject ... in=" + obj);
        if (obj != null) {
            obj.setAge(obj.getAge() + 1).setName(obj.getName() + "_m");
        }
        FLog.d(TAG, "原方法输出 fun_TObject_TObject ... out=" + obj);
        return obj;
    }

    public int fun_I_IArr(int[] arr) {
        int sum = 0;
        if (arr != null) for (int v : arr) sum += v;
        FLog.d(TAG, "原方法输出 fun_I_IArr ... len="
                + (arr == null ? 0 : arr.length) + " -> sum=" + sum);
        return sum;
    }


    public int[] fun_IArr_DArr(double[] arr) {
        int[] ret = new int[arr.length];
        if (arr != null)
            for (int i = 0; i < arr.length; i++) {
                ret[i] = (int) arr[i] * 2;
            }
        FLog.d(TAG, "原方法输出 fun_IArr_DArr ... len=" + (arr == null ? 0 : arr.length));
        return ret;
    }

    public TObject[] fun_TObjectArr_DArr(double[] arr) {
        int sum = 0;
        if (arr != null) for (double v : arr) sum += v;
        FLog.d(TAG, "fun_TObjectArr_IArr ... len=" + (arr == null ? 0 : arr.length) + " -> sum=" + sum);
        TObject[] tObjects = new TObject[2];
        tObjects[0] = new TObject("_new_" + sum, sum);
        return tObjects;
    }

    public double fun_double_DArr(double[] arr) {
        double sum = 0.0;
        for (int i = 0; i < arr.length; i++) {
            sum += arr[i];
        }
        if (arr != null) for (double v : arr) FLog.d(TAG, "fun_double_DArr ... v=" + v);
        return sum;
    }

    public static long[] jtFun_JArr_JArr(long[] arr) {
        int sum = 0;
        for (int i = 0; i < arr.length; i++) {
            sum+= arr[i];
        }
        if (arr != null) for (long v : arr) FLog.d(TAG, "jtFun_JArr_JArr ... v=" + v);
        arr[0] = sum;
        return arr;
    }
}
