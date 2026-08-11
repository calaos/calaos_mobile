#ifndef JSONKEYS_H
#define JSONKEYS_H

#include <QString>

/* Single source of truth for the *field names* of the calaos_server protocol
 * (T21).
 *
 * Every key below used to be a bare literal repeated across the transport
 * (CalaosConnection), the decoder (CalaosEventDecoder) and the models. That is
 * the one class of typo the compiler cannot catch: a misspelled key is not an
 * error, it is a key that is simply absent from the map, so the read silently
 * yields a default value (an empty QString, a 0) and the UI shows plausible
 * nonsense instead of failing.
 *
 * Scope, on purpose:
 *  - KEYS ONLY. The *values* that travel under those keys are not here.
 *    Action names ("get_home", "poll_listen", "set_state"...), IO commands
 *    ("true", "up", "move zoomin"...), the "input"/"output" side of a command
 *    and the boolean spelling "true" stay where they are read or written.
 *    gui_type values ("light", "switch"...) belong to IOTypeRegistry (T16) and
 *    must not be duplicated here.
 *  - PROTOCOL ONLY. Keys that never leave the process are not protocol keys
 *    and are deliberately absent: QSettings entries ("calaos/host",
 *    "app/favorites"), the favorite records FavoritesModel saves and reloads
 *    through QSettings, model role names, QFAppDispatcher message payloads,
 *    and the calaos-os REST API of CalaosOsAPI/UpdateManager (a different
 *    protocol, with its own files).
 *
 * One key = one constant, even when the same name appears in unrelated
 * payloads ("id" is an IO id, a camera id and an audio player id): they are
 * the same wire name, and splitting them would let the two copies drift.
 *
 * Note for Common::toIntSafe()/toDoubleSafe() (T11): their last argument is a
 * log label, not a key. It names the field in qualified form
 * ("IOBase.status.battery_level") and stays a literal, but its last segment is
 * meant to match the key constant used on the same line.
 */
namespace JsonKeys
{

//--- Websocket envelope and HTTP request body -------------------------------
//{ "msg": <action>, "msg_id": <client tag>, "data": { ... } } over the
//websocket; the same fields flattened with "action" over HTTP.
inline constexpr QLatin1StringView Msg{"msg"};
inline constexpr QLatin1StringView MsgId{"msg_id"};
inline constexpr QLatin1StringView Data{"data"};
inline constexpr QLatin1StringView Action{"action"};
//v3 event frames name their event type here instead of in "msg".
inline constexpr QLatin1StringView TypeStr{"type_str"};

//--- Credentials ------------------------------------------------------------
//Also the two names CalaosConnection redacts before logging a payload.
inline constexpr QLatin1StringView CnUser{"cn_user"};
inline constexpr QLatin1StringView CnPass{"cn_pass"};
//Credentials change (websocket "settings" action).
inline constexpr QLatin1StringView OldUser{"old_user"};
inline constexpr QLatin1StringView OldPw{"old_pw"};
inline constexpr QLatin1StringView NewUser{"new_user"};
inline constexpr QLatin1StringView NewPw{"new_pw"};
//Push notification device registration.
inline constexpr QLatin1StringView Token{"token"};
inline constexpr QLatin1StringView Hardware{"hardware"};

//--- Answer envelope and long polling ---------------------------------------
inline constexpr QLatin1StringView Success{"success"};
inline constexpr QLatin1StringView Uuid{"uuid"};
inline constexpr QLatin1StringView Events{"events"};

//--- get_home ---------------------------------------------------------------
inline constexpr QLatin1StringView Home{"home"};
inline constexpr QLatin1StringView Cameras{"cameras"};
inline constexpr QLatin1StringView Audio{"audio"};

//--- Shared by rooms, IOs, cameras and audio players ------------------------
inline constexpr QLatin1StringView Id{"id"};
inline constexpr QLatin1StringView Name{"name"};
inline constexpr QLatin1StringView Type{"type"};
inline constexpr QLatin1StringView Hits{"hits"};
inline constexpr QLatin1StringView State{"state"};
inline constexpr QLatin1StringView Value{"value"};

//--- Room contents ----------------------------------------------------------
//v2 nests two lists under "items", v3 puts a flat list there.
inline constexpr QLatin1StringView Items{"items"};
inline constexpr QLatin1StringView Inputs{"inputs"};
inline constexpr QLatin1StringView Outputs{"outputs"};

//--- IO description ---------------------------------------------------------
inline constexpr QLatin1StringView GuiType{"gui_type"};
inline constexpr QLatin1StringView IoStyle{"io_style"};
inline constexpr QLatin1StringView Unit{"unit"};
inline constexpr QLatin1StringView Rw{"rw"};
inline constexpr QLatin1StringView Visible{"visible"};
inline constexpr QLatin1StringView ValueWarning{"value_warning"};

//--- IO status_info ---------------------------------------------------------
inline constexpr QLatin1StringView StatusInfo{"status_info"};
inline constexpr QLatin1StringView BatteryLevel{"battery_level"};
inline constexpr QLatin1StringView Connected{"connected"};
inline constexpr QLatin1StringView WirelessSignal{"wireless_signal"};
inline constexpr QLatin1StringView Uptime{"uptime"};
inline constexpr QLatin1StringView IpAddress{"ip_address"};
inline constexpr QLatin1StringView WifiSsid{"wifi_ssid"};

//--- Cameras ----------------------------------------------------------------
inline constexpr QLatin1StringView CameraId{"camera_id"};
inline constexpr QLatin1StringView Ptz{"ptz"};
//Only sent by a v1 calaos-os, and the marker CameraItem detects it with.
inline constexpr QLatin1StringView UrlLowres{"url_lowres"};

//--- Audio players ----------------------------------------------------------
inline constexpr QLatin1StringView AudioPlayers{"audio_players"};
inline constexpr QLatin1StringView AudioAction{"audio_action"};
inline constexpr QLatin1StringView PlayerId{"player_id"};
inline constexpr QLatin1StringView Status{"status"};
inline constexpr QLatin1StringView Volume{"volume"};
inline constexpr QLatin1StringView TimeElapsed{"time_elapsed"};
inline constexpr QLatin1StringView CurrentTrack{"current_track"};
inline constexpr QLatin1StringView Title{"title"};
inline constexpr QLatin1StringView Album{"album"};
inline constexpr QLatin1StringView Artist{"artist"};
inline constexpr QLatin1StringView Genre{"genre"};
inline constexpr QLatin1StringView Year{"year"};
inline constexpr QLatin1StringView Duration{"duration"};

//--- Event log --------------------------------------------------------------
inline constexpr QLatin1StringView Page{"page"};
inline constexpr QLatin1StringView PerPage{"per_page"};
//Presence of "total_page" next to "events" is what tells an event log answer
//apart from any other answer carrying events.
inline constexpr QLatin1StringView TotalPage{"total_page"};
inline constexpr QLatin1StringView EventType{"event_type"};
inline constexpr QLatin1StringView EventRaw{"event_raw"};
inline constexpr QLatin1StringView Message{"message"};
inline constexpr QLatin1StringView PicUid{"pic_uid"};
inline constexpr QLatin1StringView CreatedAt{"created_at"};
inline constexpr QLatin1StringView IoId{"io_id"};
inline constexpr QLatin1StringView IoState{"io_state"};

}

#endif // JSONKEYS_H
