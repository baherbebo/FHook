package top.feadre.fhook.tools;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.Toast;

public class FFunsUI {
    private static final String TAG = "feadre_" + "FFunsUI";

    private static Toast previousToast;


    private static void showToast(Context context, String text, int duration, boolean cancelPrevious) {
        // 检查是否需要取消前一个Toast
        if (cancelPrevious && previousToast != null) {
            previousToast.cancel();
        }

        // 创建新的Toast
        previousToast = Toast.makeText(context, text, duration);
        previousToast.show();
    }

    public static void toast(Context context, String text) {
        showToast(context, text, Toast.LENGTH_SHORT, true);
    }

    public static void toast(Context context, String text, boolean cancelPrevious) {
        showToast(context, text, Toast.LENGTH_SHORT, cancelPrevious);
    }

    public static void toast_long(Context context, String text) {
        showToast(context, text, Toast.LENGTH_LONG, true);
    }

    public static void toast_long(Context context, String text, boolean cancelPrevious) {
        showToast(context, text, Toast.LENGTH_LONG, cancelPrevious);
    }

    public static int dpToPx(Context context, int dp) {
        return (int) (dp * context.getResources().getDisplayMetrics().density);
    }

    /**
     * @param context
     * @param view
     * @param size_text 按钮文字的大小 英文 / 1.5
     */
    public static void set_view_size(Context context, View view, int size_text) {
        set_view_size(context, view, size_text * 15 + 10, 35); //正常按钮的高度 35
    }

    public static void set_view_size(Context context, View view, int w_dp, int h_dp) {
        int widthPx = FFunsUI.dpToPx(context, w_dp);
        int heightPx = FFunsUI.dpToPx(context, h_dp);
        LinearLayout.LayoutParams btnParams = new LinearLayout.LayoutParams(widthPx, heightPx);
        view.setPadding(0, 0, 0, 0);
        view.setLayoutParams(btnParams);
//        bt_indate.setMinHeight(0);  // 移除默认的最小高度
//        bt_indate.setMinWidth(0);   // 移除默认的最小宽度
//        bt_indate.requestLayout(); // 重新请求
//        btnParams.gravity = Gravity.NO_GRAVITY; // 避免父布局对齐影响
//        bt_indate.setBackground(null); // 移除背景
    }

}
