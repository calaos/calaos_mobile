package fr.calaos.calaoshome;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.app.PendingIntent;
import android.graphics.Color;
import android.graphics.BitmapFactory;
import android.app.NotificationChannel;
import android.util.Log;
import android.app.Notification.*;
import android.graphics.Bitmap;
import java.net.URL;
import java.io.IOException;
import fr.calaos.calaoshome.HardwareUtils;

public class NotificationService
{
    private static NotificationManager m_notificationManager;
    private static Notification.Builder m_builder;

    public NotificationService() {}

    public static void notify(Context context, String title, String body, String eventUUID) {
        try {
            m_notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);

            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                int importance = NotificationManager.IMPORTANCE_DEFAULT;
                NotificationChannel notificationChannel = new NotificationChannel("Qt", "Qt Notifier", importance);
                m_notificationManager.createNotificationChannel(notificationChannel);
                m_builder = new Notification.Builder(context, notificationChannel.getId());
            } else {
                m_builder = new Notification.Builder(context);
            }

            m_builder.setSmallIcon(R.drawable.icon)
                    .setLargeIcon(BitmapFactory.decodeResource(context.getResources(), R.drawable.icon))
                    .setContentTitle(title)
                    .setContentText(body)
                    .setDefaults(Notification.DEFAULT_SOUND);

            if (!eventUUID.isEmpty()) {
                try {
                    URL url = new URL(HardwareUtils._context.getNotificationPictureURL(eventUUID));
                    Logger.debug("notification picture at "+url.toString());
                    Bitmap bmp = BitmapFactory.decodeStream(url.openConnection().getInputStream());
                    m_builder.setStyle(new Notification.BigPictureStyle().bigPicture(bmp));
                }
                catch (IOException e) {
                    Logger.error(e.toString());
                }
            }

            m_notificationManager.notify(0, m_builder.build());
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
