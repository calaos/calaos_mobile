package fr.calaos.calaoshome;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import android.util.Log;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

// Calaos server address and credentials, readable from Java without Qt.
//
// The push picture worker (PushPictureWorker) runs in a process where the Qt
// activity may never have started, so the host and the cn_user/cn_pass pair
// cannot stay in QSettings only: C++ (HardwareUtilsAndroid) writes them here
// and reads them back from here. This is the Android counterpart of the iOS
// shared keychain + App Group used by the notification service extension.
//
// Storage: one private SharedPreferences file. The credentials are encrypted
// with an AES-256-GCM key that lives in the Android Keystore (never
// extractable, bound to this device and app install), random IV per write.
// The host is not a secret and is kept in clear. The file is excluded from
// Auto Backup / device transfer (res/xml/backup_rules.xml,
// data_extraction_rules.xml): the Keystore key does not travel, so a restored
// copy would be unreadable anyway. EncryptedSharedPreferences is not used
// (deprecated).
//
// Thread-safety: SharedPreferences is thread-safe and every call creates its
// own Cipher; the Qt thread and a worker thread may use this class at once.
// All methods are null-safe on failure and never throw (Keystore errors
// surface as GeneralSecurityException, IOException or ProviderException, a
// RuntimeException): a credential that cannot be read is reported as absent,
// and nothing here ever logs a credential, a host with credentials, or the
// ciphertext.
public final class CredentialStore
{
    private static final String TAG = "CalaosHome";

    // Keep in sync with res/xml/backup_rules.xml and data_extraction_rules.xml.
    static final String PREFS_NAME = "calaos_credentials";
    private static final String PREF_HOST = "host";
    private static final String PREF_USER = "cn_user";
    private static final String PREF_PASS = "cn_pass";
    // False after an explicit user logout: the credentials stay (prefill of
    // the login screen) but background push processing must not use them.
    private static final String PREF_SESSION_ACTIVE = "session_active";

    private static final String KEYSTORE = "AndroidKeyStore";
    private static final String KEY_ALIAS = "calaos_credentials_v1";
    private static final String TRANSFORMATION = "AES/GCM/NoPadding";
    private static final int GCM_IV_BYTES = 12;
    private static final int GCM_TAG_BITS = 128;

    // Debug builds only (set by fr.calaos.calaoshome.debug.FakePushReceiver,
    // absent from release): makes save() fail like a broken Keystore would,
    // to exercise the QSettings fallback of HardwareUtilsAndroid.
    public static volatile boolean failEncryptForTesting;

    private CredentialStore()
    {
    }

    // ── Host ─────────────────────────────────────────────────────────────

    // The value typed by the user in the connection settings ("calaos/host"),
    // not normalised: see PushPictureWorker.apiBaseUrl().
    public static String host(Context ctx)
    {
        return prefs(ctx).getString(PREF_HOST, "");
    }

    public static void setHost(Context ctx, String host)
    {
        prefs(ctx).edit().putString(PREF_HOST, host == null ? "" : host).apply();
    }

    // ── Session ──────────────────────────────────────────────────────────

    // True while the user is logged in, or has never logged out explicitly
    // (default, so an install updated in the background keeps working). An
    // explicit logout sets it false until the next successful login; a lost
    // connection or a reconnection does not change it.
    public static boolean isSessionActive(Context ctx)
    {
        return prefs(ctx).getBoolean(PREF_SESSION_ACTIVE, true);
    }

    public static void setSessionActive(Context ctx, boolean active)
    {
        prefs(ctx).edit().putBoolean(PREF_SESSION_ACTIVE, active).commit();
    }

    // ── Credentials ──────────────────────────────────────────────────────

    // Empty string when absent or unreadable (key lost, corrupt value).
    public static String user(Context ctx)
    {
        return decryptPref(ctx, PREF_USER);
    }

    public static String password(Context ctx)
    {
        return decryptPref(ctx, PREF_PASS);
    }

    // Returns false when the Keystore refused to encrypt (the caller must then
    // keep its own copy); nothing is written in that case.
    public static boolean save(Context ctx, String user, String password)
    {
        String u = failEncryptForTesting ? null : encrypt(user == null ? "" : user);
        String p = failEncryptForTesting ? null : encrypt(password == null ? "" : password);
        if (u == null || p == null)
        {
            Log.w(TAG, "CredentialStore: Keystore encryption failed, credentials not stored");
            return false;
        }

        // commit(): C++ expects the value to be on disk when the call returns
        // (the messaging service may run in another process right after).
        return prefs(ctx).edit().putString(PREF_USER, u).putString(PREF_PASS, p).commit();
    }

    // Forgets the pair only (host and session flag untouched): used when a
    // new pair could not be stored, so a stale one is never read back.
    public static void clearCredentials(Context ctx)
    {
        prefs(ctx).edit().remove(PREF_USER).remove(PREF_PASS).commit();
    }

    // "Reset all data": forget everything, including the host, the session
    // flag and the Keystore key.
    public static void clear(Context ctx)
    {
        prefs(ctx).edit().clear().commit();
        try
        {
            KeyStore ks = KeyStore.getInstance(KEYSTORE);
            ks.load(null);
            if (ks.containsAlias(KEY_ALIAS))
                ks.deleteEntry(KEY_ALIAS);
        }
        catch (GeneralSecurityException | IOException | RuntimeException e)
        {
            // Values are gone already; a stale key only encrypts the next login.
            Log.w(TAG, "CredentialStore: could not delete the Keystore key: " + e.getClass().getSimpleName());
        }
    }

    // ── Internals ────────────────────────────────────────────────────────

    private static SharedPreferences prefs(Context ctx)
    {
        return ctx.getApplicationContext().getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
    }

    private static String decryptPref(Context ctx, String key)
    {
        String stored = prefs(ctx).getString(key, null);
        if (stored == null || stored.isEmpty())
            return "";
        String clear = decrypt(stored);
        return clear == null ? "" : clear;
    }

    private static synchronized SecretKey key(boolean create) throws GeneralSecurityException, IOException
    {
        KeyStore ks = KeyStore.getInstance(KEYSTORE);
        ks.load(null);
        KeyStore.Entry entry = ks.getEntry(KEY_ALIAS, null);
        if (entry instanceof KeyStore.SecretKeyEntry)
            return ((KeyStore.SecretKeyEntry) entry).getSecretKey();
        if (!create)
            return null;

        KeyGenerator gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
        gen.init(new KeyGenParameterSpec.Builder(KEY_ALIAS,
                                                 KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                     .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                     .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                     .setKeySize(256)
                     // The worker decrypts in the background, without the user:
                     // no authentication requirement on the key.
                     .setUserAuthenticationRequired(false)
                     .build());
        return gen.generateKey();
    }

    // base64(iv || ciphertext || tag), or null on failure.
    private static String encrypt(String clear)
    {
        try
        {
            Cipher cipher = Cipher.getInstance(TRANSFORMATION);
            cipher.init(Cipher.ENCRYPT_MODE, key(true)); // Keystore picks a random IV
            byte[] iv = cipher.getIV();
            byte[] ct = cipher.doFinal(clear.getBytes(StandardCharsets.UTF_8));
            if (iv == null || iv.length != GCM_IV_BYTES)
                return null;

            byte[] out = new byte[iv.length + ct.length];
            System.arraycopy(iv, 0, out, 0, iv.length);
            System.arraycopy(ct, 0, out, iv.length, ct.length);
            return Base64.encodeToString(out, Base64.NO_WRAP);
        }
        catch (GeneralSecurityException | IOException | RuntimeException e)
        {
            // RuntimeException covers the Keystore's ProviderException and
            // IllegalStateException: this class never throws.
            Log.w(TAG, "CredentialStore: encrypt failed: " + e.getClass().getSimpleName());
            return null;
        }
    }

    private static String decrypt(String stored)
    {
        try
        {
            byte[] in = Base64.decode(stored, Base64.NO_WRAP);
            if (in.length <= GCM_IV_BYTES)
                return null;
            SecretKey k = key(false);
            if (k == null)
                return null;

            Cipher cipher = Cipher.getInstance(TRANSFORMATION);
            cipher.init(Cipher.DECRYPT_MODE, k, new GCMParameterSpec(GCM_TAG_BITS, in, 0, GCM_IV_BYTES));
            byte[] clear = cipher.doFinal(in, GCM_IV_BYTES, in.length - GCM_IV_BYTES);
            return new String(clear, StandardCharsets.UTF_8);
        }
        catch (GeneralSecurityException | IOException | RuntimeException e)
        {
            // Key invalidated/missing or corrupt value: treated as "no
            // credentials"; the user logs in again and the store is rewritten.
            Log.w(TAG, "CredentialStore: decrypt failed: " + e.getClass().getSimpleName());
            return null;
        }
    }
}
