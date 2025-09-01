package top.feadre.fhook.activitys;

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
            FHook.hook(fun_I_II).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setHookEnter(new FHook.HookEnterCallback() {
                @Override
                public void onEnter(HookParam param) throws Throwable {

                }
            }).setOrigFunRun(false);

        });

        Button bt_main_02 = findViewById(R.id.bt_main_02);
        bt_main_02.setText("hook初始化");
        bt_main_02.setOnClickListener(v -> {
            FHook.InitReport rep  = FHook.init(this);
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