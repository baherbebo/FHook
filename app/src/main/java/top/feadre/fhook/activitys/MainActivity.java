package top.feadre.fhook.activitys;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import java.lang.reflect.Member;
import java.lang.reflect.Method;

import top.feadre.fhook.FCFG;
import top.feadre.fhook.FHook;
import top.feadre.fhook.FHookTool;
import top.feadre.fhook.HookParam;
import top.feadre.fhook.R;
import top.feadre.fhook.THook;
import top.feadre.fhook.TObject;

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

        initViews();
        initCtr();//

//        FHook.hook(method).setHookEnter(传一个接口实现类).setHookExit(传一个接口实现类).setOrigFunRun(true);
    }

    private void initCtr() {
        Button bt_main_01 = findViewById(R.id.bt_main_01);
        bt_main_01.setText("hook方法");
        bt_main_01.setOnClickListener(v -> {
            Method fun_I_II = FHookTool.findMethod4First(THook.class, "fun_I_II");
            Method fun_J_DIJ = FHookTool.findMethod4First(THook.class, "fun_J_DIJ");
            Method jc_I_IntArray = FHookTool.findMethod4First(THook.class, "jc_I_IntArray");
            FHook.hook(fun_I_II).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setOrigFunRun(false);

            FHook.hook(fun_J_DIJ).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setOrigFunRun(true);

            FHook.hook(jc_I_IntArray).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setOrigFunRun(true);
        });

        Button bt_main_02 = findViewById(R.id.bt_main_02);
        bt_main_02.setText("hook初始化");
        bt_main_02.setOnClickListener(v -> {
            FHook.InitReport rep = FHook.init(this);
            Toast.makeText(this, rep.toString(), Toast.LENGTH_LONG).show();
        });

    }

    // 替换你的 initViews()
    private void initViews() {
        THook tHook = new THook();

        // 09~10 你已写，这里统一用工具方法重写一遍
        bindV(R.id.bt_main_09, "fun_V_V", () -> tHook.fun_V_V());
        bindV(R.id.bt_main_10, "fun_V_I", () -> tHook.fun_V_I(10));

        // 11~44 覆盖 demoRun() 里的所有方法（实例 + 静态）
        bindR(R.id.bt_main_11, "fun_I_I", () -> tHook.fun_I_I(9));
        bindR(R.id.bt_main_12, "fun_I_II", () -> tHook.fun_I_II(3, 4));
        bindR(R.id.bt_main_13, "fun_I_III", () -> tHook.fun_I_III(1, 2, 3));

        bindR(R.id.bt_main_14, "fun_String_String", () -> tHook.fun_String_String("abc"));
        bindR(R.id.bt_main_15, "fun_String_StringArray", () -> tHook.fun_String_StringArray(new String[]{"x", "y", "z"}));

        bindR(R.id.bt_main_16, "fun_TObject_T", () -> tHook.fun_TObject_T(new TObject("tom", 18)));
        bindV(R.id.bt_main_17, "fun_V_TObjectArray", () -> tHook.fun_V_TObjectArray(
                new TObject[]{new TObject("tom", 18), new TObject("jerry", 20)}));

        bindR(R.id.bt_main_18, "fun_I_IArray", () -> tHook.fun_I_IArray(new int[]{1, 2, 3}));
        bindR(R.id.bt_main_19, "fun_I_Varargs", () -> tHook.fun_I_Varargs(5, 6, 7));

        bindR(R.id.bt_main_20, "fun_J_J", () -> tHook.fun_J_J(100L));
        bindR(R.id.bt_main_21, "fun_D_D", () -> tHook.fun_D_D(2.5));

        bindR(R.id.bt_main_22, "fun_I_ILJ", () -> tHook.fun_I_ILJ(1, 1000L, 3));
        bindV(R.id.bt_main_23, "fun_V_LI", () -> tHook.fun_V_LI(100L, 2));
        bindV(R.id.bt_main_24, "fun_V_IL", () -> tHook.fun_V_IL(2, 100L));

        bindR(R.id.bt_main_25, "fun_J_DIJ", () -> tHook.fun_J_DIJ(3.14, 9, 100L));
        bindR(R.id.bt_main_26, "fun_D_IDI", () -> tHook.fun_D_IDI(1, 2.5, 3));

        bindR(R.id.bt_main_27, "fun_Z_Z", () -> tHook.fun_Z_Z(true));
        bindR(R.id.bt_main_28, "fun_C_C", () -> tHook.fun_C_C('a'));
        bindR(R.id.bt_main_29, "fun_S_S", () -> tHook.fun_S_S((short) 9));
        bindR(R.id.bt_main_30, "fun_B_B", () -> tHook.fun_B_B((byte) 7));
        bindR(R.id.bt_main_31, "fun_F_F", () -> tHook.fun_F_F(3.0f));

        bindV(R.id.bt_main_32, "fun_V_Sync", () -> tHook.fun_V_Sync(123));
        bindTryV(R.id.bt_main_33, "fun_V_Throw", () -> tHook.fun_V_Throw());

        // 静态方法
        bindV(R.id.bt_main_34, "jc_fun_V_V", THook::jc_fun_V_V);
        bindV(R.id.bt_main_35, "jc_fun_V_I", () -> THook.jc_fun_V_I(8));
        bindV(R.id.bt_main_36, "jc_fun_V_II", () -> THook.jc_fun_V_II(8, 9));

        bindR(R.id.bt_main_37, "jc_I_II", () -> THook.jc_I_II(10, 20));
        bindR(R.id.bt_main_38, "jc_J_JJ", () -> THook.jc_J_JJ(100L, 200L));
        bindR(R.id.bt_main_39, "jc_D_DD", () -> THook.jc_D_DD(1.2, 3.4));
        bindR(R.id.bt_main_40, "jc_String_String", () -> THook.jc_String_String("XYZ"));
        bindR(R.id.bt_main_41, "jc_TObject_T", () -> THook.jc_TObject_T(new TObject("bob", 30)));
        bindR(R.id.bt_main_42, "jc_I_IntArray", () -> THook.jc_I_IntArray(new int[]{7, 8, 9}));
        bindR(R.id.bt_main_43, "jc_I_Varargs", () -> THook.jc_I_Varargs(10, 20, 30));
        bindR(R.id.bt_main_44, "jc_D_DID", () -> THook.jc_D_DID(1.5, 2, 3.5));

        // 45：一键跑 demoRun（可选）
        bindV(R.id.bt_main_45, "demoRun()", THook::demoRun);

        // 如果你还有更多按钮 (46~52)，可以按需加自己用的测试项
        bindV(R.id.bt_main_46, "System.exit", () -> System.exit(99));

        // 47) Class.forName(String, boolean, ClassLoader) —— 3参 + 返回 Class
        Button b47 = findViewById(R.id.bt_main_47);
        b47.setText("Class.forName(name,init,loader)");
        b47.setOnClickListener(v -> {
            try {
                Class<?> c = Class.forName("java.lang.Integer", true, getClassLoader());
                Toast.makeText(this, "got: " + c.getName(), Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
            }
        });

        bindV(R.id.bt_main_48, "Class.forName", () -> {
            try {
                ClassLoader classLoader = getClassLoader();
                classLoader.loadClass("java.lang.String");
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        });

        bindV(R.id.bt_main_48, "Class.forName", () -> {
            try {
                ClassLoader classLoader = getClassLoader();
                classLoader.loadClass("java.lang.String");
            } catch (ClassNotFoundException e) {
                throw new RuntimeException(e);
            }
        });

        // 48) ClassLoader.loadClass(String, boolean) —— 2参 + 返回 Class
        Button b48 = findViewById(R.id.bt_main_48);
        b48.setText("ClassLoader.loadClass(name,true)");
        b48.setOnClickListener(v -> {
            try {
                Class<?> c = getClassLoader().loadClass("java.lang.String");
                Toast.makeText(this, "got: " + c.getName(), Toast.LENGTH_SHORT).show();
            } catch (Throwable e) {
                Toast.makeText(this, "err: " + e, Toast.LENGTH_SHORT).show();
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


    // 放在 MainActivity 类里：两个小接口 & 绑定工具
    private interface Ret<T> {
        T get();
    }          // 有返回值

    private interface Throwing {
        void run() throws Throwable;
    } // 可能抛异常

    private void bindV(int id, String label, Runnable action) {
        Button b = findViewById(id);
        if (b != null) {
            b.setText(label);
            b.setOnClickListener(v -> {
                action.run();
                Toast.makeText(this, label, Toast.LENGTH_SHORT).show();
            });
        }
    }

    private <T> void bindR(int id, String label, Ret<T> action) {
        Button b = findViewById(id);
        if (b != null) {
            b.setText(label);
            b.setOnClickListener(v -> {
                T res = action.get();
                Toast.makeText(this, label + " -> " + String.valueOf(res), Toast.LENGTH_SHORT).show();
            });
        }
    }

    private void bindTryV(int id, String label, Throwing action) {
        Button b = findViewById(id);
        if (b != null) {
            b.setText(label);
            b.setOnClickListener(v -> {
                try {
                    action.run();
                    Toast.makeText(this, label, Toast.LENGTH_SHORT).show();
                } catch (Throwable t) {
                    Toast.makeText(this, label + " ! " +
                            t.getClass().getSimpleName() + ": " + t.getMessage(), Toast.LENGTH_SHORT).show();
                }
            });
        }
    }


}