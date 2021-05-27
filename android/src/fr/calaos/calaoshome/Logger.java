package fr.calaos.calaoshome;

import android.util.Log;

public class Logger {
    public static final String TAG = "[LOGGER]fr.calaos.calaoshome";

    static void debug(String m) {
        Log.d(TAG, m);
    }

    static void error(String m) {
        Log.e(TAG, m);
    }

    static void verbose(String m) {
        Log.v(TAG, m);
    }

    static void warning(String m) {
        Log.w(TAG, m);
    }
}
