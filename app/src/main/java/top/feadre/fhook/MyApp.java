package top.feadre.fhook;

import android.app.Application;
import android.content.Context;
import android.util.Log;

public class MyApp extends Application {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "MyApp";

    @Override
    protected void attachBaseContext(Context base) {
        Log.d(TAG, "attachBaseContext");
        Log.i(TAG, "attachBaseContext FHook.init= " + FHook.init(base));

        super.attachBaseContext(base);
    }

    @Override
    public void onCreate() {
        super.onCreate();
    }
}
