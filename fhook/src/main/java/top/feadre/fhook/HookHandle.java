package top.feadre.fhook;

import java.lang.reflect.Method;

import top.feadre.fhook.flibs.fsys.FLog;

public class HookHandle {
    private static final String TAG = FCFG.TAG_PREFIX + "HookHandle";

    public Object thisObject;
    boolean isHooked = false; // native 已安装
    long nativeHandle;              // 由 native 返回的句柄
    Method method;
    boolean disabledByPrecheck; //预检失败/禁止安装 标志
    volatile FHook.HookEnterCallback enterCb;
    volatile FHook.HookExitCallback exitCb;
    volatile boolean runOriginalByDefault = true;

    // 用于带各种数据
    public final java.util.concurrent.ConcurrentHashMap<String, Object> extras =
            new java.util.concurrent.ConcurrentHashMap<>();

    HookHandle(long h, Method m) {
        this.nativeHandle = h;
        this.method = m;
    }

    HookHandle markNotHooked() {
        this.disabledByPrecheck = true;   // ★ 标记为禁用
        this.isHooked = false;
        this.nativeHandle = -1;
        return this;
    }


    @SuppressWarnings("unchecked")
    public <T> T getExtra(String key) {
        return (T) extras.get(key);
    }

    public void putExtra(String key, Object val) {
        extras.put(key, val);
    }

    public void removeExtra(String key) {
        extras.remove(key);
    }

    public boolean isHooked() {
        return isHooked;
    }

    public void setHooked(boolean hooked) {
        this.isHooked = hooked;
    }

    public Method getMethod() {
        return method;
    }

    public HookHandle setHookEnter(FHook.HookEnterCallback cb) {
        this.enterCb = cb;
        return this;
    }

    public HookHandle setHookExit(FHook.HookExitCallback cb) {
        this.exitCb = cb;
        return this;
    }

    /**
     * 全局默认：是否执行原函数（可被 onEnter 动态覆盖：param.setSkipOriginal(true)）
     */
    public HookHandle setOrigFunRun(boolean run) {
        this.runOriginalByDefault = run;
        return this;
    }

    public synchronized HookHandle commit() {
        if (disabledByPrecheck) {
            FLog.e(TAG, "[commit] skip: precheck failed, not installing -> " + method);
            return this;
        }
        FHook.installOnce(this);          // 只安装一次
        return this;
    }

    /**
     * 撤销 hook
     */
    public void unhook() {
        if (!isHooked) return;

        // 这里需要
        FHook.unHook(nativeHandle);
        isHooked = false;
    }
}
