package fr.calaos.calaoshome;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;
import org.qtproject.qt.android.bindings.QtActivity;

// Native callbacks implemented in android/HardwareUtils_Android.cpp. They are
// registered by HardwareUtilsAndroid::platformInit() through
// QJniEnvironment::registerNativeMethods, so they only exist once Qt runs.
class HardwareUtilsNatives
{
    public static native void emitDialogTextValid(String text);
    public static native void emitDialogCancel();
}

public class HardwareUtils extends QtActivity
{
    private static final String TAG = "CalaosHome";

    // Must match HardwareUtils::NetworkStatus in src/HardwareUtils.h
    private static final int NETWORK_NOT_CONNECTED = 0;
    private static final int NETWORK_WWAN = 1;
    private static final int NETWORK_WIFI = 2;

    // The running activity. Null before onCreate() and after onDestroy(); it
    // is read from the Qt thread, hence volatile. Static helpers must copy it
    // to a local and check it before use.
    private static volatile HardwareUtils _context;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        // Set before super.onCreate(): that call starts the Qt application,
        // which may call the static helpers right away.
        _context = this;
        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onDestroy()
    {
        if (_context == this)
            _context = null;
        super.onDestroy();
    }

    private static boolean isUsable(Activity activity)
    {
        return activity != null && !activity.isFinishing() && !activity.isDestroyed();
    }

    public static void showAlertMessage(final String title, final String message, final String buttonText)
    {
        final HardwareUtils activity = _context;
        if (!isUsable(activity))
        {
            Log.w(TAG, "showAlertMessage: no activity, alert dropped");
            return;
        }

        activity.runOnUiThread(new Runnable()
        {
            public void run()
            {
                if (!isUsable(activity))
                    return;

                new AlertDialog.Builder(activity)
                    .setTitle(title)
                    .setMessage(message)
                    .setIcon(R.drawable.icon)
                    .setPositiveButton(buttonText, new DialogInterface.OnClickListener()
                    {
                        public void onClick(DialogInterface dialog, int which)
                        {
                            dialog.dismiss();
                        }
                    })
                    .show();
            }
        });
    }

    // Returns NETWORK_WIFI for Wi-Fi/Ethernet, NETWORK_WWAN for cellular (and
    // any other connected transport), NETWORK_NOT_CONNECTED when there is no
    // default network (airplane mode, ...).
    public static int getNetworkStatus()
    {
        final Context ctx = _context;
        if (ctx == null)
            return NETWORK_WWAN; // We don't know. Say it's connected.

        try
        {
            ConnectivityManager cm = (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
            if (cm == null)
                return NETWORK_WWAN;

            Network network = cm.getActiveNetwork();
            if (network == null)
                return NETWORK_NOT_CONNECTED;

            NetworkCapabilities caps = cm.getNetworkCapabilities(network);
            if (caps == null)
                return NETWORK_NOT_CONNECTED;

            if (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) ||
                caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET))
                return NETWORK_WIFI;

            return NETWORK_WWAN;
        }
        catch (SecurityException e)
        {
            Log.e(TAG, "getNetworkStatus: ACCESS_NETWORK_STATE denied", e);
            return NETWORK_WWAN;
        }
    }

    // Returns false when no dialog can be shown; the caller then reports a
    // cancel itself so the QML side does not wait for an answer forever.
    public static boolean inputtextDialog(final String title, final String message)
    {
        final HardwareUtils activity = _context;
        if (!isUsable(activity))
        {
            Log.w(TAG, "inputtextDialog: no activity");
            return false;
        }

        activity.runOnUiThread(new Runnable()
        {
            public void run()
            {
                if (!isUsable(activity))
                {
                    notifyDialogCancel();
                    return;
                }

                // Set an EditText view to get user input
                final EditText input = new EditText(activity);

                new AlertDialog.Builder(activity)
                    .setTitle(title)
                    .setMessage(message)
                    .setView(input)
                    .setIcon(R.drawable.icon)
                    .setPositiveButton("Confirm", new DialogInterface.OnClickListener()
                    {
                        public void onClick(DialogInterface dialog, int which)
                        {
                            notifyDialogTextValid(input.getText().toString());
                            dialog.dismiss();
                        }
                    })
                    .setNegativeButton("Cancel", new DialogInterface.OnClickListener()
                    {
                        public void onClick(DialogInterface dialog, int which)
                        {
                            notifyDialogCancel();
                            dialog.dismiss();
                        }
                    })
                    // Back key or tap outside the dialog
                    .setOnCancelListener(new DialogInterface.OnCancelListener()
                    {
                        public void onCancel(DialogInterface dialog)
                        {
                            notifyDialogCancel();
                        }
                    })
                    .show();
            }
        });

        return true;
    }

    private static void notifyDialogTextValid(String text)
    {
        try
        {
            HardwareUtilsNatives.emitDialogTextValid(text);
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.e(TAG, "emitDialogTextValid: native method not registered", e);
        }
    }

    private static void notifyDialogCancel()
    {
        try
        {
            HardwareUtilsNatives.emitDialogCancel();
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.e(TAG, "emitDialogCancel: native method not registered", e);
        }
    }
}
