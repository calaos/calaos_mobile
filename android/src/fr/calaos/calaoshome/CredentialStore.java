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
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

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
//
// Also kept here, for the encrypted admin code push (src/AdminCode.h):
//  - the push key (32 bytes from SecureRandom, sent as push_key by
//    register_push), encrypted with its own Keystore key in calaos_push.xml:
//    "Reset all data" (clear()) keeps it, since the box freezes the key of
//    an approver phone;
//  - the stash of admin code payloads written by CalaosMessagingService as
//    received (no cryptography), read and pruned by the app. Already sealed
//    by calaos_server, so stored in clear; wiped by clear().
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
    // JSON object host:port -> SPKI SHA-256 hex, a copy of the app's TLS pins
    // (TlsPinning.h) for PushPictureWorker. Not a secret, kept in clear.
    private static final String PREF_TLS_PINS = "tls_pins";

    // Push key: same file as the FCM token (CalaosMessagingService), excluded
    // from backup like it.
    private static final String PUSH_PREFS_NAME = "calaos_push";
    private static final String PREF_PUSH_KEY = "push_key";
    private static final String PUSH_KEY_ALIAS = "calaos_push_key_v1";
    private static final int PUSH_KEY_BYTES = 32;

    // JSON object code_id -> entry, see stashAdminCode().
    private static final String PREF_ADMIN_CODES = "admin_codes";
    private static final Object STASH_LOCK = new Object();
    // A code lives 60 s: anything this old is junk, and the stash stays small.
    private static final long STASH_MAX_AGE_MS = 10 * 60 * 1000;
    private static final int STASH_MAX_ENTRIES = 16;
    private static final int STASH_MAX_FIELD = 64;
    private static final String[][] STASH_FIELDS = {
        { "admin_code_action", "action" },
        { "admin_code_expires", "expires" },
        { "admin_code_nonce", "nonce" },
        { "admin_code_ct", "ct" },
        { "admin_code_tag", "tag" },
    };

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
    // Never throws: an unreadable flag counts as active (C++ treats any JNI
    // failure the same way), so a broken store cannot keep the app logged out.
    public static boolean isSessionActive(Context ctx)
    {
        try
        {
            return prefs(ctx).getBoolean(PREF_SESSION_ACTIVE, true);
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "CredentialStore: session flag unreadable, considered active");
            return true;
        }
    }

    public static void setSessionActive(Context ctx, boolean active)
    {
        prefs(ctx).edit().putBoolean(PREF_SESSION_ACTIVE, active).commit();
    }

    // ── TLS pins ─────────────────────────────────────────────────────────

    public static void setTlsPins(Context ctx, String json)
    {
        prefs(ctx).edit().putString(PREF_TLS_PINS, json == null ? "{}" : json).commit();
    }

    // Lowercase hex, empty when nothing is pinned for hostKey or the value is
    // unreadable.
    public static String tlsPin(Context ctx, String hostKey)
    {
        try
        {
            return new JSONObject(prefs(ctx).getString(PREF_TLS_PINS, "{}"))
                .optString(hostKey, "").toLowerCase(Locale.ROOT);
        }
        catch (JSONException | RuntimeException e)
        {
            return "";
        }
    }

    // ── Push key ─────────────────────────────────────────────────────────

    // 64 lowercase hex, created on the first call. Empty when the Keystore
    // cannot keep one: register_push then sends no key and the box pushes
    // the legacy text. Never logged.
    public static synchronized String pushKey(Context ctx)
    {
        try
        {
            SharedPreferences prefs = pushPrefs(ctx);
            String stored = prefs.getString(PREF_PUSH_KEY, null);
            if (stored != null && !stored.isEmpty())
            {
                String hex = decrypt(PUSH_KEY_ALIAS, stored);
                if (isPushKeyHex(hex))
                    return hex;
                // Keystore key lost: a new push key, sent at the next login.
                Log.w(TAG, "CredentialStore: push key unreadable, creating a new one");
            }

            byte[] raw = new byte[PUSH_KEY_BYTES];
            new SecureRandom().nextBytes(raw);
            String hex = toHex(raw);
            java.util.Arrays.fill(raw, (byte) 0);
            String enc = encrypt(PUSH_KEY_ALIAS, hex);
            if (enc == null || !prefs.edit().putString(PREF_PUSH_KEY, enc).commit())
                return "";
            Log.i(TAG, "CredentialStore: push key created");
            return hex;
        }
        catch (RuntimeException e)
        {
            Log.w(TAG, "CredentialStore: push key unavailable: " + e.getClass().getSimpleName());
            return "";
        }
    }

    // Debug builds only (FakePushReceiver): replays the golden vectors of the
    // contract on a device. Takes effect at the next app start.
    public static synchronized boolean setPushKeyForTesting(Context ctx, String hex)
    {
        if (!isPushKeyHex(hex))
            return false;
        String enc = encrypt(PUSH_KEY_ALIAS, hex);
        return enc != null && pushPrefs(ctx).edit().putString(PREF_PUSH_KEY, enc).commit();
    }

    private static boolean isPushKeyHex(String hex)
    {
        return hex != null && hex.matches("^[0-9a-f]{64}$");
    }

    private static String toHex(byte[] b)
    {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte x: b)
            sb.append(String.format(Locale.ROOT, "%02x", x & 0xff));
        return sb.toString();
    }

    private static SharedPreferences pushPrefs(Context ctx)
    {
        return ctx.getApplicationContext().getSharedPreferences(PUSH_PREFS_NAME, Context.MODE_PRIVATE);
    }

    // ── Admin code stash ─────────────────────────────────────────────────

    // Stores the admin_code_* strings of a push as received, with the
    // reception time by the wall clock and by elapsedRealtime() (the app
    // never trusts the box's expires). The id must have been validated
    // (CalaosMessagingService.sanitizeAdminCodeId). An id already stashed is
    // kept as first received. Returns false when nothing could be written.
    public static boolean stashAdminCode(Context ctx, Map<String, String> data, long receivedAt, long receivedMono)
    {
        String id = CalaosMessagingService.sanitizeAdminCodeId(data.get(CalaosMessagingService.EXTRA_ADMIN_CODE_ID));
        if (id == null)
            return false;

        try
        {
            JSONObject entry = new JSONObject();
            entry.put("code_id", id);
            for (String[] f: STASH_FIELDS)
            {
                String v = data.get(f[0]);
                entry.put(f[1], v == null || v.length() > STASH_MAX_FIELD ? "" : v);
            }
            entry.put("received_at", receivedAt);
            entry.put("received_mono", receivedMono);

            synchronized (STASH_LOCK)
            {
                JSONObject all = readStash(ctx);
                pruneStash(all, receivedAt);
                if (!all.has(id))
                    all.put(id, entry);
                return prefs(ctx).edit().putString(PREF_ADMIN_CODES, all.toString()).commit();
            }
        }
        catch (JSONException | RuntimeException e)
        {
            Log.w(TAG, "CredentialStore: admin code not stashed: " + e.getClass().getSimpleName());
            return false;
        }
    }

    // JSON array of the stashed entries, "[]" when none or unreadable.
    public static String adminCodes(Context ctx)
    {
        synchronized (STASH_LOCK)
        {
            JSONObject all = readStash(ctx);
            JSONArray out = new JSONArray();
            Iterator<String> it = all.keys();
            while (it.hasNext())
            {
                JSONObject e = all.optJSONObject(it.next());
                if (e != null)
                    out.put(e);
            }
            return out.toString();
        }
    }

    // Removes the entries whose ids are in idsJson (JSON array); returns the
    // valid ids asked for, removed or not.
    public static List<String> removeAdminCodes(Context ctx, String idsJson)
    {
        List<String> ids = new ArrayList<>();
        try
        {
            JSONArray arr = new JSONArray(idsJson == null ? "[]" : idsJson);
            for (int i = 0; i < arr.length(); i++)
            {
                String id = CalaosMessagingService.sanitizeAdminCodeId(arr.optString(i, null));
                if (id != null)
                    ids.add(id);
            }
        }
        catch (JSONException e)
        {
            return ids;
        }

        synchronized (STASH_LOCK)
        {
            JSONObject all = readStash(ctx);
            boolean changed = false;
            for (String id: ids)
                changed |= all.remove(id) != null;
            if (changed)
                prefs(ctx).edit().putString(PREF_ADMIN_CODES, all.toString()).commit();
        }
        return ids;
    }

    private static JSONObject readStash(Context ctx)
    {
        try
        {
            return new JSONObject(prefs(ctx).getString(PREF_ADMIN_CODES, "{}"));
        }
        catch (JSONException | RuntimeException e)
        {
            return new JSONObject();
        }
    }

    // Drops what is too old (or dated in the future: clock set back) and keeps
    // the newest STASH_MAX_ENTRIES. The app does the exact 60 s check.
    private static void pruneStash(JSONObject all, long now)
    {
        List<String> keys = new ArrayList<>();
        Iterator<String> it = all.keys();
        while (it.hasNext())
            keys.add(it.next());

        for (String k: keys)
        {
            JSONObject e = all.optJSONObject(k);
            long at = e == null ? 0 : e.optLong("received_at", 0);
            if (e == null || Math.abs(now - at) > STASH_MAX_AGE_MS)
                all.remove(k);
        }

        while (all.length() >= STASH_MAX_ENTRIES)
        {
            String oldest = null;
            long oldestAt = Long.MAX_VALUE;
            Iterator<String> i = all.keys();
            while (i.hasNext())
            {
                String k = i.next();
                long at = all.optJSONObject(k).optLong("received_at", 0);
                if (at < oldestAt)
                {
                    oldestAt = at;
                    oldest = k;
                }
            }
            all.remove(oldest);
        }
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
        String u = failEncryptForTesting ? null : encrypt(KEY_ALIAS, user == null ? "" : user);
        String p = failEncryptForTesting ? null : encrypt(KEY_ALIAS, password == null ? "" : password);
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
        String clear = decrypt(KEY_ALIAS, stored);
        return clear == null ? "" : clear;
    }

    private static synchronized SecretKey key(String alias, boolean create) throws GeneralSecurityException, IOException
    {
        KeyStore ks = KeyStore.getInstance(KEYSTORE);
        ks.load(null);
        KeyStore.Entry entry = ks.getEntry(alias, null);
        if (entry instanceof KeyStore.SecretKeyEntry)
            return ((KeyStore.SecretKeyEntry) entry).getSecretKey();
        if (!create)
            return null;

        KeyGenerator gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
        gen.init(new KeyGenParameterSpec.Builder(alias,
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
    private static String encrypt(String alias, String clear)
    {
        try
        {
            Cipher cipher = Cipher.getInstance(TRANSFORMATION);
            cipher.init(Cipher.ENCRYPT_MODE, key(alias, true)); // Keystore picks a random IV
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

    private static String decrypt(String alias, String stored)
    {
        try
        {
            byte[] in = Base64.decode(stored, Base64.NO_WRAP);
            if (in.length <= GCM_IV_BYTES)
                return null;
            SecretKey k = key(alias, false);
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
