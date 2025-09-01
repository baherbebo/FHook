package top.feadre.fhook;

import java.lang.reflect.Executable;

public class HookHandle {
    final long nativeHandle;              // 由 native 返回的句柄
    final Executable method;
    volatile FHook.HookEnterCallback enterCb;
    volatile FHook.HookExitCallback exitCb;
    volatile boolean runOriginalByDefault = true;

    HookHandle(long h, Executable m) {
        this.nativeHandle = h; this.method = m;
    }

    public HookHandle setHookEnter(FHook.HookEnterCallback cb) {
        this.enterCb = cb;
//        FHook.nativeSetCallbacks(nativeHandle, cb, null);
        return this;
    }

    public HookHandle setHookExit(FHook.HookExitCallback cb) {
        this.exitCb = cb;
//        FHook.nativeSetCallbacks(nativeHandle, null, cb);
        return this;
    }

    /** 全局默认：是否执行原函数（可被 onEnter 动态覆盖：param.setSkipOriginal(true)） */
    public HookHandle setOrigFunRun(boolean run) {
        this.runOriginalByDefault = run;
//        FHook.nativeUpdateRunOriginal(nativeHandle, run);
        return this;
    }

    /** 撤销 hook */
    public void unhook() {
//        FHook.nativeUnhook(nativeHandle);
//        FHook.BRIDGE.drop(nativeHandle);
    }
}
