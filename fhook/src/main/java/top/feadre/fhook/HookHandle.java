package top.feadre.fhook;

import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

import top.feadre.fhook.flibs.fsys.FLog;

public class HookHandle {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "HookHandle";

    public Object thisObject; // 这个由框架自动设置
    boolean isHooked = false; // native 已安装
    long nativeHandle;              // 由 native 返回的句柄
    public final Executable method;     // 可是 Method 或 Constructor
    public final Class<?> retType;    // 返回类型（构造器是 void.class）
    public final Class<?>[] paramTypes; // 参数类型
    public final boolean isStatic;   // 构造器恒为 false

    boolean disabledByPrecheck; //预检失败/禁止安装 标志
    volatile FHook.HookEnterCallback enterCb;
    volatile FHook.HookExitCallback exitCb;
    volatile boolean runOriginalByDefault = true; // 默认是是

    // 用于带各种数据 *****
    public final ConcurrentHashMap<String, Object> extras = new ConcurrentHashMap<>();

    HookHandle(long h, Method m) {
        this.nativeHandle = h;
        this.method = m;
        this.paramTypes = m.getParameterTypes();
        this.retType = m.getReturnType();
        this.isStatic = Modifier.isStatic(m.getModifiers());
    }

    HookHandle(Constructor<?> c) {
        this.method = c;
        this.paramTypes = c.getParameterTypes();
        this.retType = void.class;      // 构造器没有返回值
        this.isStatic = false;          // 构造器一定不是 static
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

    public Executable getMethod() {
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

    /**
     * FHook.hook(method)
     *      .setHookEnter(...)
     *      .setHookExit(...)
     *      .commitAsync(success -> FLog.i("TAG", "single hook installed: " + success));
     * @param cb
     * @return
     */
    public HookHandle commitAsync(FHook.OnHookFinish cb) {
        return commitAsync(Executors.newSingleThreadExecutor(), cb);
    }

    public HookHandle commitAsync(Executor executor, FHook.OnHookFinish cb) {
        if (disabledByPrecheck) {
            FLog.e(TAG, "[commitAsync] 失败不能提交 -> " + method);
            if (cb != null) cb.onFinish(false);
            return this;
        }
        executor.execute(() -> {
            try {
                FHook.installOnce(this); // 只安装一次
                boolean ok = this.isHooked && this.nativeHandle > 0;
                if (cb != null) cb.onFinish(ok);
            } catch (Throwable t) {
                FLog.e(TAG, "[commitAsync] install exception -> " + method, t);
                if (cb != null) cb.onFinish(false);
            }
        });
        return this;
    }

    public  HookHandle commit() {
        if (disabledByPrecheck) {
            FLog.e(TAG, "[commit] 失败不能提交 -> " + method);
            return this;
        }
        FHook.installOnce(this);  // 只安装一次
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
