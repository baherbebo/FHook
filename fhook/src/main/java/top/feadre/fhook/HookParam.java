package top.feadre.fhook;


import java.lang.reflect.Executable;

public final class HookParam {
    public final Executable method;
    public final Object thisObject;   // static 方法时为 null
    public Object[] args;             // 可在 onEnter 中修改

    // 由回调设置
    private boolean skipOriginal;     // 若设为 true，则不执行原函数
    public Object result;             // 可在 onExit（或 onEnter+skip）里设置返回值
    public Throwable throwable;       // 捕获原函数异常（可改写/置空）

    HookParam(Executable m, Object thiz, Object[] a) {
        this.method = m; this.thisObject = thiz; this.args = a;
    }
    public void setSkipOriginal(boolean v) { this.skipOriginal = v; }
    public boolean isSkipOriginal() { return skipOriginal; }
}


