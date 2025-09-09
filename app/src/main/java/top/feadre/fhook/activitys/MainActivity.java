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

        Button bt_main_02 = findViewById(R.id.bt_main_02);
        bt_main_02.setText("02 初始化 hook");
        bt_main_02.setOnClickListener(v -> {

            mainActivityHelp.do_init_views();
            mainActivityHelp.do_init_hooks();

        });


    }

}