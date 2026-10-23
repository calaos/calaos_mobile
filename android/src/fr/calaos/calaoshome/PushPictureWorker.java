package fr.calaos.calaoshome;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.service.notification.StatusBarNotification;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.core.app.NotificationCompat;
import androidx.work.Constraints;
import androidx.work.Data;
import androidx.work.ExistingWorkPolicy;
import androidx.work.NetworkType;
import androidx.work.OneTimeWorkRequest;
import androidx.work.OutOfQuotaPolicy;
import androidx.work.WorkManager;
import androidx.work.Worker;
import androidx.work.WorkerParameters;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.Locale;
import java.util.regex.Pattern;
import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLPeerUnverifiedException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;
import org.json.JSONException;
import org.json.JSONObject;

// Attaches the camera picture of a Calaos event to its push notification
// (AND-7), the Android counterpart of ios/PushNotif/NotificationService.m.
//
// CalaosMessagingService posts the text notification first, then enqueues this
// worker, expedited on Android 12+ (onMessageReceived() has ~10 s, too short
// for two requests on a slow link). The worker runs without Qt: host and credentials come from
// CredentialStore. Same two requests as iOS, against the server's HTTP API:
//   <api>?cn_user=..&cn_pass=..&action=eventlog&uuid=<event_uuid>   -> JSON, pic_uid
//   <api>?cn_user=..&cn_pass=..&action=event_picture&pic_uid=<pic_uid> -> image
// (calaos_server serves a JPEG; any format BitmapFactory decodes is accepted,
// first frame for an animated GIF). The picture is downscaled and the
// notification re-posted with the same id in BigPictureStyle, so the text
// one is replaced in place and does not alert twice (setOnlyAlertOnce).
//
// Any failure (no credentials, unreachable server, wrong credentials, no
// pic_uid, undecodable image) leaves the text notification as it is: there is
// no error to show the user. After an explicit user logout
// (CredentialStore.isSessionActive() false) nothing is downloaded, and a
// notification the user dismissed meanwhile is not re-posted. Nothing here
// logs a URL, a credential or image bytes.
//
// TLS: a certificate the system trust store accepts for the host is used as
// is. Otherwise (self-signed box, certificate for another name) only the key
// the user confirmed in the app is accepted: the app copies its pins
// (host:port -> SHA-256 of the SubjectPublicKeyInfo, src/TlsPinning.h) to
// CredentialStore. The worker cannot ask the user: no pin, or a different key,
// means the text notification only. Plain http:// hosts work because the
// manifest allows cleartext traffic, as the Qt side already does for such
// servers.
public class PushPictureWorker extends Worker
{
    private static final String TAG = "CalaosHome";

    static final String KEY_EVENT_UUID = "event_uuid";
    static final String KEY_TITLE = "title";
    static final String KEY_BODY = "body";
    static final String KEY_NOTIFICATION_ID = "notification_id";
    static final String KEY_ATTEMPT = "attempt";

    // Retries. WorkManager starts the first attempt in-process right away,
    // while the push wake-up still grants network access; once that is over
    // the system closes the sockets of the (now cached) process, and a retry
    // through Result.retry() would be re-scheduled as a *regular* job, which
    // the scheduler batches for minutes. A failed attempt therefore enqueues
    // a fresh expedited request instead (expedited jobs run promptly, with
    // their own network access), a few times at most.
    private static final int MAX_ATTEMPTS = 3;
    private static final long RETRY_PAUSE_MS = 2000;

    private static final int CONNECT_TIMEOUT_MS = 5000;
    private static final int READ_TIMEOUT_MS = 10000;
    private static final int MAX_JSON_BYTES = 64 * 1024;
    private static final int MAX_IMAGE_BYTES = 8 * 1024 * 1024;
    // Longest side of the bitmap handed to the notification.
    private static final int MAX_BITMAP_SIDE = 1024;

    // pic_uid is a server-generated uuid; anything else is refused before it
    // is put in a URL.
    private static final Pattern PIC_UID = Pattern.compile("^[0-9a-zA-Z-]{1,64}$");

    public PushPictureWorker(@NonNull Context context, @NonNull WorkerParameters params)
    {
        super(context, params);
    }

    // One job per event: a second push for the same event (or a duplicate
    // delivery) does not start a second download.
    public static void enqueue(Context ctx, int notificationId, CalaosMessagingService.PushPayload payload)
    {
        if (payload.eventUuid == null)
            return;

        Data input = new Data.Builder()
            .putString(KEY_EVENT_UUID, payload.eventUuid)
            .putString(KEY_TITLE, payload.title)
            .putString(KEY_BODY, payload.body)
            .putInt(KEY_NOTIFICATION_ID, notificationId)
            .build();
        enqueueAttempt(ctx, input, 0);
    }

    // Unique name per event and attempt: a duplicate delivery of the same
    // event is dropped (KEEP), a retry is a new request.
    private static void enqueueAttempt(Context ctx, Data base, int attempt)
    {
        Data input = new Data.Builder().putAll(base).putInt(KEY_ATTEMPT, attempt).build();
        OneTimeWorkRequest.Builder builder = new OneTimeWorkRequest.Builder(PushPictureWorker.class)
            .setInputData(input)
            .setConstraints(new Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build());
        // Expedited job on Android 12+ (prompt, own network access). Below,
        // expedited work means a foreground service with its own
        // notification: not worth it, the first attempt runs in-process right
        // away either way and only the retries would be deferred.
        if (Build.VERSION.SDK_INT >= 31)
            builder.setExpedited(OutOfQuotaPolicy.RUN_AS_NON_EXPEDITED_WORK_REQUEST); // out of quota: still run
        OneTimeWorkRequest request = builder.build();

        try
        {
            WorkManager.getInstance(ctx.getApplicationContext())
                .enqueueUniqueWork("push_picture_" + base.getString(KEY_EVENT_UUID) + "#" + attempt,
                                   ExistingWorkPolicy.KEEP, request);
        }
        catch (IllegalStateException e)
        {
            // WorkManager not initialised (should not happen with the default
            // initializer); the text notification stays.
            Log.w(TAG, "Push picture: WorkManager unavailable: " + e.getMessage());
        }
    }

    @NonNull
    @Override
    public Result doWork()
    {
        Context ctx = getApplicationContext();
        Data in = getInputData();
        String eventUuid = CalaosMessagingService.sanitizeEventUuid(in.getString(KEY_EVENT_UUID));
        int notificationId = in.getInt(KEY_NOTIFICATION_ID, 0);
        int attempt = in.getInt(KEY_ATTEMPT, 0);
        if (eventUuid == null || notificationId == 0)
            return Result.failure();

        if (attempt > 0)
        {
            // A retry started in-process right after the failure: let a
            // transient server hiccup pass.
            SystemClock.sleep(RETRY_PAUSE_MS);
            if (isStopped())
                return Result.failure();
        }

        Bitmap picture;
        try
        {
            if (!CredentialStore.isSessionActive(ctx))
            {
                Log.i(TAG, "Push picture: user logged out, text notification kept");
                return Result.failure();
            }
            String host = CredentialStore.host(ctx);
            String user = CredentialStore.user(ctx);
            String pass = CredentialStore.password(ctx);
            if (host.isEmpty() || user.isEmpty())
            {
                Log.i(TAG, "Push picture: no server or credentials stored, text notification kept");
                return Result.failure();
            }
            String api = apiBaseUrl(host);

            String json = new String(httpGet(ctx, eventUrl(api, user, pass, eventUuid), MAX_JSON_BYTES), "UTF-8");
            String picUid = picUidFromEvent(json);
            if (picUid == null)
            {
                Log.i(TAG, "Push picture: event " + eventUuid + " has no picture");
                return Result.failure();
            }

            byte[] bytes = httpGet(ctx, pictureUrl(api, user, pass, picUid), MAX_IMAGE_BYTES);
            picture = decodeScaled(bytes, MAX_BITMAP_SIDE);
            if (picture == null)
            {
                Log.w(TAG, "Push picture: undecodable image (" + bytes.length + " bytes)");
                return Result.failure();
            }
        }
        catch (PermanentFailure e)
        {
            // Wrong credentials (400), unknown event or picture (404), body
            // over the cap, ...: a retry would not help.
            Log.w(TAG, "Push picture: " + e.getMessage() + " for event " + eventUuid);
            return Result.failure();
        }
        catch (IOException e)
        {
            // Unreachable server, timeout, TLS failure (certificate neither
            // trusted nor pinned), network access withdrawn: a few more
            // attempts, then give up.
            final boolean again = attempt + 1 < MAX_ATTEMPTS && !isStopped();
            Log.w(TAG, "Push picture: download failed: " + e.getClass().getSimpleName()
                  + " (attempt " + (attempt + 1) + "/" + MAX_ATTEMPTS + ")"
                  + (again ? ", will retry" : ""));
            if (again)
                enqueueAttempt(ctx, in, attempt + 1);
            return Result.failure();
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "Push picture: unexpected failure: " + e.getClass().getSimpleName());
            return Result.failure();
        }

        if (!isNotificationActive(ctx, notificationId))
        {
            // Dismissed (or tapped) meanwhile: do not bring it back.
            Log.i(TAG, "Push picture: notification gone for event " + eventUuid + ", not re-posted");
            return Result.success();
        }

        CalaosMessagingService.PushPayload payload = new CalaosMessagingService.PushPayload(
            in.getString(KEY_TITLE), in.getString(KEY_BODY), eventUuid);
        Notification n = CalaosMessagingService.buildNotification(ctx, payload)
            .setLargeIcon(picture)
            .setStyle(new NotificationCompat.BigPictureStyle()
                          .bigPicture(picture)
                          // Hide the large icon once expanded: the picture is there.
                          .bigLargeIcon((Bitmap) null)
                          .setSummaryText(payload.body))
            .build();
        CalaosMessagingService.showNotification(ctx, notificationId, n);
        Log.i(TAG, "Push picture: notification updated for event " + eventUuid);
        return Result.success();
    }

    private static boolean isNotificationActive(Context ctx, int id)
    {
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        if (nm == null)
            return true;
        try
        {
            for (StatusBarNotification sbn: nm.getActiveNotifications())
                if (sbn.getId() == id)
                    return true;
            return false;
        }
        catch (RuntimeException e)
        {
            // Cannot tell: re-post rather than lose the picture.
            return true;
        }
    }

    // ── URLs (same rules as the iOS extension and CalaosConnection::login) ──

    // http(s)://... is used as typed (the app's HTTP endpoint), ws(s)://... is
    // the websocket endpoint of the same server, a bare host gets the default
    // HTTPS endpoint.
    static String apiBaseUrl(String host)
    {
        String h = host.trim();
        if (h.startsWith("http://") || h.startsWith("https://"))
            return h;
        if (h.startsWith("ws://"))
            return "http://" + h.substring(5);
        if (h.startsWith("wss://"))
            return "https://" + h.substring(6);
        return "https://" + h + "/api.php";
    }

    static String eventUrl(String api, String user, String pass, String eventUuid)
    {
        return Uri.parse(api).buildUpon()
            .appendQueryParameter("cn_user", user)
            .appendQueryParameter("cn_pass", pass)
            .appendQueryParameter("action", "eventlog")
            .appendQueryParameter("uuid", eventUuid)
            .build().toString();
    }

    static String pictureUrl(String api, String user, String pass, String picUid)
    {
        return Uri.parse(api).buildUpon()
            .appendQueryParameter("cn_user", user)
            .appendQueryParameter("cn_pass", pass)
            .appendQueryParameter("action", "event_picture")
            .appendQueryParameter("pic_uid", picUid)
            .build().toString();
    }

    // The eventlog answer for one uuid is the event object itself
    // (HistEvent::toJson in calaos_server): pic_uid is a top-level string,
    // empty when the rule had no camera attached.
    static String picUidFromEvent(String json)
    {
        try
        {
            String picUid = new JSONObject(json).optString("pic_uid", "");
            return PIC_UID.matcher(picUid).matches() ? picUid : null;
        }
        catch (JSONException e)
        {
            return null;
        }
    }

    // ── HTTP ─────────────────────────────────────────────────────────────

    // A failure no retry can fix (the message is safe to log: no URL).
    private static class PermanentFailure extends IOException
    {
        private static final long serialVersionUID = 1L;

        PermanentFailure(String message)
        {
            super(message);
        }
    }

    private static final class HttpStatusException extends PermanentFailure
    {
        private static final long serialVersionUID = 1L;

        HttpStatusException(int status)
        {
            super("server answered HTTP " + status);
        }
    }

    // GET with short timeouts; the body is capped (a server answering with
    // something huge must not exhaust the worker). The URL is never logged.
    private static byte[] httpGet(Context ctx, String url, int maxBytes) throws IOException
    {
        URL u = new URL(url);
        HttpURLConnection conn = (HttpURLConnection) u.openConnection();
        try
        {
            if (conn instanceof HttpsURLConnection)
                applyPinning((HttpsURLConnection) conn, CredentialStore.tlsPin(ctx, hostKey(u)));
            conn.setConnectTimeout(CONNECT_TIMEOUT_MS);
            conn.setReadTimeout(READ_TIMEOUT_MS);
            conn.setRequestMethod("GET");
            conn.setUseCaches(false);
            // The credentials are in the query string: never follow a
            // redirect that could carry them to another host.
            conn.setInstanceFollowRedirects(false);

            int status = conn.getResponseCode();
            if (status != HttpURLConnection.HTTP_OK)
                throw new HttpStatusException(status);

            try (InputStream in = conn.getInputStream())
            {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[16 * 1024];
                int n;
                while ((n = in.read(buf)) > 0)
                {
                    if (out.size() + n > maxBytes)
                        throw new PermanentFailure("body larger than " + maxBytes + " bytes");
                    out.write(buf, 0, n);
                }
                return out.toByteArray();
            }
        }
        finally
        {
            conn.disconnect();
        }
    }

    // ── TLS pinning ──────────────────────────────────────────────────────

    // Same key as TlsPinning::hostKey(): lowercase host, IPv6 in brackets,
    // explicit port.
    static String hostKey(URL url)
    {
        String h = url.getHost().toLowerCase(Locale.ROOT);
        if (h.contains(":") && !h.startsWith("["))
            h = "[" + h + "]";
        int port = url.getPort() == -1 ? url.getDefaultPort() : url.getPort();
        return h + ":" + port;
    }

    // getEncoded() of an X.509 public key is its SubjectPublicKeyInfo DER,
    // the bytes the app hashes.
    static String spkiSha256(Certificate cert)
    {
        try
        {
            byte[] d = MessageDigest.getInstance("SHA-256").digest(cert.getPublicKey().getEncoded());
            StringBuilder sb = new StringBuilder(d.length * 2);
            for (byte b: d)
                sb.append(String.format(Locale.ROOT, "%02x", b & 0xff));
            return sb.toString();
        }
        catch (NoSuchAlgorithmException | RuntimeException e)
        {
            return "";
        }
    }

    private static boolean pinMatches(Certificate[] chain, String pin)
    {
        return pin != null && !pin.isEmpty() && chain != null && chain.length > 0
            && pin.equals(spkiSha256(chain[0]));
    }

    private static X509TrustManager systemTrustManager() throws GeneralSecurityException
    {
        TrustManagerFactory tmf = TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        tmf.init((KeyStore) null);
        for (TrustManager tm: tmf.getTrustManagers())
            if (tm instanceof X509TrustManager)
                return (X509TrustManager) tm;
        throw new GeneralSecurityException("no system X509TrustManager");
    }

    // System validation first, the pin only when it fails, for the chain and
    // for the host name alike (a pinned box is usually reached by IP).
    private static void applyPinning(HttpsURLConnection https, final String pin) throws IOException
    {
        final X509TrustManager system;
        final SSLContext sc;
        try
        {
            system = systemTrustManager();
            X509TrustManager pinned = new X509TrustManager()
            {
                @Override
                public void checkClientTrusted(X509Certificate[] chain, String authType) throws CertificateException
                {
                    system.checkClientTrusted(chain, authType);
                }

                @Override
                public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException
                {
                    try
                    {
                        system.checkServerTrusted(chain, authType);
                    }
                    catch (CertificateException e)
                    {
                        if (!pinMatches(chain, pin))
                            throw e;
                    }
                }

                @Override
                public X509Certificate[] getAcceptedIssuers()
                {
                    return system.getAcceptedIssuers();
                }
            };
            sc = SSLContext.getInstance("TLS");
            sc.init(null, new TrustManager[] { pinned }, null);
        }
        catch (GeneralSecurityException e)
        {
            throw new PermanentFailure("TLS setup failed: " + e.getClass().getSimpleName());
        }

        final HostnameVerifier defaultVerifier = HttpsURLConnection.getDefaultHostnameVerifier();
        https.setSSLSocketFactory(sc.getSocketFactory());
        https.setHostnameVerifier(new HostnameVerifier()
        {
            @Override
            public boolean verify(String host, SSLSession session)
            {
                if (defaultVerifier.verify(host, session))
                    return true;
                try
                {
                    return pinMatches(session.getPeerCertificates(), pin);
                }
                catch (SSLPeerUnverifiedException e)
                {
                    return false;
                }
            }
        });
    }

    // ── Image ────────────────────────────────────────────────────────────

    // Decodes (first frame of) the image, subsampled so that its longest
    // side is at most maxSide pixels. Null when the bytes are not an image.
    static Bitmap decodeScaled(byte[] bytes, int maxSide)
    {
        BitmapFactory.Options bounds = new BitmapFactory.Options();
        bounds.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(bytes, 0, bytes.length, bounds);
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0)
            return null;

        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inSampleSize = 1;
        while (Math.max(bounds.outWidth, bounds.outHeight) / opts.inSampleSize > maxSide)
            opts.inSampleSize *= 2;

        try
        {
            return BitmapFactory.decodeByteArray(bytes, 0, bytes.length, opts);
        }
        catch (OutOfMemoryError e)
        {
            return null;
        }
    }
}
