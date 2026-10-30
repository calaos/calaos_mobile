package fr.calaos.calaoshome.debug;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.util.Log;
import com.google.firebase.messaging.RemoteMessage;
import fr.calaos.calaoshome.CalaosMessagingService;
import fr.calaos.calaoshome.CredentialStore;

// Debug builds only (android/debug/ source set, see build.gradle): injects a
// fake FCM data message into CalaosMessagingService without Play services, to
// exercise the notification + camera picture path (AND-7) on an emulator.
//
//   adb shell 'am broadcast -n fr.calaos.calaoshome/.debug.FakePushReceiver \
//       --es title Calaos --es body "Motion detected" --es event_uuid <uuid>'
//
// Only the data keys are forwarded (RemoteMessage.Builder cannot carry a
// notification block), which is the shape calaos_server sends to tokens
// registered with push_format "data". With --ez fail_encrypt true|false
// (and no data keys) it only toggles CredentialStore.failEncryptForTesting,
// to exercise the QSettings fallback of a failing Keystore.
//
// Encrypted admin code (src/AdminCode.h): --es push_key <64 hex> replaces the
// push key (restart the app so it reads it), then the admin_code_* extras are
// forwarded like the other data keys, e.g. vector 1 of calaos_base
// docs/08_http_api.md (key 000102...1e1f):
//
//   adb shell 'am broadcast -n fr.calaos.calaoshome/.debug.FakePushReceiver \
//       --es title Calaos --es body "Calaos admin code to open the web console." \
//       --es admin_code_id 0123456789abcdef --es admin_code_action console_open \
//       --es admin_code_expires 2026-10-25T09:13:03Z \
//       --es admin_code_nonce f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff \
//       --es admin_code_ct 8532d9ecac0a --es admin_code_tag 9e72bd26d3926652bc157819dd26d995'
//
// The receiver
// requires the DUMP permission (adb shell has it), is not compiled into
// release builds, and refuses to run in a non-debuggable process anyway.
public class FakePushReceiver extends BroadcastReceiver
{
    private static final String TAG = "CalaosHome";
    private static final String[] DATA_KEYS = {
        "title", "body", "message", "event_uuid",
        "admin_code_id", "admin_code_action", "admin_code_expires",
        "admin_code_nonce", "admin_code_ct", "admin_code_tag",
    };

    @Override
    public void onReceive(Context context, Intent intent)
    {
        if ((context.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0)
            return;

        if (intent.hasExtra("fail_encrypt"))
        {
            CredentialStore.failEncryptForTesting = intent.getBooleanExtra("fail_encrypt", false);
            Log.i(TAG, "FakePushReceiver: failEncryptForTesting = " + CredentialStore.failEncryptForTesting);
            if (intent.getStringExtra("event_uuid") == null && intent.getStringExtra("body") == null)
                return;
        }

        if (intent.hasExtra("push_key"))
        {
            boolean ok = CredentialStore.setPushKeyForTesting(context, intent.getStringExtra("push_key"));
            Log.i(TAG, "FakePushReceiver: push key " + (ok ? "replaced, restart the app" : "refused"));
            if (intent.getStringExtra("admin_code_id") == null && intent.getStringExtra("body") == null)
                return;
        }

        RemoteMessage.Builder b = new RemoteMessage.Builder("debug@fcm.googleapis.com")
            .setMessageId("debug-" + System.currentTimeMillis());
        for (String key: DATA_KEYS)
        {
            String v = intent.getStringExtra(key);
            if (v != null)
                b.addData(key, v);
        }

        Log.i(TAG, "FakePushReceiver: injecting a fake data message");
        CalaosMessagingService.handleMessage(context, b.build());
    }
}
