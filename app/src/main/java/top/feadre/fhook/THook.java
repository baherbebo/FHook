package top.feadre.fhook;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.system.ErrnoException;
import android.system.Os;

import java.io.FileDescriptor;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Locale;
import java.util.Random;

import top.feadre.fhook.flibs.fsys.FLog;

/**
 * 测试类  各种类型的方法
 */
public class THook {
    private static final String TAG = FCFG_fhook.TAG_PREFIX + "THook";

    // ----------------------------  initAppBt01 ---------------------------------

    public int fun_I_III(int a, int b, int c) {
        int ret = a + b + c;
        FLog.d(TAG, "原方法输出 fun_I_III ... a=" + a + ", b=" + b + ", c=" + c + " -> ret=" + ret);
        return ret;
    }

    public String fun_String_String2(String a, String b) {
        String ret = a + b;
        FLog.d(TAG, "原方法输出 fun_String_String2 ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }

    public static int jtFun_I_II(int a, int b) {
        int ret = a + b;
        FLog.d(TAG, "原方法输出 jtFun_I_II ... a=" + a + ", b=" + b + " -> ret=" + ret);
        return ret;
    }


    // ----------------------------  initAppBt02 ---------------------------------


    public void fun_V_V() {
        FLog.d(TAG, "原方法输出 fun_V_V ...");
    }

    public static void jtFun_V_V() {
        FLog.d(TAG, "原方法输出 jtFun_V_V ...");
    }

    // ----------------------------  initAppBt03 ---------------------------------

    public TObject fun_TObject_TObject(TObject obj) {
        FLog.d(TAG, "原方法输出 fun_TObject_TObject ... in=" + obj);
        if (obj != null) {
            obj.setAge(obj.getAge() + 1).setName(obj.getName() + "_m");
        }
        FLog.d(TAG, "原方法输出 fun_TObject_TObject ... out=" + obj);
        return obj;
    }

    public int fun_I_IArr(int[] arr) {
        int sum = 0;
        if (arr != null) for (int v : arr) sum += v;
        FLog.d(TAG, "原方法输出 fun_I_IArr ... len="
                + (arr == null ? 0 : arr.length) + " -> sum=" + sum);
        return sum;
    }


    public int[] fun_IArr_DArr(double[] arr) {
        int[] ret = new int[arr.length];
        if (arr != null)
            for (int i = 0; i < arr.length; i++) {
                ret[i] = (int) arr[i] * 2;
            }
        FLog.d(TAG, "原方法输出 fun_IArr_DArr ... len=" + (arr == null ? 0 : arr.length));
        return ret;
    }

    public TObject[] fun_TObjectArr_DArr(double[] arr) {
        int sum = 0;
        if (arr != null) for (double v : arr) sum += v;
        FLog.d(TAG, "fun_TObjectArr_IArr ... len=" + (arr == null ? 0 : arr.length) + " -> sum=" + sum);
        TObject[] tObjects = new TObject[2];
        tObjects[0] = new TObject("_new_" + sum, sum);
        return tObjects;
    }

    public double fun_double_DArr(double[] arr) {
        double sum = 0.0;
        for (int i = 0; i < arr.length; i++) {
            sum += arr[i];
        }
        if (arr != null) for (double v : arr) FLog.d(TAG, "fun_double_DArr ... v=" + v);
        return sum;
    }

    public static long[] jtFun_JArr_JArr(long[] arr) {
        int sum = 0;
        for (int i = 0; i < arr.length; i++) {
            sum += arr[i];
        }
        if (arr != null) for (long v : arr) FLog.d(TAG, "jtFun_JArr_JArr ... v=" + v);
        arr[0] = sum;
        return arr;
    }

    /// 多出口 + try/throw 的方法 多个 early return try/catch/finally 改写结果 条件触发 throw
    public static int fun_fz01(int a, int b, String tag) {
        int res = 0;
        int pivot = (a ^ b) & 0xFF;
        int guard = (tag == null ? 0 : tag.length());

        // 早返回 #1：小输入直接返回
        if ((a | b) == 0) {
            return 7;
        }

        try {
            // 制造一堆局部变量（触发寄存器紧张）
            int x0 = a + 1, x1 = b + 2, x2 = pivot + 3, x3 = guard + 4;
            long l0 = ((long) x0 << 32) ^ x1;
            double d0 = Math.sqrt((x2 * 31.0) + x3);
            boolean f0 = (x0 & 1) == 0;
            String s0 = String.valueOf(d0);
            String s1 = String.format(Locale.US, "%s-%d", s0, pivot);

            res = (int) ((x0 * 13) ^ (x1 * 17) ^ (x2 * 19) ^ (x3 * 23));
            res += (int) (l0 ^ ((long) res << 1));
            if (f0) {
                // 早返回 #2：满足条件直接返回，但 finally 仍会改写
                return res ^ s1.hashCode();
            }

            // 条件 throw：测试异常路径
            if ((res & 3) == 1) {
                throw new IllegalStateException("multiExitTryThrow: synthetic error " + res);
            }

            // 早返回 #3
            if ((res & 7) == 0) {
                return res + 101;
            }

            // 正常路径返回 #4
            return res ^ 0x55AA55AA;

        } catch (RuntimeException re) {
            // 异常返回 #5
            res = (re.getMessage() == null ? -1001 : re.getMessage().length() * -37);
            return res - pivot;
        } finally {
            // finally 对结果做最后一步“扰动”
            res ^= (pivot * 131 + guard * 17 + 0xA5);
        }
    }


    ///  复杂方法：大量本地寄存器（> 20） 混合宽/窄/对象类型 循环、数组、MethodHandle、反射字段访问
    public static long fun_fz02(Context ctx, int n, ContentResolver cr) {
        // --- 构造大量局部变量 ---
        int a0 = n + 1, a1 = n + 2, a2 = n + 3, a3 = n + 4, a4 = n + 5;
        int a5 = n + 6, a6 = n + 7, a7 = n + 8, a8 = n + 9, a9 = n + 10;
        long l0 = (long) a0 * a1, l1 = (long) a2 * a3, l2 = (long) a4 * a5;
        long l3 = (long) a6 * a7, l4 = (long) a8 * a9;
        double d0 = Math.log1p(Math.abs(n) + 0.01), d1 = Math.exp((n & 7) * 0.1);
        double d2 = d0 * d1 + 0.12345, d3 = Math.hypot(d2, 3.14159);
        boolean b0 = (n & 1) == 0, b1 = (n & 2) != 0, b2 = (n & 4) != 0;
        String s0 = "x" + n, s1 = s0.toUpperCase(Locale.US);
        Object o0 = new Object();
        Object[] array = new Object[16]; // 占一些寄存器
        array[0] = ctx;
        array[1] = cr;
        array[2] = s0;
        array[3] = s1;
        array[4] = o0;
        array[5] = d3;

        // --- MethodHandle（防止轻易内联 & 增加寄存器转换开销）---
        try {
            MethodHandles.Lookup lk = MethodHandles.publicLookup();
            MethodType mt = MethodType.methodType(String.class, Object.class);
            MethodHandle mh = lk.findVirtual(Object.class, "toString", mt); // 其实不会被调用（准备好即算）
            if (mh != null && (n % 5 == 0)) {
                /*
                框架不支持签名多态/const-method-handle写回
                mh.invokeExact(...) 会在 DEX 里生成 invoke-polymorphic 指令
                 */
                String xs = (String) mh.invokeExact((Object) s0);
                array[6] = xs;
            }
        } catch (Throwable ignored) {
        }

        // --- 反射 Field 访问（触发额外寄存器&异常路径）---
        try {
            @SuppressLint("SoonBlockedPrivateApi") Field f = String.class.getDeclaredField("hash");
            f.setAccessible(true);
            int rawHash = f.getInt(s1); // JDK 内部字段（有些版本可能抛异常）
            a0 ^= rawHash;
        } catch (Throwable t) {
            a0 ^= (t.getClass().getName().hashCode());
        }

        // --- 算法/循环 ---
        long acc = 0;
        int len = Math.max(64, (n & 255) + 64);
        byte[] buf = new byte[len];
        new Random(n * 31L + 7).nextBytes(buf);

        for (int i = 0; i < buf.length; i++) {
            int v = (buf[i] & 0xFF);
            acc += ((long) v * (i + 1));
            if ((i & 7) == 0) {
                d3 += Math.sqrt(v + 1.0);
            } else if ((i & 7) == 1) {
                d3 -= Math.log(v + 2.0);
            } else if ((i & 7) == 2) {
                l0 ^= (long) v << (i % 13);
            } else if ((i & 7) == 3) {
                l1 ^= (long) v * (i + 17);
            } else if ((i & 7) == 4) {
                a1 += v;
            } else if ((i & 7) == 5) {
                a2 ^= v * 3;
            } else if ((i & 7) == 6) {
                b0 = !b0;
            } else {
                s0 = s0 + (char) ('a' + (v % 26));
            }
        }

        // --- 汇总 & 返回（宽寄存器）---
        long mix = (l0 ^ l1 ^ l2 ^ l3 ^ l4) + (long) (d0 * 1_000_000)
                + (b0 ? 13 : 17) + (b1 ? 19 : 23) + (b2 ? 29 : 31);
        mix ^= (long) s0.hashCode() * 65537L;
        mix += acc;
        return mix ^ ((long) a0 << 33) ^ ((long) a2 << 17) ^ ((long) a4);
    }


    /// 15+ 参数（宽居多）+ 返回宽寄存器 + 本地寄存器>20 参数中 long/double 占多数 返回 double 内部大量 locals
    public static double fun_fz03(
            long p1, double p2,
            long p3, double p4,
            long p5, double p6,
            long p7, double p8,
            long p9, double p10,
            long p11, double p12,
            long p13, double p14,
            // 补充几个窄寄存器参与搅拌
            int i1, int i2, float f1, float f2, int i3
    ) {
        // 20+ 局部
        double r0 = p2 + p4 + p6 + p8 + p10 + p12 + p14;
        long r1 = p1 ^ p3 ^ p5 ^ p7 ^ p9 ^ p11 ^ p13;
        int t0 = i1 + i2 + i3;
        float tf = f1 * f2;

        // 继续堆 locals
        long a0 = r1 + t0, a1 = a0 ^ p1, a2 = a1 ^ p3, a3 = a2 ^ p5, a4 = a3 ^ p7;
        long a5 = a4 ^ p9, a6 = a5 ^ p11, a7 = a6 ^ p13;
        double d0 = Math.sin(r0 + tf) + Math.cos(t0);
        double d1 = Math.hypot(d0, 3.0) * 0.25 + Math.ulp(r0);
        double d2 = Math.nextAfter(d1, Double.POSITIVE_INFINITY) + Math.scalb(r0, -3);
        double d3 = (Double.isFinite(d2) ? d2 : 0.0) + (a7 & 0xFFFF);

        // 对宽/窄做一轮混洗
        double mix = d3
                + (p1 & 0x7FFF) + p2 * 1e-3
                + (p3 & 0x7FFF) + p4 * 1e-3
                + (p5 & 0x7FFF) + p6 * 1e-3
                + (p7 & 0x7FFF) + p8 * 1e-3
                + (p9 & 0x7FFF) + p10 * 1e-3
                + (p11 & 0x7FFF) + p12 * 1e-3
                + (p13 & 0x7FFF) + p14 * 1e-3
                + t0 * 0.125 + tf * 0.03125;

        // 再创建一些对象 locals（触发寄存器对象槽）
        String s = String.format(Locale.US, "MIX:%.6f-%d", mix, t0);
        Object[] arr = new Object[8];
        arr[0] = s;
        arr[1] = mix;
        arr[2] = a7;
        arr[3] = d2;
        arr[4] = tf;
        arr[5] = i2;
        arr[6] = i3;
        arr[7] = Arrays.copyOf(s.getBytes(StandardCharsets.UTF_8), 8);

        // 返回宽寄存器 double
        return mix + s.hashCode() * 1e-6 + ((byte[]) arr[7])[0];
    }


    public static int h_fz014sys(String payload, int port) {
        FileDescriptor fd = null;
        int bytesSentTotal = 0;

        // 合法“魔数”
        final int MAGIC = 0xFEAD_BEEF;

        // 构造“复杂”报文内容：前导头 + 校验 + 原始数据 + 尾
        byte[] data = (payload == null) ? new byte[0] : payload.getBytes(StandardCharsets.UTF_8);
        ByteBuffer bb = ByteBuffer.allocate(Math.max(64, data.length + 32));
        bb.putInt(MAGIC);
        bb.putInt(data.length);
        int checksum = 0;
        for (byte b : data) checksum += (b & 0xFF);
        bb.putInt(checksum);
        bb.put(data);
        while (bb.position() % 4 != 0) bb.put((byte) 0); // 4 对齐
        bb.putInt(0xA55A_5AA5);
        bb.flip();
        byte[] packet = new byte[bb.remaining()];
        bb.get(packet);

        try {
            // 1) 创建 UDP socket
            fd = Os.socket(android.system.OsConstants.AF_INET,
                    android.system.OsConstants.SOCK_DGRAM,
                    android.system.OsConstants.IPPROTO_UDP);

            // 2) 尝试设置 SO_RCVTIMEO（避免阻塞）——用反射兜底，避免编译期依赖 StructTimeval
            trySetRecvTimeoutViaReflection(fd, /*millis*/200);

            // 3) 目标地址（127.0.0.1:port）
            InetAddress loop = InetAddress.getByName(null); // 127.0.0.1
            int flags1 = 0; // 第一次 flags = 0

            // —— 目标系统函数调用：Os.sendto(fd, bytes, offset, count, flags, addr, port) ——
            int sent1 = Os.sendto(fd, packet, 0, packet.length, flags1, loop, port);
            bytesSentTotal += Math.max(0, sent1);

            // 4) 再发送一次，改变 flags；并对数据做一点点变换（模拟另一种路径）
            if (packet.length >= 2) {
                packet[0] ^= 0x5A;
                packet[1] ^= 0xA5;
            }
            int flags2 = getOsConstSafely("MSG_DONTWAIT", /*fallback*/0); // 取不到就用 0
            int sent2 = Os.sendto(fd, packet, 0, packet.length, flags2, loop, port);
            bytesSentTotal += Math.max(0, sent2);

            // 5) 反射小插曲（无关紧要，但增加寄存器/异常路径）
            try {
                Method m = String.class.getDeclaredMethod("valueOf", Object.class);
                String probe = (String) m.invoke(null, "ok");
                bytesSentTotal ^= probe.hashCode();
            } catch (Throwable ignored) {
            }

        } catch (ErrnoException ee) {
            bytesSentTotal = -ee.errno;
        } catch (Throwable t) {
            // 捕获一切，防止影响宿主
            bytesSentTotal = (t.getClass().getName().hashCode() & 0x7FFFFFFF) * -1;
        } finally {
            try {
                if (fd != null) Os.close(fd);
            } catch (Throwable ignored) {
            }
        }
        return bytesSentTotal;
    }

    /**
     * 反射获取 OsConstants.<name>，取不到就返回 fallback。
     */
    private static int getOsConstSafely(String name, int fallback) {
        try {
            Class<?> c = Class.forName("android.system.OsConstants");
            Field f = c.getField(name);
            return f.getInt(null);
        } catch (Throwable ignored) {
            return fallback;
        }
    }

    /**
     * 反射调用 Os.setsockoptTimeval，避免直接依赖 StructTimeval 的编译期类型。
     */
    private static void trySetRecvTimeoutViaReflection(FileDescriptor fd, int millis) {
        try {
            // 反射构造 StructTimeval(sec, usec)
            Class<?> tvClz = Class.forName("android.system.StructTimeval");
            long sec = Math.max(0, millis / 1000);
            long usec = (millis % 1000) * 1000L;
            Constructor<?> ctor = tvClz.getConstructor(long.class, long.class);
            Object tv = ctor.newInstance(sec, usec);

            // 反射调用 Os.setsockoptTimeval(fd, SOL_SOCKET, SO_RCVTIMEO, tv)
            Class<?> osClz = Class.forName("android.system.Os");
            Class<?> osConstClz = Class.forName("android.system.OsConstants");
            int SOL_SOCKET = osConstClz.getField("SOL_SOCKET").getInt(null);
            int SO_RCVTIMEO = osConstClz.getField("SO_RCVTIMEO").getInt(null);

            Method m = osClz.getMethod("setsockoptTimeval", FileDescriptor.class, int.class, int.class, tvClz);
            m.invoke(null, fd, SOL_SOCKET, SO_RCVTIMEO, tv);
        } catch (Throwable ignored) {
            // 某些 ROM/编译环境不可用，直接忽略即可，不影响 sendto
        }
    }
}
