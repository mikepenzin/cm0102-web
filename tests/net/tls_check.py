#!/usr/bin/env python3
"""The secure connection (0.7.0 slice 5, src/recomp_rt/win/secure.c) against a local TLS server.

Usage: python3 tests/net/tls_check.py <relay_test>

The script makes a test root certificate and a server certificate for "localhost" with the openssl tool, serves
GET /v1/health over TLS on localhost, and runs `relay_test --https <address>`:
    1 with the test root (RECOMP_TEST_TLS_CA): the lobby call succeeds
    2 without it: the call fails, because the system does not trust the test root
    3 with the test root, at 127.0.0.1: the call fails, because the certificate names localhost only
"""
import http.server
import os
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading


class Health(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'{"ok":true,"api":1}'
        self.send_response(200)
        self.send_header("X-Bosman-Api", "1")
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def make_certificates(d):
    def run(*args):
        subprocess.run(["openssl", *args], cwd=d, check=True, capture_output=True)
    with open(os.path.join(d, "ca.cnf"), "w") as f:
        f.write("[req]\ndistinguished_name=dn\n[dn]\n[ca]\nbasicConstraints=critical,CA:TRUE\n"
                "keyUsage=critical,keyCertSign,cRLSign\n[server]\nbasicConstraints=CA:FALSE\n"
                "keyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n")
    run("ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", "ca.key")
    run("req", "-x509", "-new", "-key", "ca.key", "-out", "ca.pem", "-days", "2", "-subj", "/CN=Bosman Test Root",
        "-config", "ca.cnf", "-extensions", "ca")
    run("ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", "server.key")
    run("req", "-new", "-key", "server.key", "-out", "server.csr", "-subj", "/CN=localhost", "-config", "ca.cnf")
    run("x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key", "-CAcreateserial",
        "-out", "server.pem", "-days", "2", "-extfile", "ca.cnf", "-extensions", "server")


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    if not shutil.which("openssl"):
        print("tls check: no openssl tool, skipped")
        return 77                           # ctest SKIP_RETURN_CODE
    program = os.path.abspath(argv[1])
    with tempfile.TemporaryDirectory(prefix="cm0102-tls-") as d:
        make_certificates(d)
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Health)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(os.path.join(d, "server.pem"), os.path.join(d, "server.key"))
        server.socket = context.wrap_socket(server.socket, server_side=True)
        port = server.server_address[1]
        threading.Thread(target=server.serve_forever, daemon=True).start()
        failed = []

        def check(name, address, ca, want, why=None):
            env = dict(os.environ)
            env.pop("RECOMP_TEST_TLS_CA", None)
            if ca:
                env["RECOMP_TEST_TLS_CA"] = os.path.join(d, "ca.pem")
            r = subprocess.run([program, "--https", address], env=env, capture_output=True, text=True, timeout=30)
            # A failure must be the certificate check, not some other failure.
            ok = (r.returncode == 0) == want and (not why or why in r.stderr)
            print(("ok   " if ok else "FAIL ") + name)
            if not ok:
                failed.append(name)
                print(r.stdout + r.stderr)

        check("1 the test root: the lobby call succeeds", f"https://localhost:{port}", True, True)
        check("2 no test root: the server is not trusted", f"https://localhost:{port}", False, False,
              "certificate is not trusted")
        check("3 another name: the certificate does not match", f"https://127.0.0.1:{port}", True, False,
              "certificate is not trusted")
        server.shutdown()
    print("tls check: " + ("PASS" if not failed else f"FAIL ({len(failed)})"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
