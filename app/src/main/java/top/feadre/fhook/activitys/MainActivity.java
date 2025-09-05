package top.feadre.fhook.activitys;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.widget.Button;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;

import top.feadre.fhook.CLinker;
import top.feadre.fhook.FCFG;
import top.feadre.fhook.FHook;
import top.feadre.fhook.FHookTool;
import top.feadre.fhook.R;
import top.feadre.fhook.THook;
import top.feadre.fhook.TObject;
import top.feadre.fhook.flibs.fsys.FLog;
import top.feadre.fhook.tools.FFunsUI;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = FCFG.TAG_PREFIX + "MainActivity";


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

        initSysHook01();

    }

    private void initSysHook01() {
        Button bt_main_21 = findViewById(R.id.bt_main_21);
        bt_main_21.setText("21 Class.forName");
        bt_main_21.setOnClickListener(v -> {
            try {
                ClassLoader classLoader = getClassLoader();
                Class<?> aClass = classLoader.loadClass("java.lang.String");
                if (aClass == null) {
                    return;
                }

                FFunsUI.toast(this, "got: " + aClass.getName());

            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        });

        // 49) Method.invoke(Object, Object...) —— 变参 + 返回 Object（反射调用）
        Button bt_main_22 = findViewById(R.id.bt_main_22);
        bt_main_22.setText("22 Method.invoke(THook.fun_I_II)");
        bt_main_22.setOnClickListener(v -> {
            try {
                Method m =
                        THook.class.getMethod("fun_I_II", int.class, int.class);
                Object ret = m.invoke(new THook(), 12, 34);
                Toast.makeText(this, "ret=" + ret, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        // 50) Constructor.newInstance(Object...) —— 变参 + 返回实例
        Button bt_main_23 = findViewById(R.id.bt_main_23);
        bt_main_23.setText("23 Ctor.newInstance(String(bytes,charset))");
        bt_main_23.setOnClickListener(v -> {
            try {
                java.lang.reflect.Constructor<String> c =
                        String.class.getConstructor(byte[].class, java.nio.charset.Charset.class);
                String s = c.newInstance("hi".getBytes(java.nio.charset.StandardCharsets.UTF_8),
                        java.nio.charset.StandardCharsets.UTF_8);
                Toast.makeText(this, "new String -> " + s, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        // 51) Settings.Secure.getString(ContentResolver, String) —— 2参 + 返回 String（常被审计）
        Button bt_main_24 = findViewById(R.id.bt_main_24);
        bt_main_24.setText("24 Settings.Secure.getString(ANDROID_ID)");
        bt_main_24.setOnClickListener(v -> {
            try {
                String res = Settings.Secure.getString(
                        getContentResolver(), Settings.Secure.ANDROID_ID);
                Toast.makeText(this, "ANDROID_ID=" + res, Toast.LENGTH_SHORT).show();
                FLog.d(TAG, "ANDROID_ID=" + res);
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        // 52) SharedPreferences 写入 + 提交 —— putString(String,String) + commit() 返回 boolean
        Button bt_main_25 = findViewById(R.id.bt_main_25);
        bt_main_25.setText("25 SharedPreferences.put/commit");
        bt_main_25.setOnClickListener(v -> {
            try {
                SharedPreferences sp = getSharedPreferences("demo", MODE_PRIVATE);
                boolean ok = sp.edit().putString("k", "时间" + System.currentTimeMillis()).commit();
                String v2 = sp.getString("k", "");
                Toast.makeText(this, "commit=" + ok + ", v=" + v2, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });
    }

    private void doSysHook01() throws Throwable {

        // ============ 21) ClassLoader.loadClass(String) ============
//        {
//            java.lang.reflect.Method mLoadClass =
//                    ClassLoader.class.getMethod("loadClass", String.class);
//
//            FHook.hook(mLoadClass)
//                    .setOrigFunRun(false)
//                    .setHookEnter((thiz, args, types, hh) -> {
//                        // thiz 是 ClassLoader 实例；args[0] 是 name
//                        String name = (String) args.get(0);
//                        showLog("ClassLoader.loadClass", thiz, args, types);
//                        // 你也可以试着替换类名验证 hook 生效：
//                        // if ("java.lang.String".equals(name)) args.set(0, "java.lang.Integer");
//                    })
//                    .setHookExit((ret, type, hh) -> {
//                        // ret 是 Class<?>，打印类名
//                        Class<?> c = (Class<?>) ret;
//                        showLog("ClassLoader.loadClass", hh.thisObject, ret, type);
//                        FFunsUI.toast(this, "[hook] loadClass -> " + (c != null ? c.getName() : "null"));
//                        return ret;
//                    })
//                    .commit();
//        }

        // ============ 22) Method.invoke(Object, Object...) ============
//        {
//            java.lang.reflect.Method mInvoke =
//                    java.lang.reflect.Method.class.getMethod("invoke", Object.class, Object[].class);
//
//            FHook.hook(mInvoke)
//                    .setOrigFunRun(true)
//                    .setHookEnter((thiz, args, types, hh) -> {
//                        // thiz 是 java.lang.reflect.Method
//                        java.lang.reflect.Method target = (java.lang.reflect.Method) thiz;
//                        Object targetObj = args.get(0);          // 目标实例（static 时为 null）
//                        Object[] invokeArgs = (Object[]) args.get(1); // 实参数组
//
//                        showLog("Method.invoke", thiz, args, types);
//
//                        // 演示：如果是调用 THook.fun_I_II(int,int)，把第一个参数+100，看是否影响结果
//                        if (target.getDeclaringClass() == top.feadre.fhook.THook.class
//                                && "fun_I_II".equals(target.getName())
//                                && invokeArgs != null && invokeArgs.length >= 2
//                                && invokeArgs[0] instanceof Integer) {
//                            int old0 = (Integer) invokeArgs[0];
//                            invokeArgs[0] = old0 + 100;
//                        }
//                    })
//                    .setHookExit((ret, type, hh) -> {
//                        // ret 是被调用方法的返回值（Object）
//                        showLog("Method.invoke", hh.thisObject, ret, type);
//                        FFunsUI.toast(this, "[hook] Method.invoke ret=" + ret);
//                        return ret;
//                    })
//                    .commit();
//        }

//// ============ 23) Constructor.newInstance(Object...) ============
//        {
//            java.lang.reflect.Method mCtorNewInstance =
//                    java.lang.reflect.Constructor.class.getMethod("newInstance", Object[].class);
//
//            FHook.hook(mCtorNewInstance)
//                    .setOrigFunRun(true)
//                    .setHookEnter((thiz, args, types, hh) -> {
//                        // thiz 是 Constructor<?>；args[0] 是 Object[] initargs
//                        java.lang.reflect.Constructor<?> ctor = (java.lang.reflect.Constructor<?>) thiz;
//                        Object[] initargs = (Object[]) args.get(0);
//                        showLog("Constructor.newInstance", thiz, args, types);
//
//                        // demo：若是 String(byte[], Charset) 构造，替换第一个参数内容
//                        if (ctor.getDeclaringClass() == String.class && initargs != null && initargs.length == 2) {
//                            if (initargs[0] instanceof byte[]) {
//                                byte[] b = (byte[]) initargs[0];
//                                // 简单演示：把前两个字节改掉
//                                if (b.length >= 2) {
//                                    b[0] = 'H';
//                                    b[1] = 'K';
//                                }
//                            }
//                        }
//                    })
//                    .setHookExit((ret, type, hh) -> {
//                        // ret 是新建的实例
//                        showLog("Constructor.newInstance", hh.thisObject, ret, type);
//                        if (ret instanceof String) {
//                            ret = ((String) ret) + " [hooked]";
//                        }
//                        return ret;
//                    })
//                    .commit();
//        }

// ============ 24) Settings.Secure.getString(ContentResolver, String) ============
        {
            java.lang.reflect.Method mGetString =
                    android.provider.Settings.Secure.class.getMethod(
                            "getString", android.content.ContentResolver.class, String.class);

            FHook.hook(mGetString)
                    .setOrigFunRun(true)
                    .setHookEnter((thiz, args, types, hh) -> {
                        // static 方法 thiz=null；args[0]=ContentResolver, args[1]=key
                        showLog("Settings.Secure.getString", thiz, args, types);
                    })
                    .setHookExit((ret, type, hh) -> {
                        showLog("Settings.Secure.getString", hh.thisObject, ret, type);
                        // 演示：在返回值后面加标记，肉眼可见 hook 生效
                        if (ret instanceof String) {
                            ret = ret + "_HOOK";
                        }
                        return ret;
                    })
                    .commit();
        }

// ============ 25) SharedPreferences.Editor.commit() ============
        {
            // 从实现类上取真实的 commit()（不是接口上的）
            //  boolean commit();
            Method mCommitIface = SharedPreferences.Editor.class.getMethod("commit");
            SharedPreferences.Editor editor = this.getSharedPreferences("demo", MODE_PRIVATE).edit();
            Method mCommitImpl = FHookTool.findMethod4Impl(editor, mCommitIface);

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
                        ret = false;
                        return ret;
                    })
                    .commit();
        }


    }

    private void doAppHook03() {
        Method fun_TObject_TObject = FHookTool.findMethod4First(THook.class, "fun_TObject_TObject");
        FHook.hook(fun_TObject_TObject)
                .setOrigFunRun(false)
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
                .setOrigFunRun(false)
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

        Method jcFun_JArr_JArr = FHookTool.findMethod4First(THook.class, "jcFun_JArr_JArr");
        FHook.hook(jcFun_JArr_JArr)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("jcFun_JArr_JArr", thiz, args, types);
                    long[] o = (long[]) args.get(0);
                    o[0] = 666;

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("jcFun_JArr_JArr", hh.thisObject, ret, type);
                    if (ret == null) {
                        long[] out = new long[2];
                        out[0] = 6666;
                        return out;
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
                    THook.jcFun_JArr_JArr(new long[]{1, 2, 3})));
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