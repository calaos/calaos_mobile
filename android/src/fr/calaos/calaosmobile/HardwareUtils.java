
package fr.calaos.calaosmobile;

import android.content.Context;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

public class HardwareUtils extends org.qtproject.qt5.android.bindings.QtActivity
{
    private static HardwareUtils _context;

    public HardwareUtils()
    {
        _context = this;
    }

    public static void showAlertMessage(String title, String message, String buttontext)
    {
        final AlertDialog alertDialog = new AlertDialog.Builder(_context).create();
        alertDialog.setTitle(title);
        alertDialog.setMessage(message);
        alertDialog.setButton(AlertDialog.BUTTON_POSITIVE, buttontext, new DialogInterface.OnClickListener()
        {
            public void onClick(DialogInterface dialog, int which)
            {
                alertDialog.dismiss();
            }
        });
        alertDialog.setIcon(R.drawable.icon);
        alertDialog.show();
    }

    public static int getNetworkStatus()
    {
        NetworkInfo networkInfo = null;
        ConnectivityManager connectivityManager = (ConnectivityManager) _context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connectivityManager != null)
        {
            networkInfo = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
            if (networkInfo.isAvailable() &&
                networkInfo.isConnected())
            {
                return 2; //Wifi
            }

            networkInfo = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_MOBILE);
            if (networkInfo.isAvailable() &&
                networkInfo.isConnected())
            {
                return 1; //WWAN
            }

            return 0; //Not connected
        }

        return 1; //We don't know. Say it's connected.
    }



/*

//Qt example for showing the user a notification in the notif center
//Maybe use that later.

    private static NotificationManager m_notificationManager;
    private static Notification.Builder m_builder;
    private static NotificationClient m_instance;

    public NotificationClient()
    {
        m_instance = this;
    }

    public static void notify(String s)
    {
        if (m_notificationManager == null) {
            m_notificationManager = (NotificationManager)m_instance.getSystemService(Context.NOTIFICATION_SERVICE);
            m_builder = new Notification.Builder(m_instance);
            m_builder.setSmallIcon(R.drawable.icon);
            m_builder.setContentTitle("A message from Qt!");
        }

        m_builder.setContentText(s);
        m_notificationManager.notify(1, m_builder.build());
    }
*/
}
