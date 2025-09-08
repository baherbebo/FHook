package top.feadre.fhook.activitys;

import android.content.ContentResolver;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.widget.Button;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import top.feadre.fhook.CLinker;
import top.feadre.fhook.FCFG_fhook;
import top.feadre.fhook.FHook;
import top.feadre.fhook.FHookTool;
import top.feadre.fhook.R;
import top.feadre.fhook.THook;
import top.feadre.fhook.TObject;
import top.feadre.fhook.flibs.fsys.FLog;
import top.feadre.fhook.tools.FFunsUI;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "MainActivity";


    static {
        System.loadLibrary("ft001");
    }

    public native int jcFt001T01(int a, int b);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        EdgeToEdge.enable(this); // 沉浸式
        setContentView(R.layout.activity_main);

        Log.i("Ft001", "jcFt001T01: ----------------" + jcFt001T01(1, 2));
        Method method;
        try {
            method = Class.class.getMethod("getMethod", String.class, Class[].class);
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(e);
        }

        initAppBt();
        initSysBt();
        initCtr();//

//        FHook.hook(method).setHookEnter(传一个接口实现类).setHookExit(传一个接口实现类).setOrigFunRun(true);
    }

    void showLog(String name_fun, Object thiz, List<Object> args, Class<?>[] types) {
        FLog.d("----------- " + name_fun + " --------");
        FLog.d("    thiz= " + thiz);
        for (int i = 0; i < args.size(); i++) {
            FLog.d("    args[" + i + "]= " + args.get(i) + " " + types[i]);
        }
    }

    void showLog(String name_fun, Object thiz, Object arg, Class<?> type) {
        showLog(name_fun, thiz,
                java.util.Collections.singletonList(arg),
                new Class<?>[]{type});
    }

    private void initCtr() {
        Button bt_main_37 = findViewById(R.id.bt_main_37);
        bt_main_37.setText("37 方法2测试");
        bt_main_37.setOnClickListener(v -> {


            FFunsUI.toast(this, "方法2测试 点击");
        });

        // --------------------------------------
        Button bt_main_01 = findViewById(R.id.bt_main_01);
        bt_main_01.setText("01 hook普通方法");
        bt_main_01.setOnClickListener(v -> {
//            doAppHook01();
//            doAppHook02();
            doAppHook03();

            FFunsUI.toast(this, "hook普通方法 点击");
        });

        Button bt_main_02 = findViewById(R.id.bt_main_02);
        bt_main_02.setText("02 初始化");
        bt_main_02.setOnClickListener(v -> {
            FHook.InitReport rep = FHook.init(this);
            Toast.makeText(this, rep.toString(), Toast.LENGTH_LONG).show();
        });


        Button bt_main_03 = findViewById(R.id.bt_main_03);
        bt_main_03.setText("03 hook系统方法");
        bt_main_03.setOnClickListener(v -> {
            try {
                doSysHook01();
            } catch (Exception e) {
                FLog.e(TAG, "doSysHook01: " + e);

            } catch (Throwable e) {
                FLog.e(TAG, "doSysHook01: " + e);

            }

            FFunsUI.toast(this, "hook系统方法 点击");

        });

        Button bt_main_04 = findViewById(R.id.bt_main_04);
        bt_main_04.setText("04 取消所有hook");
        bt_main_04.setOnClickListener(v -> {
            FHook.unHookAll();
        });

        Button bt_main_05 = findViewById(R.id.bt_main_05);
        bt_main_05.setText("05 查看已hook的情况");
        bt_main_05.setOnClickListener(v -> {
            FHook.showHookInfo();
        });

        Button bt_main_06 = findViewById(R.id.bt_main_06);
        bt_main_06.setText("06 查看类的实例hook实例");
        bt_main_06.setOnClickListener(v -> {
            TObject o1 = new TObject("o1", 1);
            TObject o2 = new TObject("o2", 1);
            TObject o3 = new TObject("o3", 1);

            FHook.hook(o1).setOrigFunRun(false).commit();

            Object[] instances = CLinker.jcJvmtiFindInstances(TObject.class, true);
            for (Object o : instances) {
                FLog.d(TAG, "TObject instance: " + o);
            }


            Class<?>[] classes1 = CLinker.jcJvmtiFindImpl(SharedPreferences.Editor.class);
            for (Class<?> clazz : classes1) {
                FHook.hook(clazz)
                        .setHookEnter((thiz, args, types, hh) -> {
                            FLog.d(TAG, "SharedPreferences.Editor.class.getName()=" + clazz.getName());
                            showLog("SharedPreferences.Editor.class.getName()", thiz, args, types);
                        })
                        .setHookExit((thiz, ret, hh) -> {
                            showLog("SharedPreferences.Editor.class.getName()", thiz, ret, ret.getClass());
                            return ret;
                        })
                        .commit();
                FLog.d(TAG, "SharedPreferences.Editor: " + clazz.getName());
            }
        });


        Button bt_main_07 = findViewById(R.id.bt_main_07);
        bt_main_07.setText("07 hook类所有方法");
        bt_main_07.setOnClickListener(v -> {
            FHook.hook(TObject.class)
                    .setOrigFunRun(false)
                    .setHookEnter((thiz, args, types, hh) -> {
                        FLog.d(TAG, "TObject.class.getName()=" + TObject.class.getName());
                    })
                    .setHookExit((thiz, ret, hh) -> {
                        FLog.d(TAG, "TObject.class.getName()=" + TObject.class.getName());
                        return ret;
                    })
                    .commit();

        });

        Button bt_main_08 = findViewById(R.id.bt_main_08);
        bt_main_08.setText("08 取消hook类所有方法");
        bt_main_08.setOnClickListener(v -> {
            FHook.unHook(TObject.class);

        });

    }

    private void initAppBt() {
        THook tHook = new THook();

//        initAppBt01(tHook);
//        initAppBt02(tHook);
        initAppBt03(tHook);

        initSysBt();

    }

    private void initSysBt() {

        initSysBt01();

    }


    // ---------------------------------------------------
    private void initSysBt01() {
//        Class<?> clazz = classLoader.loadClass("java.lang.String");
        Button bt_main_21 = findViewById(R.id.bt_main_21);
        bt_main_21.setText("21 classLoader_loadClass_Class_S");
        bt_main_21.setOnClickListener(v -> {
            try {
                ClassLoader classLoader = getClassLoader();
                Class<?> clazz = classLoader.loadClass("java.lang.String");
                if (clazz == null) {
                    return;
                }

                FFunsUI.toast(this, "classLoader.loadClass " + clazz.getName());

            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        });

//        method.invoke(new THook(), 11, 12, 34);
//        method.invoke(null, 11, 12, 34);
        Button bt_main_22 = findViewById(R.id.bt_main_22);
        bt_main_22.setText("22 method_invoke_O_OArr");
        bt_main_22.setOnClickListener(v -> {
            try {
                Method method = THook.class.getMethod("fun_I_III",
                        int.class, int.class, int.class);
                Object ret = method.invoke(new THook(), 11, 12, 34);

                Toast.makeText(this, "method_invoke_O_OArr ret=" + ret, Toast.LENGTH_SHORT).show();
                FLog.d(TAG, "method_invoke_O_OArr ret=" + ret);
            } catch (Throwable e) {
                Toast.makeText(this, "method_invoke_O_OArr err: " + e, Toast.LENGTH_SHORT).show();
                FLog.e(TAG, "method_invoke_O_OArr err: " + e);
            }
        });

//        Thread thread = Thread.currentThread();
//        thread.getContextClassLoader();
        Button bt_main_23 = findViewById(R.id.bt_main_23);
        bt_main_23.setText("23 class_getDeclaredMethod_M_S_C 多个方法");
        bt_main_23.setOnClickListener(v -> {
            Method method = null;

            try {
                Thread thread = Thread.currentThread();
                ClassLoader contextClassLoader = thread.getContextClassLoader();

                // 方案A
                Class<?> clazz = contextClassLoader.loadClass("top.feadre.fhook.FHookTool");
                method = clazz.getDeclaredMethod("printStackTrace", int.class);
                Object res = method.invoke(null, 5);

                // 方案B
//                Class<?> clazz = Class.forName("top.feadre.fhook.FHookTool", true, contextClassLoader);
//                MethodHandles.Lookup lk = MethodHandles.publicLookup();
//                MethodType mt = MethodType.methodType(void.class, int.class);
//                MethodHandle mh = lk.findStatic(clazz, "printStackTrace", mt);
//                Object res = mh.invokeWithArguments(5);


//                FLog.d(TAG, "clazz= " + clazz);
//                FLog.d(TAG, "method= " + method);
//                FLog.d(TAG, "method.invoke(null, 5)= " + res);
//                FLog.d(TAG, "mh.invokeWithArguments(5)= " + res);

            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            } catch (NoSuchMethodException e) {
                throw new RuntimeException(e);
            } catch (InvocationTargetException e) {
                throw new RuntimeException(e);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(e);
            } catch (Throwable e) {
                throw new RuntimeException(e);
            }
            Toast.makeText(this, "method -> " + method, Toast.LENGTH_SHORT).show();
        });


        Button bt_main_24 = findViewById(R.id.bt_main_24);
        bt_main_24.setText("24 Settings_Secure_getString_S_OS");
        bt_main_24.setOnClickListener(v -> {
            try {
                ContentResolver contentResolver = getContentResolver();
                String res = Settings.Secure.getString(
                        contentResolver, Settings.Secure.ANDROID_ID);

//                String res2 = Settings.Secure.getString(
//                        getContentResolver(), Settings.Secure.INPUT_METHOD_SELECTOR_VISIBILITY);
//                FLog.d(TAG, "BACKGROUND_DATA=" + res2);
//                Toast.makeText(this, "ANDROID_ID=" + res, Toast.LENGTH_SHORT).show();
                FLog.d(TAG, "Settings_Secure_getString_S_OS res= " + res);
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        // 52) SharedPreferences 写入 + 提交 —— putString(String,String) + commit() 返回 boolean
        Button bt_main_25 = findViewById(R.id.bt_main_25);
        bt_main_25.setText("25 sp_putString_S_SS");
        bt_main_25.setOnClickListener(v -> {
            try {
                SharedPreferences sp = getSharedPreferences("demo", MODE_PRIVATE);
                String key = "k";
                boolean ok = sp.edit().putString(key, "时间" + System.currentTimeMillis()).commit();
                String v2 = sp.getString(key, "");
                Toast.makeText(this, "sp_putString_S_SS=" + ok + ", v2=" + v2, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                FLog.e(TAG, "运行出错: " + e);
                Toast.makeText(this, "运行出错: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        Button bt_main_26 = findViewById(R.id.bt_main_26);
        bt_main_26.setText("26 Class_forName_Class_S");
        bt_main_26.setOnClickListener(v -> {
            try {
                Class<?> clazz = Class.forName("top.feadre.fhook.FHookTool");
                if (clazz != null) {
                    FFunsUI.toast(this, "Class_forName_Class_S 成功 " + clazz.getName());
                } else {
                    FFunsUI.toast(this, "Class_forName_Class_S 失败 ");
                }

            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        });
    }

    private void doSysHook01() throws Throwable {

        /*
         [canHook] native 不支持: public native java.lang.Object java.lang.reflect.Method.invoke
         */
//        Method method_invoke_O_OArr = FHookTool.findMethod4First(Method.class, "invoke");
//        FHook.hook(method_invoke_O_OArr)
//                .setOrigFunRun(false)
//                .setHookEnter((thiz, args, types, hh) -> {
//                    FLog.d(TAG, "[method_invoke_O_OArr] start ....");
//                    args.set(0, "java.lang.Integer");
//                })
//                .setHookExit((ret, type, hh) -> {
//                    FLog.d(TAG, "[method_invoke_O_OArr] start ....");
//                    return ret;
//                });

        // method = clazz.getDeclaredMethod("printStackTrace", int.class);
//        Method class_getDeclaredMethod_M_S_C = FHookTool.findMethod4First(
//                Class.class, "getDeclaredMethod");
//        FHook.hook(class_getDeclaredMethod_M_S_C)
//                .setOrigFunRun(false)
//                .setHookEnter((thiz, args, types, hh) -> {
//                    FLog.d(TAG, "[class_getDeclaredMethod_M_S_C] start ....");
//                })
//                .setHookExit((ret, type, hh) -> {
//                    FLog.d(TAG, "[class_getDeclaredMethod_M_S_C] start ....");
//                    return ret;
//                })
//                .commit();


        // Class<?> clazz = Class.forName("top.feadre.fhook.FHookTool");
//        Method Class_forName = FHookTool.findMethod4First(Class.class, "forName");
//        FHook.hook(Class_forName)
//                .setOrigFunRun(true)
//                .setHookEnter((thiz, args, types, hh) -> {
//                    FLog.d(TAG, "[Class_forName_Class_S] start ....");
//                    args.set(0, "java.lang.Integer");
//                })
//                .setHookExit((ret, type, hh) -> {
//                    if (ret != null) {
//                        Class<?> _ret = (Class<?>) ret;
//                        FLog.d(TAG, "[Class_forName_Class_S] end ....ret= " + _ret.getName());
//                    }
//                    return ret;
//                })
//                .commit();


//
//        // Class<?> clazz = classLoader.loadClass("java.lang.String");
//        Method classLoader_loadClass_Class_S = FHookTool.findMethod4First(ClassLoader.class, "loadClass");
//        FHook.hook(classLoader_loadClass_Class_S)
//                .setOrigFunRun(true)
//                .setHookEnter((thiz, args, types, hh) -> {
//                    FLog.d(TAG, "[classLoader_loadClass_Class_S] start ....");
//                })
//                .setHookExit((ret, type, hh) -> {
//                    FLog.d(TAG, "[classLoader_loadClass_Class_S] end .... ");
//                    return ret;
//                })
//                .commit();
//
//
//        // String res = Settings.Secure.getString(getContentResolver(), Settings.Secure.ANDROID_ID);
        {
            Method Settings_Secure_getString_S_OS =
                    android.provider.Settings.Secure.class.getMethod(
                            "getString", android.content.ContentResolver.class, String.class);

            FHook.hook(Settings_Secure_getString_S_OS)
                    .setOrigFunRun(true)
//                    .setHookEnter((thiz, args, types, hh) -> {
//                        // static 方法 thiz=null；args[0]=ContentResolver, args[1]=key
//                        showLog("Settings_Secure_getString_S_OS", thiz, args, types);
//                        args.set(1, "input_method_selector_visibility");
//
//                    })
                    .setHookExit((ret, type, hh) -> {
                        showLog("Settings_Secure_getString_S_OS", hh.thisObject, ret, type);
                        // 演示：在返回值后面加标记，肉眼可见 hook 生效
                        if (ret instanceof String) {
                            ret = ret + "_HOOK";
                        }else{
                            ret = "null_null_";
                        }
                        return ret;
                    })
                    .commit();
        }

        // boolean ok = sp.edit().putString(key, "时间" + System.currentTimeMillis()).commit();
        // 从实现类上取真实的 commit()（不是接口上的）
        //  boolean commit();
        Method sp_putString_S_SS = SharedPreferences.Editor.class.getMethod("putString", String.class, String.class);
        SharedPreferences.Editor editor = this.getSharedPreferences("demo", MODE_PRIVATE).edit();
        // 这里 hook的是 sp_putString_S_SS
        Method mCommitImpl = FHookTool.findMethod4Impl(editor, sp_putString_S_SS);

        FHook.hook(mCommitImpl)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    // thiz 是具体 Editor 实例；无参数
                    showLog("SharedPreferences.Editor.commit", thiz, args, types);
                })
                .setHookExit((ret, type, hh) -> {
                    // ret 是 Boolean（primitive boolean 会被装箱）
                    showLog("SharedPreferences.Editor.commit", hh.thisObject, ret, type);
                    // 你也可以强制返回 true 观测效果：
                    // 运行出错: java.lang.ClassCastException: java.lang.Boolean cannot be cast to android.content.SharedPreferences$Editor
                    ret = false;
                    return ret;
                })
                .commit();


    }

    private void doAppHook03() {
        Method fun_TObject_TObject = FHookTool.findMethod4First(THook.class, "fun_TObject_TObject");
        FHook.hook(fun_TObject_TObject)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_TObject_TObject", thiz, args, types);
                    TObject o1 = (TObject) args.get(0);
                    o1.setAge(9999);
                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_TObject_TObject", hh.thisObject, ret, type);
                    if (ret == null) {
                        ret = new TObject("bbbbbbbbbbb", 8888);
                        return ret;
                    }
                    TObject o1 = (TObject) ret;
                    o1.setName("aaaaaabbb");
                    return ret;
                })
                .commit();


        Method fun_I_IArr = FHookTool.findMethod4First(THook.class, "fun_I_IArr");
        FHook.hook(fun_I_IArr)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_I_IArr", thiz, args, types);
                    int[] arr = (int[]) args.get(0);
                    arr[0] = 666;
                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_I_IArr", hh.thisObject, ret, type);

                    ret = 999;
                    return ret;
                })
                .commit();

        Method fun_IArr_DArr = FHookTool.findMethod4First(THook.class, "fun_IArr_DArr");
        FHook.hook(fun_IArr_DArr)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_IArr_DArr", thiz, args, types);
                    double[] arg0 = (double[]) args.get(0);
                    arg0[2] = 999.99999;

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_IArr_DArr", hh.thisObject, ret, type);
                    int[] ret1 = (int[]) ret;
                    ret1[0] = 666;
                    return ret;
                })
                .commit();

        Method fun_TObjectArr_DArr = FHookTool.findMethod4First(THook.class, "fun_TObjectArr_DArr");
        FHook.hook(fun_TObjectArr_DArr)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_TObjectArr_DArr", thiz, args, types);
                    double[] o = (double[]) args.get(0);
                    o[0] = 999.9999;

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_TObjectArr_DArr", hh.thisObject, ret, type);
                    if (ret == null) {
                        TObject[] out = new TObject[2];
                        out[0] = new TObject("_new_" + 999, 999);
                        return out; // 一定要返回 TObject[]，与原方法签名匹配
                    }

                    TObject[] oarr = (TObject[]) ret;
                    oarr[0].setName("_new_" + 999);
                    return ret;

                })
                .commit();

        Method fun_double_DArr = FHookTool.findMethod4First(THook.class, "fun_double_DArr");
        FHook.hook(fun_double_DArr)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_double_DArr", thiz, args, types);
                    double[] o = (double[]) args.get(0);
                    o[0] = 5555.555;

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_double_DArr", hh.thisObject, ret, type);
                    if (ret == null) {
                        double out = 456.6666;
                        return out;
                    }
                    ret = 22.2;
                    return ret;
                })
                .commit();

        Method jtFun_JArr_JArr = FHookTool.findMethod4First(THook.class, "jtFun_JArr_JArr");
        FHook.hook(jtFun_JArr_JArr)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("jtFun_JArr_JArr", thiz, args, types);
                    long[] o = (long[]) args.get(0);
                    o[0] = 999;
                    hh.extras.put("args", new java.util.ArrayList<>(args));// 这里要再包一层
                })

                .setHookExit((ret, type, hh) -> {
                    showLog("jtFun_JArr_JArr", hh.thisObject, ret, type);
                    if (ret == null) {
                        long[] out = new long[2];
                        out[0] = 6666;
                        return out;
                    }
                    ArrayList<long[]> args = (ArrayList<long[]>) hh.extras.get("args");
                    for (int i = 0; i < args.size(); i++) {
                        FLog.d(TAG, "jtFun_JArr_JArr ... args[" + i + "]=" + Arrays.toString(args.get(i)));
                    }
                    long[] r = (long[]) ret;
                    r[1] = 99;
                    return ret;
                })
                .commit();

    }

    private void doAppHook02() {
        Method fun_V_V = FHookTool.findMethod4First(THook.class, "fun_V_V");
        Method jcFun_V_V = FHookTool.findMethod4First(THook.class, "jcFun_V_V");


        FHook.hook(fun_V_V)
                .setOrigFunRun(false)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_V_V", thiz, args, types);

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_V_V", hh.thisObject, ret, type);

                    return ret;
                })
                .commit();

        FHook.hook(jcFun_V_V)
                .setOrigFunRun(false)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("jcFun_V_V", thiz, args, types);

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("jcFun_V_V", hh.thisObject, ret, type);

                    return ret;
                })
                .commit();

    }

    private void doAppHook01() {
        Method fun_String_String2 = FHookTool.findMethod4First(THook.class, "fun_String_String2");
        Method fun_I_III = FHookTool.findMethod4First(THook.class, "fun_I_III");
        Method jtFun_I_II = FHookTool.findMethod4First(THook.class, "jtFun_I_II");

        FHook.hook(fun_String_String2).setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_String_String2", thiz, args, types);
                    String args0 = (String) args.set(0, "111");
                    String args1 = (String) args.get(1);
                    args1 = "9999" + args0;
                    args.set(1, args1);
                })
                .setHookExit(
                        (ret, type, hh) -> {
                            showLog("fun_String_String2", hh.thisObject, ret, type);

                            ret = "11111";
                            return ret;
                        })
                .commit();

        FHook.hook(fun_I_III)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_I_III", thiz, args, types);
                    args.set(0, 6666);

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_I_III", hh.thisObject, ret, type);

                    ret = 8888;
                    return ret;
                })
                .setOrigFunRun(true)
                .commit();

        FHook.hook(jtFun_I_II)
                .setOrigFunRun(false)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("jtFun_I_II", thiz, args, types);
                    args.set(0, 8888);

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("jtFun_I_II", hh.thisObject, ret, type);

                    ret = 99999;
                    return ret;
                })
                .commit();
    }
    // 替换你的 initViews()

    private void initAppBt03(THook tHook) {
        Button bt_main_14 = findViewById(R.id.bt_main_14);
        bt_main_14.setText("14 fun_TObject_TObject");
        bt_main_14.setOnClickListener(v -> {
            FFunsUI.toast(this, String.valueOf(
                    tHook.fun_TObject_TObject(
                            new TObject("jerry", 20))));
        });

        Button bt_main_15 = findViewById(R.id.bt_main_15);
        bt_main_15.setText("15 fun_I_IArr");
        bt_main_15.setOnClickListener(v -> {
            FFunsUI.toast(this, String.valueOf(
                    tHook.fun_I_IArr(new int[]{1, 2, 3})
            ));
        });

        Button bt_main_16 = findViewById(R.id.bt_main_16);
        bt_main_16.setText("16 fun_IArr_DArr");
        bt_main_16.setOnClickListener(v -> {
            FFunsUI.toast(this, Arrays.toString(
                    tHook.fun_IArr_DArr(new double[]{1.1, 2.2, 3.3})));
        });

        Button bt_main_17 = findViewById(R.id.bt_main_17);
        bt_main_17.setText("17 fun_TObjectArr_IArr");
        bt_main_17.setOnClickListener(v -> {
            FFunsUI.toast(this, Arrays.toString(
                    tHook.fun_TObjectArr_DArr(new double[]{1.1, 2.2, 3.3})));
        });

        Button bt_main_18 = findViewById(R.id.bt_main_18);
        bt_main_18.setText("18 fun_double_DArr");
        bt_main_18.setOnClickListener(v -> {
            FFunsUI.toast(this,
                    String.valueOf(tHook.fun_double_DArr(new double[]{1.1, 2.2, 3.3})));
        });

        Button bt_main_19 = findViewById(R.id.bt_main_19);
        bt_main_19.setText("19 fun_TObjectArr_IArr");
        bt_main_19.setOnClickListener(v -> {
            FFunsUI.toast(this, Arrays.toString(
                    THook.jtFun_JArr_JArr(new long[]{1, 2, 3})));
        });
    }

    private void initAppBt02(THook tHook) {
        Button bt_main_12 = findViewById(R.id.bt_main_12);
        bt_main_12.setText("12 fun_V_V");
        bt_main_12.setOnClickListener(v -> {
            tHook.fun_V_V();
            FFunsUI.toast(this, "fun_V_V");
        });

        Button bt_main_13 = findViewById(R.id.bt_main_13);
        bt_main_13.setText("13 jcFun_V_V");
        bt_main_13.setOnClickListener(v -> {
            tHook.fun_V_V();
            FFunsUI.toast(this, "jcFun_V_V");
        });
    }

    private void initAppBt01(THook tHook) {
        // 09~10 你已写，这里统一用工具方法重写一遍
        Button bt_main_09 = findViewById(R.id.bt_main_09);
        bt_main_09.setText("09 fun_String_String2");
        bt_main_09.setOnClickListener(v -> {
            FFunsUI.toast(this, String.valueOf(tHook.fun_String_String2("09", "09")));
        });

        Button bt_main_10 = findViewById(R.id.bt_main_10);
        bt_main_10.setText("10 fun_I_III");
        bt_main_10.setOnClickListener(v -> {
            FFunsUI.toast(this, String.valueOf(tHook.fun_I_III(10, 20, 30)));
        });

        Button bt_main_11 = findViewById(R.id.bt_main_11);
        bt_main_11.setText("11 jtFun_I_II");
        bt_main_11.setOnClickListener(v -> {
            FFunsUI.toast(this, String.valueOf(THook.jtFun_I_II(20, 30)));

        });
    }

}