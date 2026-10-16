#!/usr/bin/env python3
"""Minimal stand-in for calaos_server's api.php, for the Android push picture
path (AND-7, PushPictureWorker) on an emulator: no calaos_server needed.

Serves, like JsonApiHandlerHttp:
  GET /api.php?cn_user=U&cn_pass=P&action=eventlog&uuid=<event_uuid>
      -> 200, the event as JSON with a "pic_uid" (HistEvent::toJson)
  GET /api.php?cn_user=U&cn_pass=P&action=event_picture&pic_uid=<pic_uid>
      -> 200, the picture given with --picture (image/jpeg)
  wrong or missing credentials -> 400 (checked before the action)
  --no-picture: eventlog answers with an empty pic_uid (rule without camera)
  --delay N: serve the picture after N seconds (text-first, then picture)
  POST /api.php (JSON body, the app's HTTP transport): get_home answers an
      empty home, register_push logs the fields it received (token redacted),
      poll_listen hands out a uuid then holds the long poll, so the app can
      log in against this server too.

From the Android emulator the Mac is 10.0.2.2: configure the app with the
host "http://10.0.2.2:8080/api.php" and the same user/password. Credentials
are never printed (only whether they matched).

Development tool only; not part of the app.
"""

import argparse
import hashlib
import http.server
import json
import sys
import time
import urllib.parse


class Handler(http.server.BaseHTTPRequestHandler):
    args = None

    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        q = urllib.parse.parse_qs(url.query)
        get = lambda k: q.get(k, [""])[0]  # noqa: E731

        if url.path != "/api.php":
            return self._send(404, b"not found", "text/plain")

        if get("cn_user") != self.args.user or get("cn_pass") != self.args.password:
            self.log_message("action=%s credentials=WRONG -> 400", get("action"))
            return self._send(400, b"bad request", "text/html")

        action = get("action")
        if action == "eventlog":
            uuid = get("uuid")
            pic_uid = "" if self.args.no_picture else hashlib.sha1(uuid.encode()).hexdigest()[:16]
            body = json.dumps({
                "id": uuid,
                "event_type": "3",
                "io_id": "camera_1",
                "io_state": "",
                "pic_uid": pic_uid,
                "created_at": "2026-10-16 12:00:00",
                "event_raw": {},
            }).encode()
            self.log_message("action=eventlog uuid=%s credentials=ok -> pic_uid=%r", uuid, pic_uid)
            return self._send(200, body, "application/json")

        if action == "event_picture":
            if self.args.delay:
                time.sleep(self.args.delay)
            self.log_message("action=event_picture pic_uid=%s credentials=ok -> %d bytes",
                             get("pic_uid"), len(self.args.picture_bytes))
            return self._send(200, self.args.picture_bytes, "image/jpeg")

        return self._send(400, b"unknown action", "text/plain")

    def do_POST(self):
        if urllib.parse.urlsplit(self.path).path != "/api.php":
            return self._send(404, b"not found", "text/plain")
        length = int(self.headers.get("Content-Length", "0"))
        try:
            j = json.loads(self.rfile.read(length) or b"{}")
        except ValueError:
            j = {}
        if j.get("cn_user") != self.args.user or j.get("cn_pass") != self.args.password:
            self.log_message("POST action=%s credentials=WRONG -> 400", j.get("action"))
            return self._send(400, b"<html>Bad request</html>", "text/html")

        action = j.get("action")
        if action == "get_home":
            self.log_message("POST action=get_home credentials=ok -> empty home")
            body = {"success": "true", "home": [], "cameras": [], "audio": []}
        elif action == "register_push":
            tok = str(j.get("token", ""))
            self.log_message("POST action=register_push hardware=%r push_format=%r token=%s... (%d chars)",
                             j.get("hardware"), j.get("push_format"), tok[:8], len(tok))
            body = {"success": "true"}
        elif action == "poll_listen" and j.get("type") == "register":
            body = {"success": "true", "uuid": "fake-poll-uuid"}
        elif action == "poll_listen":
            time.sleep(25)  # long poll with no event
            body = {"success": "true", "events": []}
        else:
            body = {"success": "true"}
        return self._send(200, json.dumps(body).encode(), "application/json")

    def _send(self, status, body, ctype):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    # Default log line would print the full request path, credentials included.
    def log_request(self, code="-", size="-"):
        pass


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", type=int, default=8080)
    p.add_argument("--user", required=True)
    p.add_argument("--password", required=True)
    p.add_argument("--picture", required=True, help="JPEG file served as the event picture")
    p.add_argument("--no-picture", action="store_true", help="eventlog answers with an empty pic_uid")
    p.add_argument("--delay", type=float, default=0.0,
                   help="seconds to wait before serving the picture (to see the text notification first)")
    args = p.parse_args()
    with open(args.picture, "rb") as f:
        args.picture_bytes = f.read()

    Handler.args = args
    server = http.server.ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    print(f"fake api.php on port {args.port} (emulator: http://10.0.2.2:{args.port}/api.php)", file=sys.stderr)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
