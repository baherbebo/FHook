package top.feadre.fhook.activitys;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;


import top.feadre.fhook.FCFG_fhook;
import top.feadre.fhook.R;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("ft001");
    }

    private static final String TAG = FCFG_fhook.TAG_PREFIX + "MainActivity";

    public native int jcFt001T01(int a, int b);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
//        EdgeToEdge.enable(this); // 沉浸式
        setContentView(R.layout.activity_main);

        Log.i("Ft001", "jcFt001T01: ----------------" + jcFt001T01(1, 2));

        MainActivityHelp mainActivityHelp = new MainActivityHelp(this);

        Button bt_main_01 = findViewById(R.id.bt_main_01);
        bt_main_01.setText("01 启动 DebugSample");
        bt_main_01.setOnClickListener(v -> {

            Intent intent = new Intent(this, DebugSample.class);
            startActivity(intent);

        });

        mainActivityHelp.do_init_views();


        Button bt_main_02 = findViewById(R.id.bt_main_02);
        bt_main_02.setText("02 初始化 FAndroidHook");
        bt_main_02.setOnClickListener(v -> {

            mainActivityHelp.do_init_hooks();

        });

        init_views();
    }

    private void init_views() {
        // 03 触发 System.exit(123)（若 hook 正常会被拦截）
        Button bt_main_03 = findViewById(R.id.bt_main_03);
        bt_main_03.setText("03 触发 System.exit(123)");
        bt_main_03.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger System.exit(123)");
            try {
                System.exit(123); // 你的 hook_System_exit() 已 setOrigFunRun(false)，这里不会真的退
            } catch (Throwable t) {
                Log.e(TAG, "System.exit trigger error", t);
            }
        });

// 04 触发 Runtime.exec("id")
        Button bt_main_04 = findViewById(R.id.bt_main_04);
        bt_main_04.setText("04 触发 Runtime.exec(\"id\")");
        bt_main_04.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger Runtime.exec(\"id\")");
            try {
                Process p = Runtime.getRuntime().exec("id");
                // 简单读一行输出，避免阻塞
                new Thread(() -> {
                    try (java.io.BufferedReader r =
                                 new java.io.BufferedReader(new java.io.InputStreamReader(p.getInputStream()))) {
                        String line = r.readLine();
                        Log.i(TAG, "[exec out] " + line);
                    } catch (Exception ignore) {
                    }
                }).start();
            } catch (Throwable t) {
                Log.e(TAG, "exec trigger error", t);
            }
        });

// 05 触发 System.loadLibrary("log")
        Button bt_main_05 = findViewById(R.id.bt_main_05);
        bt_main_05.setText("05 触发 System.loadLibrary(\"log\")");
        bt_main_05.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger System.loadLibrary(\"log\")");
            try {
                System.loadLibrary("log"); // 常驻系统库，基本都在
                android.widget.Toast.makeText(this, "loadLibrary(\"log\") ok", android.widget.Toast.LENGTH_SHORT).show();
            } catch (UnsatisfiedLinkError e) {
                android.widget.Toast.makeText(this, "loadLibrary failed: " + e.getMessage(), android.widget.Toast.LENGTH_SHORT).show();
                Log.e(TAG, "loadLibrary error", e);
            }
        });

// 06 触发 SystemProperties.get("ro.build.version.release")
        Button bt_main_06 = findViewById(R.id.bt_main_06);
        bt_main_06.setText("06 触发 SystemProperties.get(版本号)");
        bt_main_06.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger SystemProperties.get(\"ro.build.version.release\")");
            try {
                Class<?> sp = Class.forName("android.os.SystemProperties");
                java.lang.reflect.Method get = sp.getDeclaredMethod("get", String.class);
                get.setAccessible(true);
                String val = (String) get.invoke(null, "ro.build.version.release");
                android.widget.Toast.makeText(this, "release=" + val, android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                Log.e(TAG, "SystemProperties.get trigger error", t);
            }
        });

// 07 触发 Settings.Secure.getString(..., ANDROID_ID)
        Button bt_main_07 = findViewById(R.id.bt_main_07);
        bt_main_07.setText("07 触发 ANDROID_ID 读取");
        bt_main_07.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger Settings.Secure.getString ANDROID_ID");
            try {
                String id = android.provider.Settings.Secure.getString(
                        getContentResolver(), android.provider.Settings.Secure.ANDROID_ID);
                android.widget.Toast.makeText(this, "ANDROID_ID=" + id, android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                Log.e(TAG, "Settings.Secure.getString trigger error", t);
            }
        });

        // 08 触发 Class.forName("java.lang.String")
        Button bt_main_08 = findViewById(R.id.bt_main_08);
        bt_main_08.setText("08 触发 Class.forName(\"java.lang.String\")");
        bt_main_08.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger Class.forName(\"java.lang.String\")");
            try {
                Class<?> c = Class.forName("java.lang.String");
                android.widget.Toast.makeText(this, "loaded: " + c.getName(), android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                Log.e(TAG, "Class.forName trigger error", t);
            }
        });

// 09 触发 Process.killProcess(myPid()) —— 注意：先点“初始化 hook”
        Button bt_main_09 = findViewById(R.id.bt_main_09);
        bt_main_09.setText("09 触发 killProcess(myPid())（需已初始化Hook）");
        bt_main_09.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger Process.killProcess(myPid())");
            try {
                int pid = android.os.Process.myPid();
                android.os.Process.killProcess(pid); // hook 后会被拦截
                android.widget.Toast.makeText(this, "killProcess(" + pid + ") called", android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                Log.e(TAG, "killProcess trigger error", t);
            }
        });

// 10 触发 Debug.isDebuggerConnected()
        Button bt_main_10 = findViewById(R.id.bt_main_10);
        bt_main_10.setText("10 触发 Debug.isDebuggerConnected()");
        bt_main_10.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger Debug.isDebuggerConnected()");
            try {
                boolean connected = android.os.Debug.isDebuggerConnected();
                android.widget.Toast.makeText(this, "isDebuggerConnected=" + connected, android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                Log.e(TAG, "isDebuggerConnected trigger error", t);
            }
        });

        // 11 触发 MessageDigest.getInstance(\"MD5\") 并做一次摘要
        Button bt_main_11 = findViewById(R.id.bt_main_11);
        bt_main_11.setText("11 触发 MessageDigest.MD5(\"abc\")");
        bt_main_11.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger MessageDigest.getInstance(\"MD5\")");
            try {
                java.security.MessageDigest md = java.security.MessageDigest.getInstance("MD5");
                byte[] out = md.digest("abc".getBytes(java.nio.charset.StandardCharsets.UTF_8));
                StringBuilder sb = new StringBuilder();
                for (byte b : out) sb.append(String.format("%02x", b));
                android.widget.Toast.makeText(this, "md5(abc)=" + sb, android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                Log.e(TAG, "MessageDigest trigger error", t);
            }
        });

    // 12 触发 Cipher.getInstance(\"AES/CBC/PKCS5Padding\")
        Button bt_main_12 = findViewById(R.id.bt_main_12);
        bt_main_12.setText("12 触发 Cipher.getInstance(\"AES/CBC/PKCS5Padding\")");
        bt_main_12.setOnClickListener(v -> {
            Log.i(TAG, "[UI] trigger Cipher.getInstance(\"AES/CBC/PKCS5Padding\")");
            try {
                javax.crypto.Cipher c = javax.crypto.Cipher.getInstance("AES/CBC/PKCS5Padding");
                android.widget.Toast.makeText(this, "cipher=" + c.getAlgorithm(), android.widget.Toast.LENGTH_SHORT).show();
            } catch (Throwable t) {
                android.widget.Toast.makeText(this, "cipher error: " + t.getMessage(), android.widget.Toast.LENGTH_SHORT).show();
                Log.e(TAG, "Cipher trigger error", t);
            }
        });


    }

}