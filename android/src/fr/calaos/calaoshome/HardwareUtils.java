package fr.calaos.calaoshome;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.KeyguardManager;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Color;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.PersistableBundle;
import android.provider.Settings;
import android.security.keystore.KeyPermanentlyInvalidatedException;
import android.util.Log;
import android.view.WindowManager;
import android.view.WindowInsetsController;
import android.widget.EditText;
import com.google.firebase.messaging.FirebaseMessaging;
import org.qtproject.qt.android.bindings.QtActivity;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Locale;
import java.util.concurrent.Executor;
import javax.crypto.Cipher;

// Native callbacks implemented in android/HardwareUtils_Android.cpp. They are
// registered by HardwareUtilsAndroid::platformInit() through
// QJniEnvironment::registerNativeMethods, so they only exist once Qt runs.
class HardwareUtilsNatives
{
    public static native void emitDialogTextValid(String text);
    public static native void emitDialogCancel();
    public static native void emitDeviceToken(String token);
    public static native void emitNotificationOpened(String eventUuid);
    public static native void emitAdminCodeNotified(String codeId);
    public static native void emitBiometricStoreDone(String key, int result);
    public static native void emitBiometricReadDone(String key, int result, String secret);
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

    // Same for an admin code notification tapped before Qt came up.
    private static volatile String pendingAdminCodeId;

    private static final int REQUEST_POST_NOTIFICATIONS = 0x4A10;

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        // Set before super.onCreate(): that call starts the Qt application,
        // which may call the static helpers right away.
        _context = this;
        // Only a genuine launch: when the system recreates the activity after
        // process death it replays the original intent (without
        // FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY), which must not reopen the event.
        if (savedInstanceState == null)
            rememberEventUuid(getIntent());
        super.onCreate(savedInstanceState);

        // Edge to edge (Android 15+), the 3-button navigation bar gets a light
        // translucent scrim over the app by default. The app draws its own
        // dark menu bar under it: let the bar stay transparent.
        if (Build.VERSION.SDK_INT >= 29)
            getWindow().setNavigationBarContrastEnforced(false);
        useLightNavigationBarIcons();

        CalaosMessagingService.ensureNotificationChannel(this);
        requestNotificationPermission();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);
        // Qt applies the system light/dark mode to the system bar icons here
        useLightNavigationBarIcons();
    }

    // From Android 15 the navigation bar is transparent over the app, which is
    // dark whatever the system theme. Qt follows the system theme for the
    // navigation bar icons (the status bar follows the app's dark colour
    // scheme, see Application.cpp): in a light system theme the 3-button
    // navigation icons came out grey on our dark menu bar.
    @SuppressWarnings("deprecation") // setNavigationBarColor, still read by the system
    private void useLightNavigationBarIcons()
    {
        if (Build.VERSION.SDK_INT < 35)
            return;
        // The system also forces dark icons while the requested bar colour is
        // light (Qt sets the light theme's one). Edge to edge the colour is not
        // drawn, it only drives the icons: give the app's dark background (with
        // a transparent colour the icons came out dimmed on the API 36
        // emulator).
        getWindow().setNavigationBarColor(Color.rgb(8, 8, 8));
        WindowInsetsController controller = getWindow().getInsetsController();
        if (controller != null)
            controller.setSystemBarsAppearance(0, WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);
        // Qt sets the system bars up again once its window is shown
        if (hasFocus)
            useLightNavigationBarIcons();
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
        String codeId = adminCodeIdFromIntent(intent);
        if (codeId != null)
        {
            if (nativesReady)
                notifyAdminCode(codeId);
            else
                pendingAdminCodeId = codeId;
            return;
        }

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
    // Recents must not reopen the event. The activity is exported: the extra
    // is untrusted (any app can send it), so it is validated and a hostile
    // bundle that fails to unparcel is treated as no uuid.
    static String eventUuidFromIntent(Intent intent)
    {
        if (intent == null)
            return null;
        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0)
            return null;

        String uuid;
        try
        {
            uuid = intent.getStringExtra(CalaosMessagingService.EXTRA_EVENT_UUID);
        }
        catch (RuntimeException e) // BadParcelableException, ClassCastException, ...
        {
            Log.w(TAG, "Ignoring unreadable launch extras");
            return null;
        }
        return CalaosMessagingService.sanitizeEventUuid(uuid);
    }

    // Same rules as eventUuidFromIntent(). The id only tells the app to read
    // the stash: a forged one shows nothing.
    static String adminCodeIdFromIntent(Intent intent)
    {
        if (intent == null)
            return null;
        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0)
            return null;

        try
        {
            return CalaosMessagingService.sanitizeAdminCodeId(
                intent.getStringExtra(CalaosMessagingService.EXTRA_ADMIN_CODE_ID));
        }
        catch (RuntimeException e)
        {
            return null;
        }
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

    // The name the user gave the phone in the system settings (no permission
    // needed); devices that do not expose it get "<manufacturer> <model>".
    public static String getDeviceName()
    {
        final Context ctx = _context;
        if (ctx != null)
        {
            try
            {
                String name = Settings.Global.getString(ctx.getContentResolver(),
                                                        Settings.Global.DEVICE_NAME);
                if (name != null && !name.trim().isEmpty())
                    return name;
            }
            catch (SecurityException e)
            {
                Log.w(TAG, "getDeviceName: DEVICE_NAME not readable", e);
            }
        }

        final String model = Build.MODEL == null ? "" : Build.MODEL;
        String manufacturer = Build.MANUFACTURER == null ? "" : Build.MANUFACTURER;
        // Some vendors already start MODEL with the brand ("Xiaomi Redmi 9").
        if (manufacturer.isEmpty() ||
            model.toLowerCase(Locale.ROOT).startsWith(manufacturer.toLowerCase(Locale.ROOT)))
            return model;
        // Some vendors report it in lower case ("samsung").
        manufacturer = manufacturer.substring(0, 1).toUpperCase(Locale.ROOT) + manufacturer.substring(1);
        return manufacturer + " " + model;
    }

    // Returns false when no dialog can be shown; the caller then reports a
    // cancel itself so the QML side does not wait for an answer forever.
    public static boolean inputtextDialog(final String title, final String message,
                                         final String confirmText, final String cancelText)
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
                    .setPositiveButton(confirmText, new DialogInterface.OnClickListener()
                    {
                        public void onClick(DialogInterface dialog, int which)
                        {
                            notifyDialogTextValid(input.getText().toString());
                            dialog.dismiss();
                        }
                    })
                    .setNegativeButton(cancelText, new DialogInterface.OnClickListener()
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

    // Values of AdminBiometrics::Result.
    private static final int BIOMETRIC_OK = 0;
    private static final int BIOMETRIC_CANCELED = 1;
    private static final int BIOMETRIC_FAILED = 2;
    private static final int BIOMETRIC_INVALIDATED = 3;

    public static boolean biometricsAvailable()
    {
        HardwareUtils activity = _context;
        if (activity == null)
            return false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
        {
            BiometricManager bm = activity.getSystemService(BiometricManager.class);
            return bm != null &&
                   bm.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_STRONG) == BiometricManager.BIOMETRIC_SUCCESS;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
        {
            BiometricManager bm = activity.getSystemService(BiometricManager.class);
            return bm != null && bm.canAuthenticate() == BiometricManager.BIOMETRIC_SUCCESS;
        }
        // API 28 has no way to ask without the deprecated fingerprint service.
        KeyguardManager km = activity.getSystemService(KeyguardManager.class);
        return km != null && km.isDeviceSecure() &&
               activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_FINGERPRINT);
    }

    private static void authenticate(final Activity activity, Cipher cipher, String title, String cancelText,
                                      final BiometricPrompt.AuthenticationCallback callback, final Runnable canceled)
    {
        Executor executor = activity.getMainExecutor();
        BiometricPrompt prompt = new BiometricPrompt.Builder(activity)
            .setTitle(title)
            .setNegativeButton(cancelText, executor, new DialogInterface.OnClickListener()
            {
                public void onClick(DialogInterface dialog, int which)
                {
                    canceled.run();
                }
            })
            .build();
        prompt.authenticate(new BiometricPrompt.CryptoObject(cipher), new CancellationSignal(), executor, callback);
    }

    private static boolean isCancel(int errorCode)
    {
        return errorCode == BiometricPrompt.BIOMETRIC_ERROR_CANCELED ||
               errorCode == BiometricPrompt.BIOMETRIC_ERROR_USER_CANCELED;
    }

    public static void biometricStore(final String key, final String secret, final String title, final String cancelText)
    {
        final HardwareUtils activity = _context;
        if (!isUsable(activity))
        {
            notifyBiometricStoreDone(key, BIOMETRIC_FAILED);
            return;
        }
        activity.runOnUiThread(new Runnable()
        {
            @Override
            public void run()
            {
                try
                {
                    Cipher cipher = CredentialStore.biometricEncryptCipher();
                    authenticate(activity, cipher, title, cancelText, new BiometricPrompt.AuthenticationCallback()
                    {
                        @Override
                        public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result)
                        {
                            try
                            {
                                CredentialStore.biometricSave(activity, key, result.getCryptoObject().getCipher(), secret);
                                notifyBiometricStoreDone(key, BIOMETRIC_OK);
                            }
                            catch (GeneralSecurityException e)
                            {
                                notifyBiometricStoreDone(key, BIOMETRIC_FAILED);
                            }
                        }

                        @Override
                        public void onAuthenticationError(int errorCode, CharSequence errString)
                        {
                            notifyBiometricStoreDone(key, isCancel(errorCode) ? BIOMETRIC_CANCELED : BIOMETRIC_FAILED);
                        }
                    }, new Runnable()
                    {
                        @Override
                        public void run()
                        {
                            notifyBiometricStoreDone(key, BIOMETRIC_CANCELED);
                        }
                    });
                }
                catch (GeneralSecurityException | IOException e)
                {
                    Log.w(TAG, "Biometric storage not available");
                    notifyBiometricStoreDone(key, BIOMETRIC_FAILED);
                }
            }
        });
    }

    public static void biometricRead(final String key, final String title, final String cancelText)
    {
        final HardwareUtils activity = _context;
        if (!isUsable(activity))
        {
            notifyBiometricReadDone(key, BIOMETRIC_FAILED, "");
            return;
        }
        activity.runOnUiThread(new Runnable()
        {
            @Override
            public void run()
            {
                try
                {
                    Cipher cipher = CredentialStore.biometricDecryptCipher(activity, key);
                    if (cipher == null)
                    {
                        notifyBiometricReadDone(key, BIOMETRIC_INVALIDATED, "");
                        return;
                    }
                    authenticate(activity, cipher, title, cancelText, new BiometricPrompt.AuthenticationCallback()
                    {
                        @Override
                        public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result)
                        {
                            try
                            {
                                String secret = CredentialStore.biometricOpen(activity, key, result.getCryptoObject().getCipher());
                                notifyBiometricReadDone(key, BIOMETRIC_OK, secret);
                            }
                            catch (GeneralSecurityException e)
                            {
                                notifyBiometricReadDone(key, BIOMETRIC_FAILED, "");
                            }
                        }

                        @Override
                        public void onAuthenticationError(int errorCode, CharSequence errString)
                        {
                            notifyBiometricReadDone(key, isCancel(errorCode) ? BIOMETRIC_CANCELED : BIOMETRIC_FAILED, "");
                        }
                    }, new Runnable()
                    {
                        @Override
                        public void run()
                        {
                            notifyBiometricReadDone(key, BIOMETRIC_CANCELED, "");
                        }
                    });
                }
                catch (KeyPermanentlyInvalidatedException e)
                {
                    notifyBiometricReadDone(key, BIOMETRIC_INVALIDATED, "");
                }
                catch (GeneralSecurityException | IOException e)
                {
                    Log.w(TAG, "Biometric storage not available");
                    notifyBiometricReadDone(key, BIOMETRIC_FAILED, "");
                }
            }
        });
    }

    public static void biometricRemove(String key)
    {
        HardwareUtils activity = _context;
        if (activity != null)
            CredentialStore.biometricRemove(activity, key);
    }

    public static void biometricRemoveAll()
    {
        HardwareUtils activity = _context;
        if (activity != null)
            CredentialStore.biometricRemoveAll(activity);
    }

    private static void notifyBiometricStoreDone(String key, int result)
    {
        try
        {
            HardwareUtilsNatives.emitBiometricStoreDone(key, result);
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.e(TAG, "emitBiometricStoreDone: native method not registered", e);
        }
    }

    private static void notifyBiometricReadDone(String key, int result, String secret)
    {
        try
        {
            HardwareUtilsNatives.emitBiometricReadDone(key, result, secret);
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.e(TAG, "emitBiometricReadDone: native method not registered", e);
        }
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

    // Also used by CalaosMessagingService.onNewToken, which runs on the FCM
    // SDK's intent-handler executor thread; the native side queues to Qt.
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

    // An admin code was stashed (CalaosMessagingService, FCM executor thread):
    // the app reads it now if Qt runs, else at its next start or foreground.
    static void deliverAdminCode(String codeId)
    {
        if (!nativesReady)
            return;
        notifyAdminCode(codeId);
    }

    // Not parked on failure: nothing would take it once Qt is up, and the
    // app reads the whole stash anyway at its next start or foreground.
    private static void notifyAdminCode(String codeId)
    {
        try
        {
            HardwareUtilsNatives.emitAdminCodeNotified(codeId);
        }
        catch (UnsatisfiedLinkError e)
        {
            Log.w(TAG, "emitAdminCodeNotified: native method not registered");
        }
    }

    // The admin code on the clipboard, flagged sensitive so the system
    // clipboard preview (Android 13+) and keyboards do not show it. The app
    // clears it after its lifetime (AdminCodeModel). Qt thread.
    public static boolean copySecret(String text)
    {
        final HardwareUtils activity = _context;
        if (!isUsable(activity))
            return false;

        try
        {
            ClipboardManager cm = (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm == null)
                return false;
            ClipData clip = ClipData.newPlainText("Calaos admin code", text);
            PersistableBundle extras = new PersistableBundle();
            extras.putBoolean(Build.VERSION.SDK_INT >= 33 ? ClipDescription.EXTRA_IS_SENSITIVE
                                                          : "android.content.extra.IS_SENSITIVE", true);
            clip.getDescription().setExtras(extras);
            cm.setPrimaryClip(clip);
            return true;
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "copySecret failed: " + e.getClass().getSimpleName());
            return false;
        }
    }

    // FLAG_SECURE while an admin code is on screen: no screenshot, blank in
    // Recents.
    public static void setSecureScreen(final boolean on)
    {
        final HardwareUtils activity = _context;
        if (!isUsable(activity))
            return;

        activity.runOnUiThread(new Runnable()
        {
            public void run()
            {
                if (!isUsable(activity))
                    return;
                if (on)
                    activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
                else
                    activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
            }
        });
    }

    // Admin code id the activity was started with, if any. Cleared on read.
    public static String takePendingAdminCodeId()
    {
        String id = pendingAdminCodeId;
        pendingAdminCodeId = null;
        return id == null ? "" : id;
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
