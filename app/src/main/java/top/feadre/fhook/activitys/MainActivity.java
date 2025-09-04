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
//            initHook01();
//            initHook02();
            initHook03();

        });

        Button bt_main_02 = findViewById(R.id.bt_main_02);
        bt_main_02.setText("02 hook初始化");
        bt_main_02.setOnClickListener(v -> {
            FHook.InitReport rep = FHook.init(this);
            Toast.makeText(this, rep.toString(), Toast.LENGTH_LONG).show();
        });


        Button bt_main_03 = findViewById(R.id.bt_main_03);
        bt_main_03.setText("03hook系统方法");
        bt_main_03.setOnClickListener(v -> {
            Method forName = FHookTool.findMethod4First(Class.class, "forName");
            Method getString = FHookTool.findMethod4First(Settings.Secure.class, "getString");
            Method commit = FHookTool.findMethod4First(SharedPreferences.class, "commit");
            FHook.hook(forName).setOrigFunRun(false).commit();

            FHook.hook(getString).setOrigFunRun(true).commit();

            FHook.hook(commit).setOrigFunRun(true).commit();
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


    }

    private void initAppBt() {
        THook tHook = new THook();

//        initAppBt01(tHook);
//        initAppBt02(tHook);
        initAppBt03(tHook);

    }

    private void initSysBt() {
        // 48) ClassLoader.loadClass(String, boolean) —— 2参 + 返回 Class
//        Button b48 = findViewById(R.id.bt_main_48);
//        b48.setText("ClassLoader.loadClass(name,true)");
//        b48.setOnClickListener(v -> {
//            try {
//                Class<?> c = getClassLoader().loadClass("java.lang.String");
//                Toast.makeText(this, "got: " + c.getName(), Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            }
//        });


        Button bt_main_48 = findViewById(R.id.bt_main_48);
        bt_main_48.setText("48 Class.forName");
        bt_main_48.setOnClickListener(v -> {
            try {
                ClassLoader classLoader = getClassLoader();
                classLoader.loadClass("java.lang.String");
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        });

        // 49) Method.invoke(Object, Object...) —— 变参 + 返回 Object（反射调用）
        Button b49 = findViewById(R.id.bt_main_49);
        b49.setText("Method.invoke(THook.fun_I_II)");
        b49.setOnClickListener(v -> {
            try {
                java.lang.reflect.Method m =
                        top.feadre.fhook.THook.class.getMethod("fun_I_II", int.class, int.class);
                Object ret = m.invoke(new top.feadre.fhook.THook(), 12, 34);
                Toast.makeText(this, "ret=" + ret, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        // 50) Constructor.newInstance(Object...) —— 变参 + 返回实例
        Button b50 = findViewById(R.id.bt_main_50);
        b50.setText("Ctor.newInstance(String(bytes,charset))");
        b50.setOnClickListener(v -> {
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
        Button b51 = findViewById(R.id.bt_main_51);
        b51.setText("Settings.Secure.getString(ANDROID_ID)");
        b51.setOnClickListener(v -> {
            try {
                String id = android.provider.Settings.Secure.getString(
                        getContentResolver(), android.provider.Settings.Secure.ANDROID_ID);
                Toast.makeText(this, "ANDROID_ID=" + id, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        // 52) SharedPreferences 写入 + 提交 —— putString(String,String) + commit() 返回 boolean
        Button b52 = findViewById(R.id.bt_main_52);
        b52.setText("SharedPreferences.put/commit");
        b52.setOnClickListener(v -> {
            try {
                android.content.SharedPreferences sp = getSharedPreferences("demo", MODE_PRIVATE);
                boolean ok = sp.edit().putString("k", "v@" + System.currentTimeMillis()).commit();
                String v2 = sp.getString("k", "");
                Toast.makeText(this, "commit=" + ok + ", v=" + v2, Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        //        // 53) SQLiteDatabase.query(...) —— 8参 + 返回 Cursor（高频 SQL Hook 点）
//        Button b53 = findViewById(R.id.bt_main_53);
//        b53.setText("SQLiteDatabase.query(8参)");
//        b53.setOnClickListener(v -> {
//            android.database.sqlite.SQLiteDatabase db = null;
//            android.database.Cursor c = null;
//            try {
//                db = android.database.sqlite.SQLiteDatabase.create(null);
//                db.execSQL("CREATE TABLE t(a INTEGER, b TEXT)");
//                db.execSQL("INSERT INTO t(a,b) VALUES(?,?)", new Object[]{1, "x"});
//                db.execSQL("INSERT INTO t(a,b) VALUES(?,?)", new Object[]{2, "y"});
//                c = db.query("t",
//                        new String[]{"a","b"},
//                        "a>?",
//                        new String[]{"0"},
//                        null, null,
//                        "a DESC",
//                        "10");
//                int rows = c.getCount();
//                Toast.makeText(this, "rows=" + rows, Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            } finally {
//                if (c != null) c.close();
//                if (db != null) db.close();
//            }
//        });
//
//        // 54) Cipher.init(int, Key, SecureRandom) + doFinal(byte[]) —— 3参 + 返回 byte[]
//        Button b54 = findViewById(R.id.bt_main_54);
//        b54.setText("Cipher.init/doFinal(AES)");
//        b54.setOnClickListener(v -> {
//            try {
//                javax.crypto.Cipher cipher = javax.crypto.Cipher.getInstance("AES/ECB/PKCS5Padding");
//                javax.crypto.spec.SecretKeySpec key =
//                        new javax.crypto.spec.SecretKeySpec(
//                                "0123456789abcdef".getBytes(java.nio.charset.StandardCharsets.UTF_8),
//                                "AES");
//                java.security.SecureRandom rnd = new java.security.SecureRandom();
//                cipher.init(javax.crypto.Cipher.ENCRYPT_MODE, key, rnd);
//                byte[] out = cipher.doFinal("hello".getBytes(java.nio.charset.StandardCharsets.UTF_8));
//                Toast.makeText(this, "enc len=" + out.length, Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            }
//        });
//
//        // 55) MessageDigest.getInstance + digest(byte[]) —— 1参 + 返回 byte[]
//        Button b55 = findViewById(R.id.bt_main_55);
//        b55.setText("MessageDigest.digest(SHA-256)");
//        b55.setOnClickListener(v -> {
//            try {
//                java.security.MessageDigest md = java.security.MessageDigest.getInstance("SHA-256");
//                byte[] out = md.digest("abc".getBytes(java.nio.charset.StandardCharsets.UTF_8));
//                String head = String.format("%02x%02x%02x%02x", out[0], out[1], out[2], out[3]);
//                Toast.makeText(this, "sha256[0..3]=" + head, Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            }
//        });
//
//        // 56) Parcel.obtain()/writeInt/readInt/recycle —— 多步 + 常见 Binder 数据面 Hook
//        Button b56 = findViewById(R.id.bt_main_56);
//        b56.setText("Parcel.write/read");
//        b56.setOnClickListener(v -> {
//            android.os.Parcel p = null;
//            try {
//                p = android.os.Parcel.obtain();
//                p.writeInt(42);
//                p.setDataPosition(0);
//                int val = p.readInt();
//                Toast.makeText(this, "parcel val=" + val, Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            } finally {
//                if (p != null) p.recycle();
//            }
//        });
//
//        // 57) Intent.putExtra(String,int)（链式）+ getIntExtra —— 2参 + 返回 Intent/int
//        Button b57 = findViewById(R.id.bt_main_57);
//        b57.setText("Intent.putExtra/getExtra");
//        b57.setOnClickListener(v -> {
//            try {
//                android.content.Intent it = new android.content.Intent()
//                        .putExtra("k", 123)
//                        .putExtra("m", 456);
//                int got = it.getIntExtra("m", -1);
//                Toast.makeText(this, "extra m=" + got, Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            }
//        });
//
//        // 58) Uri.buildUpon().appendQueryParameter(...).build() —— 多步 + 返回 Uri
//        Button b58 = findViewById(R.id.bt_main_58);
//        b58.setText("Uri.buildUpon/appendQP/build");
//        b58.setOnClickListener(v -> {
//            try {
//                android.net.Uri u = android.net.Uri.parse("https://ex.com/p")
//                        .buildUpon()
//                        .appendQueryParameter("a", "1")
//                        .appendQueryParameter("b", "2")
//                        .build();
//                Toast.makeText(this, u.toString(), Toast.LENGTH_SHORT).show();
//            } catch (Throwable e) {
//                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
//            }
//        });
    }

    private void initHook03() {
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


        Method fun_I_IArray = FHookTool.findMethod4First(THook.class, "fun_I_IArray");
        FHook.hook(fun_I_IArray)
                .setOrigFunRun(false)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_I_IArray", thiz, args, types);
                    int[] arr = (int[]) args.get(0);
                    arr[0] = 666;
                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_I_IArray", hh.thisObject, ret, type);

                    ret = 999;
                    return ret;
                })
                .commit();

        Method fun_I_DArray = FHookTool.findMethod4First(THook.class, "fun_I_DArray");
        FHook.hook(fun_I_DArray)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_I_DArray", thiz, args, types);
                    double[] arg0 = (double[]) args.get(0);
                    arg0[2] = 999;

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_I_DArray", hh.thisObject, ret, type);
                    int[] ret1 = (int[]) ret;
                    ret1[0] = 888;
                    return ret;
                })
                .commit();

        Method fun_DArray_IArray = FHookTool.findMethod4First(THook.class, "fun_DArray_IArray");
        FHook.hook(fun_DArray_IArray)
                .setOrigFunRun(true)
                .setHookEnter((thiz, args, types, hh) -> {
                    showLog("fun_DArray_IArray", thiz, args, types);
                    double[] o = (double[]) args.get(0);
                    o[0] = 999;

                })
                .setHookExit((ret, type, hh) -> {
                    showLog("fun_DArray_IArray", hh.thisObject, ret, type);
                    TObject[] ret1 = (TObject[]) ret;
                    ret1[0].setName("aaaaaabbb");
                    ret1[1] = new TObject("cccccc", 9999);
                    return ret;
                })
                .commit();

    }

    private void initHook02() {
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

    private void initHook01() {
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
        bt_main_15.setText("15 fun_I_IArray");
        bt_main_15.setOnClickListener(v -> {
            FFunsUI.toast(this, String.valueOf(
                    tHook.fun_I_IArray(new int[]{1, 2, 3})
            ));
        });

        Button bt_main_16 = findViewById(R.id.bt_main_16);
        bt_main_16.setText("15 fun_I_DArray");
        bt_main_16.setOnClickListener(v -> {
            FFunsUI.toast(this, Arrays.toString(
                    tHook.fun_I_DArray(new double[]{1.1, 2.2, 3.3})));
        });

        Button bt_main_17 = findViewById(R.id.bt_main_17);
        bt_main_17.setText("17 fun_DArray_IArray");
        bt_main_17.setOnClickListener(v -> {
            FFunsUI.toast(this, Arrays.toString(
                    tHook.fun_DArray_IArray(new double[]{1.1, 2.2, 3.3})));
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