package top.feadre.fhook.flibs.fsys;

import android.content.Context;
import android.util.Log;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * 全局日志控制类（带文件大小限制）
 * 功能特点：
 * 1. 多级别日志控制（V/D/I/W/E）
 * 2. 自动文件轮转（最多3个文件）
 * 3. 单文件大小限制（默认5MB）
 * 4. 线程安全写入
 * 5. 自动生成TAG
 * <p>
 * // 初始化（建议在Application中调用）
 * FLog.init(context,  true);
 * <p>
 * // 记录日志
 * FLog.d("调试信息");
 * FLog.i("用户登录成功");
 * FLog.e("网络请求失败", exception);
 * <p>
 * // 动态配置
 * FLog.setLogLevel(BuildConfig.DEBUG  ? FLog.DEBUG : LogController.WARN);
 */
public class FLog {
    private static final String GLOBAL_PREFIX = "feadre";
    // 日志级别常量
    public static final int VERBOSE = 0;
    public static final int DEBUG = 1;
    public static final int INFO = 2;
    public static final int WARN = 3;
    public static final int ERROR = 4;

    // 默认配置
    private static int sLogLevel = DEBUG;
    private static boolean sLoggable = true;
    private static boolean sLogToFile = false;
    private static String sLogFilePath;
    private static final long MAX_FILE_SIZE = 5 * 1024 * 1024; // 5MB限制
    private static final int MAX_LOG_FILES = 3; // 最多3个日志文件

    private static final SimpleDateFormat TIME_FORMAT =
            new SimpleDateFormat("MM-dd HH:mm:ss.SSS", Locale.getDefault());
    private static final SimpleDateFormat FILE_FORMAT =
            new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault());

    private FLog() {
        throw new UnsupportedOperationException("工具类禁止实例化");
    }

    /**
     * 初始化日志系统 （建议在Application中调用）
     */
    public static void init(Context context, boolean enableFileLogging) {
        sLogToFile = enableFileLogging;
        if (enableFileLogging && context != null) {
            File logDir = context.getExternalFilesDir("logs");
            if (logDir != null && !logDir.exists()) {
                logDir.mkdirs();
            }
            sLogFilePath = logDir + File.separator + "app_" + FILE_FORMAT.format(new Date()) + ".log";
        }
    }

    // 自动生成TAG（调用类名）
    private static String generateTag() {
        StackTraceElement[] st = Thread.currentThread().getStackTrace();
        String cls = "Unknown";
        for (int i = 4; i < st.length; i++) {
            String c = st[i].getClassName();
            if (!c.equals(FLog.class.getName())) { cls = c; break; }
        }
        String simple = cls.substring(cls.lastIndexOf('.') + 1);
        return GLOBAL_PREFIX + "_" + simple;
    }

    /**
     * 日志文件轮转控制
     */
    private static synchronized void rotateLogs(File currentFile) {
        File parentDir = currentFile.getParentFile();
        File[] existingLogs = parentDir.listFiles((dir, name) -> name.startsWith("app_") && name.endsWith(".log"));

        if (existingLogs != null && existingLogs.length >= MAX_LOG_FILES) {
            // 按修改时间排序删除最旧的文件
            File oldest = existingLogs[0];
            for (File f : existingLogs) {
                if (f.lastModified() < oldest.lastModified()) {
                    oldest = f;
                }
            }
            oldest.delete();
        }
    }

    /**
     * 线程安全的日志写入方法
     * /storage/emulated/0/Android/data/<应用包名>/files/logs/
     * /storage/emulated/0/Android/data/top.feadre.flibs/files/logs/app_20250709_091823.log
     */
    private static synchronized void writeToFile(String level, String tag, String msg) {
        if (!sLogToFile || sLogFilePath == null) return;

        File logFile = new File(sLogFilePath);
        try {
            // 检查文件大小
            if (logFile.exists() && logFile.length() > MAX_FILE_SIZE) {
                String newName = "app_" + FILE_FORMAT.format(new Date()) + ".log";
                sLogFilePath = logFile.getParent() + File.separator + newName;
                logFile = new File(sLogFilePath);
                rotateLogs(logFile);
            }

            // 写入日志内容
            try (BufferedWriter writer = new BufferedWriter(new FileWriter(logFile, true))) {
                writer.write(String.format("%s  [%s] %s: %s\n",
                        TIME_FORMAT.format(new Date()),
                        level,
                        tag,
                        msg));
            }
        } catch (IOException e) {
            Log.e("LogController", "日志写入失败", e);
        }
    }

    // 公共日志方法
    public static void v(String msg) {
        if (sLoggable && sLogLevel <= VERBOSE) {
            String tag = generateTag();
            Log.v(tag, msg);
            writeToFile("V", tag, msg);
        }
    }

    public static void v(String tag, String msg) {
        if (sLoggable && sLogLevel <= VERBOSE) {
            Log.v(tag, msg);
            writeToFile("V", tag, msg);
        }
    }

    public static void d(String msg) {
        if (sLoggable && sLogLevel <= DEBUG) {
            String tag = generateTag();
            Log.d(tag, msg);
            writeToFile("D", tag, msg);
        }
    }

    public static void d(String tag, String msg) {
        if (sLoggable && sLogLevel <= DEBUG) {
            Log.d(tag, msg);
            writeToFile("D", tag, msg);
        }
    }

    public static void i(String msg) {
        if (sLoggable && sLogLevel <= INFO) {
            String tag = generateTag();
            Log.i(tag, msg);
            writeToFile("I", tag, msg);
        }
    }

    public static void i(String tag, String msg) {
        if (sLoggable && sLogLevel <= INFO) {
            Log.i(tag, msg);
            writeToFile("I", tag, msg);
        }
    }

    public static void w(String msg) {
        if (sLoggable && sLogLevel <= WARN) {
            String tag = generateTag();
            Log.w(tag, msg);
            writeToFile("W", tag, msg);
        }
    }

    public static void w(String tag, String msg) {
        if (sLoggable && sLogLevel <= WARN) {
            Log.w(tag, msg);
            writeToFile("W", tag, msg);
        }
    }

    public static void w(String tag, String msg, Throwable tr) {
        if (sLoggable && sLogLevel <= ERROR) {
            Log.w(tag, msg, tr);
            writeToFile("W", tag, msg + "\n" + Log.getStackTraceString(tr));
        }
    }

    public static void e(String msg) {
        if (sLoggable && sLogLevel <= ERROR) {
            String tag = generateTag();
            Log.e(tag, msg);
            writeToFile("E", tag, msg);
        }
    }

    public static void e(String tag, String msg) {
        if (sLoggable && sLogLevel <= ERROR) {
            Log.e(tag, msg);
            writeToFile("E", tag, msg);
        }
    }

    public static void e(String msg, Throwable tr) {
        if (sLoggable && sLogLevel <= ERROR) {
            String tag = generateTag();
            Log.e(tag, msg, tr);
            writeToFile("E", tag, msg + "\n" + Log.getStackTraceString(tr));
        }
    }

    public static void e(String tag, String msg, Throwable tr) {
        if (sLoggable && sLogLevel <= ERROR) {
            Log.e(tag, msg, tr);
            writeToFile("E", tag, msg + "\n" + Log.getStackTraceString(tr));
        }
    }

    // 配置方法
    public static void setLogLevel(int level) {
        sLogLevel = level;
    }

    public static void setEnabled(boolean enabled) {
        sLoggable = enabled;
    }
}