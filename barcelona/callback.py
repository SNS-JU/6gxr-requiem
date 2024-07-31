#!/usr/bin/env python3
"""An example HTTP server with GET and POST endpoints."""

# Copyright (C) 2024 by its authors (See AUTHORS).  Licensed under the
# EUPL-1.2 or later.  See LICENSE for the exact licensing conditions.

from http.server import HTTPServer, BaseHTTPRequestHandler
from http import HTTPStatus
import json
import time
import argparse
import subprocess


# Sample blog post data similar to
# https://ordina-jworks.github.io/frontend/2019/03/04/vue-with-typescript.html#4-how-to-write-your-first-component
_g_posts = [
    {
        'title': 'My first blogpost ever!',
        'body': 'Lorem ipsum dolor sit amet.',
        'author': 'Elke',
        'date_ms': 1593607500000,  # 2020 July 1 8:45 AM Eastern
    },
    {
        'title': 'Look I am blogging!',
        'body': 'Hurray for me, this is my second post!',
        'author': 'Elke',
        'date_ms': 1593870300000, # 2020 July 4 9:45 AM Eastern
    },
    {
        'title': 'Another one?!',
        'body': 'Another one!',
        'author': 'Elke',
        'date_ms': 1594419000000, # 2020 July 10 18:10 Eastern
    }
]


class _RequestHandler(BaseHTTPRequestHandler):
    # Borrowing from https://gist.github.com/nitaku/10d0662536f37a087e1b
    def _set_headers(self):
        self.send_response(HTTPStatus.OK.value)
        self.send_header('Content-type', 'application/json')
        # Allow requests from any origin, so CORS policies don't
        # prevent local development.
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()

    def do_GET(self):
        self._set_headers()
        self.wfile.write(json.dumps(_g_posts).encode('utf-8'))

    def do_POST(self):
        length = int(self.headers.get('content-length'))
        message = json.loads(self.rfile.read(length))
        message['date_ms'] = int(time.time()) * 1000
        _g_posts.append(message)
        self._set_headers()
        self.wfile.write(json.dumps({'success': True}).encode('utf-8'))
        print('triggering the migration')
        subprocess.check_output(['qdbus', '--literal org.freedesktop.gst',
                                 '/quic', 'org.freedesktop.gst.Rebind', args.addr])


    def do_OPTIONS(self):
        # Send allow-origin header for preflight POST XHRs.
        self.send_response(HTTPStatus.NO_CONTENT.value)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST')
        self.send_header('Access-Control-Allow-Headers', 'content-type')
        self.end_headers()


def run_server():
    server_address = ('', 8001)
    httpd = HTTPServer(server_address, _RequestHandler)
    print('serving at %s:%d' % server_address)
    httpd.serve_forever()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("addr", help="addr to migrate to")
    args = parser.parse_args()
    run_server()

