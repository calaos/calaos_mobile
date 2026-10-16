package fr.calaos.calaoshome;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.widget.EditText;
import com.google.firebase.messaging.FirebaseMessaging;
import org.qtproject.qt.android.bindings.QtActivity;

// Native callbacks implemented in android/HardwareUtils_Android.cpp. They are
// registered by HardwareUtilsAndroid::platformInit() through
// QJniEnvironment::registerNativeMethods, so they only exist once Qt runs.
class HardwareUtilsNatives
{
    public static native void emitDialogTextValid(String text);
    public static native void emitDialogCancel();
    public static native void emitDeviceToken(String token);
    public static native void emitNotificationOpened(String eventUuid);
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

    // True once HardwareUtilsAndroid::platformInit() registered the methods of
    // HardwareUtilsNatives. Before that (and in a process started for a push
    // only) calling them throws UnsatisfiedLinkError.
    private static volatile boolean nativesReady;

    // Event uuid of the notification that started the activity, waiting for
    // Qt to come up and take it (takePendingEventUuid).
    private static volatile String pendingEventUuid;

    private static final int REQUEST_POST_NOTIFICATIONS = 0x4A10;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        // Set before super.onCreate(): that call starts the Qt application,
        // which may call the static helpers right away.
        _context = this;
        rememberEventUuid(getIntent());
        super.onCreate(savedInstanceState);

        CalaosMessagingService.ensureNotificationChannel(this);
        requestNotificationPermission();
    }

    // The activity is singleTop: a notification tapped while it exists lands
    // here instead of a second onCreate().
    @Override
    protected void onNewIntent(Intent intent)
    {
        super.onNewIntent(intent);
        setIntent(intent);
        rememberEventUuid(intent);
    }

    private void rememberEventUuid(Intent intent)
    {
        String uuid = eventUuidFromIntent(intent);
        if (uuid == null)
            return;

        if (nativesReady)
            notifyNotificationOpened(uuid);
        else
            pendingEventUuid = uuid;
    }

    // event_uuid comes either from our own PendingIntent
    // (CalaosMessagingService) or, for a message displayed by the system while
    // the app was in the background, from the FCM data keys that Play
    // services copies into the launcher intent. An intent replayed from
    // Recents must not reopen the event.
    static String eventUuidFromIntent(Intent intent)
    {
        if (intent == null)
            return null;
        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0)
            return null;

        String uuid = intent.getStringExtra(CalaosMessagingService.EXTRA_EVENT_UUID);
        if (uuid == null || uuid.isEmpty())
            return null;
        return uuid;
    }

    // Android 13+ only: below that, notifications need no runtime permission
    // (the manifest entry is ignored). The system stops showing the dialog by
    // itself after repeated denials.
    private void requestNotificationPermission()
    {
        if (Build.VERSION.SDK_INT < 33)
            return;
        if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED)
            return;

        requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, REQUEST_POST_NOTIFICATIONS);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
    {
        if (requestCode != REQUEST_POST_NOTIFICATIONS)
        {
            // Qt's own requests (QtAndroidPrivate::requestPermission)
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        Log.i(TAG, "POST_NOTIFICATIONS " + (granted ? "granted" : "denied"));
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

    // ── Push (called from HardwareUtilsAndroid, Qt thread) ─────────────────

    // Called right after registerNativeMethods(): from now on token refreshes
    // and notification taps are forwarded to C++ instead of being parked.
    public static void setNativesReady()
    {
        nativesReady = true;
    }

    // Token persisted by a previous run or by CalaosMessagingService.onNewToken
    // while Qt was not loaded. Empty string when there is none yet.
    public static String getStoredPushToken()
    {
        final Context ctx = _context;
        if (ctx == null)
            return "";
        return CalaosMessagingService.storedToken(ctx);
    }

    // Asks Firebase for the current token (cached, or freshly generated on
    // first run) and forwards it to C++ when it arrives, on the main thread.
    // FCM >= 25.1 deprecates getToken() in favour of register(), which hands
    // out a Firebase Installation ID for "direct-send" messaging. calaos_server
    // and the push.calaos.fr relay (gorush) address devices by the classic FCM
    // registration token, which only getToken()/onNewToken() provide, so the
    // deprecated pair stays until the relay side moves.
    @SuppressWarnings("deprecation")
    public static void requestPushToken()
    {
        final Context ctx = _context;
        if (ctx == null)
            return;

        try
        {
            FirebaseMessaging.getInstance().getToken().addOnCompleteListener(task ->
            {
                if (!task.isSuccessful() || task.getResult() == null || task.getResult().isEmpty())
                {
                    Log.w(TAG, "FCM getToken failed", task.getException());
                    return;
                }

                String token = task.getResult();
                Log.i(TAG, "FCM token obtained: " + CalaosMessagingService.describeToken(token));
                CalaosMessagingService.storeToken(ctx, token);
                deliverPushToken(token);
            });
        }
        catch (IllegalStateException e)
        {
            // FirebaseApp not initialized: google-services.json missing from
            // the build. Push is unavailable, the rest of the app works.
            Log.e(TAG, "Firebase is not initialized, push disabled", e);
        }
    }

    // Also used by CalaosMessagingService.onNewToken, from a binder thread.
    static void deliverPushToken(String token)
    {
        if (!nativesReady)
            return;

        try
        {
            HardwareUtilsNatives.emitDeviceToken(token);
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.w(TAG, "emitDeviceToken: native method not registered");
        }
    }

    // Event uuid the activity was started with, if any. Cleared on read.
    public static String takePendingEventUuid()
    {
        String uuid = pendingEventUuid;
        pendingEventUuid = null;
        return uuid == null ? "" : uuid;
    }

    private static void notifyNotificationOpened(String uuid)
    {
        try
        {
            HardwareUtilsNatives.emitNotificationOpened(uuid);
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.w(TAG, "emitNotificationOpened: native method not registered, keeping uuid");
            pendingEventUuid = uuid;
        }
    }
}
