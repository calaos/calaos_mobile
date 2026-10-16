package fr.calaos.calaoshome;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.util.Log;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;
import java.util.Map;

// FCM entry point. This service is started by Google Play services, possibly
// in a fresh process where the Qt activity never ran: libCalaosHome is then
// not loaded, so nothing here may depend on Qt. State that C++ needs (the
// push token) goes through SharedPreferences and is read at startup by
// HardwareUtilsAndroid::platformInit(); natives are only called when the
// activity reported that Qt registered them.
//
// Payload, as relayed by push.calaos.fr (gorush) from calaos_server
// (NotifManager::sendPushNotification): a notification + data message,
//   notification: { title: "Calaos", body: <message> }
//   data:         { title, body, message, [event_uuid] }
// With a notification block, onMessageReceived() only runs while the app is
// in the foreground; in the background the system tray shows the message and
// the data keys arrive as extras of the activity intent when it is tapped
// (see HardwareUtils.eventUuidFromIntent). Both paths end in the same
// notification layout and the same tap intent.
public class CalaosMessagingService extends FirebaseMessagingService
{
    private static final String TAG = "CalaosHome";

    // Keep in sync with the default_notification_channel_id meta-data in
    // AndroidManifest.xml (used by the system for background messages).
    public static final String CHANNEL_ID = "calaos_events";

    // Intent extra carrying the Calaos event uuid to open (same key as the
    // push payload and as the iOS userInfo).
    public static final String EXTRA_EVENT_UUID = "event_uuid";

    // Payload keys.
    private static final String KEY_TITLE = "title";
    private static final String KEY_BODY = "body";
    private static final String KEY_MESSAGE = "message";
    private static final String DEFAULT_TITLE = "Calaos";

    // Token persistence, shared with the activity.
    private static final String PREFS_NAME = "calaos_push";
    private static final String PREF_TOKEN = "fcm_token";

    // Decoded push payload. Kept as a small value object so the picture
    // enrichment (AND-7) can reuse it when it rebuilds the notification.
    public static final class PushPayload
    {
        public final String title;
        public final String body;
        public final String eventUuid; // null when the payload has none

        PushPayload(String title, String body, String eventUuid)
        {
            this.title = title;
            this.body = body;
            this.eventUuid = eventUuid;
        }

        static PushPayload from(RemoteMessage message)
        {
            Map<String, String> data = message.getData();
            RemoteMessage.Notification notif = message.getNotification();

            String title = firstNonEmpty(data.get(KEY_TITLE),
                                         notif != null ? notif.getTitle() : null,
                                         DEFAULT_TITLE);
            String body = firstNonEmpty(data.get(KEY_BODY),
                                        data.get(KEY_MESSAGE),
                                        notif != null ? notif.getBody() : null,
                                        "");
            String uuid = data.get(EXTRA_EVENT_UUID);
            if (uuid != null && uuid.isEmpty())
                uuid = null;

            return new PushPayload(title, body, uuid);
        }

        private static String firstNonEmpty(String... values)
        {
            for (String v: values)
                if (v != null && !v.isEmpty())
                    return v;
            return null;
        }
    }

    // ── Token ────────────────────────────────────────────────────────────

    // Deprecated by FCM >= 25.1 together with getToken(), in favour of
    // onRegistered() which delivers a Firebase Installation ID. The Calaos
    // relay targets classic registration tokens: see
    // HardwareUtils.requestPushToken().
    @Override
    @SuppressWarnings("deprecation")
    public void onNewToken(String token)
    {
        Log.i(TAG, "FCM token refreshed: " + describeToken(token));
        storeToken(this, token);
        HardwareUtils.deliverPushToken(token);
    }

    public static String storedToken(Context ctx)
    {
        return prefs(ctx).getString(PREF_TOKEN, "");
    }

    public static void storeToken(Context ctx, String token)
    {
        prefs(ctx).edit().putString(PREF_TOKEN, token == null ? "" : token).apply();
    }

    private static SharedPreferences prefs(Context ctx)
    {
        return ctx.getApplicationContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    // Never log a push token in full: it is a credential for this device.
    public static String describeToken(String token)
    {
        if (token == null || token.isEmpty())
            return "<empty>";
        int n = Math.min(8, token.length());
        return token.substring(0, n) + "... (" + token.length() + " chars)";
    }

    // ── Messages ─────────────────────────────────────────────────────────

    @Override
    public void onMessageReceived(RemoteMessage message)
    {
        PushPayload payload = PushPayload.from(message);
        Log.i(TAG, "FCM message received, event uuid: "
              + (payload.eventUuid != null ? payload.eventUuid : "<none>"));

        int id = notificationId(payload.eventUuid);
        showNotification(this, id, buildNotification(this, payload).build());

        // AND-7 extension point: when payload.eventUuid is set, enqueue the
        // expedited picture worker here. It must rebuild the notification with
        // buildNotification(), add the BigPictureStyle / largeIcon, and call
        // showNotification() again with the same id so the text notification
        // posted above is replaced in place. onMessageReceived() has ~10 s:
        // never download from this thread.
    }

    // ── Notification helpers (shared with the activity and AND-7) ────────

    // Idempotent; cheap to call at every start.
    public static void ensureNotificationChannel(Context ctx)
    {
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        if (nm == null)
            return;

        NotificationChannel channel = new NotificationChannel(CHANNEL_ID, "Calaos",
                                                              NotificationManager.IMPORTANCE_HIGH);
        channel.setDescription("Calaos home automation events");
        channel.enableLights(true);
        channel.enableVibration(true);
        nm.createNotificationChannel(channel);
    }

    // One notification per Calaos event, so a later update (picture, AND-7)
    // replaces the text-only one instead of stacking a second entry. Pushes
    // without an event uuid get a time-based id and never collide.
    public static int notificationId(String eventUuid)
    {
        if (eventUuid != null && !eventUuid.isEmpty())
            return eventUuid.hashCode();
        return (int) (System.currentTimeMillis() & 0x7fffffff);
    }

    // The text notification. Callers may decorate the builder further
    // (BigPictureStyle, largeIcon) before build().
    public static NotificationCompat.Builder buildNotification(Context ctx, PushPayload payload)
    {
        ensureNotificationChannel(ctx);

        NotificationCompat.Builder b = new NotificationCompat.Builder(ctx, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_stat_calaos)
            .setColor(ctx.getColor(R.color.calaos_notification))
            .setContentTitle(payload.title)
            .setContentText(payload.body)
            .setStyle(new NotificationCompat.BigTextStyle().bigText(payload.body))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setDefaults(Notification.DEFAULT_ALL)
            .setAutoCancel(true)
            .setContentIntent(openAppIntent(ctx, payload.eventUuid));

        return b;
    }

    public static void showNotification(Context ctx, int id, Notification notification)
    {
        NotificationManagerCompat nm = NotificationManagerCompat.from(ctx);
        if (!nm.areNotificationsEnabled())
        {
            // Android 13+ without POST_NOTIFICATIONS, or the user disabled
            // the app's notifications: notify() would be dropped silently.
            Log.w(TAG, "Notifications are disabled for this app, push not shown");
            return;
        }

        try
        {
            nm.notify(id, notification);
        }
        catch (SecurityException e)
        {
            Log.w(TAG, "notify() refused: " + e.getMessage());
        }
    }

    // Tap target: the (singleTop) Qt activity, carrying the event uuid. The
    // request code is derived from the uuid so two pending notifications keep
    // their own extras (FLAG_UPDATE_CURRENT would otherwise overwrite them).
    private static PendingIntent openAppIntent(Context ctx, String eventUuid)
    {
        Intent intent = new Intent(ctx, HardwareUtils.class);
        intent.setAction(Intent.ACTION_MAIN);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        if (eventUuid != null)
            intent.putExtra(EXTRA_EVENT_UUID, eventUuid);

        int requestCode = eventUuid != null ? eventUuid.hashCode() : 0;
        return PendingIntent.getActivity(ctx, requestCode, intent,
                                         PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }
}
