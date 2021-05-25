
package fr.calaos.calaoshome;

import android.content.Context;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.widget.EditText;
import android.util.Log;
import android.os.Bundle;
import android.content.SharedPreferences;
import java.lang.String;
import fr.calaos.calaoshome.Common;

class HardwareUtilsNatives
{
    public static native void emitDialogTextValid(String text);
    public static native void emitDialogCancel();
    public static native String getDemoUser();
    public static native String getDemoPass();
    public static native String getDemoHost();
}

public class HardwareUtils extends org.qtproject.qt5.android.bindings.QtActivity
{
    private static String SharedPreferencesKey = "fr.calaos.CalaosMobile";
    private static String SharedPreferencesHostKey = "calaos.hostname";
    private static String SharedPreferencesUserKey = "calaos-cn-user";
    private static String SharedPreferencesPassKey = "calaos-cn-pass";
    public static HardwareUtils _context;

    public String host;
    public String email;
    public String pass;

    public HardwareUtils()
    {
        _context = this;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // getSharedPreferences is not available in constructor (before onCreate). So we should use it here.
        SharedPreferences pref = this.getSharedPreferences(HardwareUtils.SharedPreferencesKey, Context.MODE_PRIVATE);
        this.host = pref.getString(HardwareUtils.SharedPreferencesHostKey, Common.getDemoHost());
        this.email = pref.getString(HardwareUtils.SharedPreferencesUserKey, Common.getDemoUser());
        this.pass = pref.getString(HardwareUtils.SharedPreferencesPassKey, Common.getDemoPass());
    }

    public static void showAlertMessage(String _title, String _message, String _buttontext)
    {
        final String title = _title;
        final String msg = _message;
        final String bt = _buttontext;
        _context.runOnUiThread(new Runnable()
        {
            public void run()
            {
                final AlertDialog alertDialog = new AlertDialog.Builder(_context).create();
                alertDialog.setTitle(title);
                alertDialog.setMessage(msg);
                alertDialog.setButton(AlertDialog.BUTTON_POSITIVE, bt, new DialogInterface.OnClickListener()
                {
                    public void onClick(DialogInterface dialog, int which)
                    {
                        alertDialog.dismiss();
                    }
                });
                alertDialog.setIcon(R.drawable.icon);
                alertDialog.show();
            }
        });
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

    public static void inputtextDialog(String _title, String _message)
    {
        final String title = _title;
        final String msg = _message;
        _context.runOnUiThread(new Runnable()
        {
            public void run()
            {
                final AlertDialog.Builder builder = new AlertDialog.Builder(_context);
                // Set an EditText view to get user input
                final EditText input = new EditText(_context);

                builder.setTitle(title);
                builder.setMessage(msg);
                builder.setView(input);

                builder.setPositiveButton("Confirm", new DialogInterface.OnClickListener()
                {
                    public void onClick(DialogInterface dialog, int which)
                    {
                        HardwareUtilsNatives.emitDialogTextValid(input.getText().toString());
                        dialog.dismiss();
                    }
                });
                builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener()
                {
                    public void onClick(DialogInterface dialog, int which)
                    {
                        HardwareUtilsNatives.emitDialogCancel();
                        dialog.dismiss();
                    }
                });

                builder.setIcon(R.drawable.icon);
                builder.show();
            }
        });
    }

    public static void loadAuthKeychain()
    {
        SharedPreferences pref = _context.getSharedPreferences(HardwareUtils.SharedPreferencesKey, Context.MODE_PRIVATE);
        String email = pref.getString(HardwareUtils.SharedPreferencesUserKey, "");
        String pass = pref.getString(HardwareUtils.SharedPreferencesPassKey, "");
        _context.email = email;
        _context.pass = pass;
    }

    public static void saveAuthKeychain(String email, String pass)
    {
        SharedPreferences pref = _context.getSharedPreferences(HardwareUtils.SharedPreferencesKey, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(HardwareUtils.SharedPreferencesUserKey, email);
        editor.putString(HardwareUtils.SharedPreferencesPassKey, pass);
        editor.apply();
    }

    public static void setConfigOption(String key, String value)
    {
        if (key.equals("calaos/host")) {
            SharedPreferences pref = _context.getSharedPreferences(HardwareUtils.SharedPreferencesKey, Context.MODE_PRIVATE);
            SharedPreferences.Editor editor = pref.edit();
            editor.putString(HardwareUtils.SharedPreferencesHostKey, value);
            editor.apply();
            _context.host = value;
        }
    }

    public static void resetAuthKeychain()
    {
        SharedPreferences pref = _context.getSharedPreferences(HardwareUtils.SharedPreferencesKey, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = pref.edit();
        editor.putString(HardwareUtils.SharedPreferencesUserKey, Common.getDemoUser());
        editor.putString(HardwareUtils.SharedPreferencesPassKey, Common.getDemoPass());
        editor.apply();
    }

    public String getNotificationPictureURL(String uuid)
    {
        String host = this.host;
        if (host.startsWith("ws://") || host.startsWith("wss://")) {
            host = host.replace("ws", "http");
        }
        if (!host.startsWith("http://") && !host.startsWith("https://")) {
            host = String.format("https://%s/api", host);
        }
        return String.format("%s?cn_user=%s&cn_pass=%s&action=eventlog&uuid=%s", host, this.email, this.pass, uuid);
    }
}
