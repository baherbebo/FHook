package top.feadre.fhook;

import java.lang.reflect.Method;

public class HookHandle {
    Object thisObject;
    boolean isHooked = false; // native 已安装
    long nativeHandle;              // 由 native 返回的句柄
    Method method;
    volatile FHook.HookEnterCallback enterCb;
    volatile FHook.HookExitCallback exitCb;
    volatile boolean runOriginalByDefault = true;

    HookHandle(long h, Method m) {
        this.nativeHandle = h;
        this.method = m;
    }

    HookHandle markNotHooked() { this.isHooked = false; return this; }

    public void setThisObject(Object thisObject) {
        this.thisObject = thisObject;
    }

    public Object getThisObject() {
        return thisObject;
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
        FHook.installOnce(this);          // 只安装一次
        return this;
    }

    /**
     * 撤销 hook
     */
    public void unhook() {
        if (!isHooked) return;

        // 这里需要
        FHook.unregister(nativeHandle);
        isHooked = false;
    }
}
