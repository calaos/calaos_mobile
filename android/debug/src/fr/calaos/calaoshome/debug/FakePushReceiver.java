package fr.calaos.calaoshome.debug;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.util.Log;
import com.google.firebase.messaging.RemoteMessage;
import fr.calaos.calaoshome.CalaosMessagingService;

// Debug builds only (android/debug/ source set, see build.gradle): injects a
// fake FCM data message into CalaosMessagingService without Play services, to
// exercise the notification + camera picture path (AND-7) on an emulator.
//
//   adb shell am broadcast -n fr.calaos.calaoshome/.debug.FakePushReceiver \
//       --es title Calaos --es body "Motion detected" --es event_uuid <uuid>
//
// Only the data keys are forwarded (RemoteMessage.Builder cannot carry a
// notification block), which is the shape calaos_server sends to tokens
// registered with push_format "data". Not compiled into release builds; and
// refuses to run in a non-debuggable process anyway.
public class FakePushReceiver extends BroadcastReceiver
{
    private static final String TAG = "CalaosHome";
    private static final String[] DATA_KEYS = { "title", "body", "message", "event_uuid" };

    @Override
    public void onReceive(Context context, Intent intent)
    {
        if ((context.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0)
            return;

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
