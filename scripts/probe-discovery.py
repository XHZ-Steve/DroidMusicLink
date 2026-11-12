"""Emit an explicitly labelled local DNS-SD test fixture for the real WSS relay.

Run only during discovery verification. This does not change product data/code or
claim that an emulator is a LAN device. Repeated announcements exercise URL dedup.
"""
import argparse
import hashlib
import socket
import ssl
import struct
import time


def dns_name(value):
    return b"".join(bytes([len(part)]) + part.encode("utf-8") for part in value.split(".")) + b"\0"


def record(owner, kind, data):
    return dns_name(owner) + struct.pack("!HHIH", kind, 1, 30, len(data)) + data


def packet(host, port, fingerprint):
    service = "_droidmedialink._tcp.local"
    instance = "Android emulator TEST." + service
    target = "dml-emulator-test.local"
    fields = ["name=Android emulator TEST", "deviceId=" + fingerprint, "pairingVersion=2"]
    txt = b"".join(bytes([len(field)]) + field.encode() for field in fields)
    answers = [record(service, 12, dns_name(instance)),
               record(instance, 16, txt),
               record(instance, 33, struct.pack("!HHH", 0, 0, port) + dns_name(target)),
               record(target, 1, socket.inet_aton(host))]
    return struct.pack("!HHHHHH", 0, 0x8400, 0, len(answers), 0, 0) + b"".join(answers)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18123)
    parser.add_argument("--repeat", type=int, default=12)
    args = parser.parse_args()
    # Public certificate only; no application identity/token is read or sent.
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    with socket.create_connection((args.host, args.port), timeout=5) as raw:
        with context.wrap_socket(raw, server_hostname=args.host) as tls:
            fingerprint = hashlib.sha256(tls.getpeercert(binary_form=True)).hexdigest()
    data = packet(args.host, args.port, fingerprint)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        for _ in range(args.repeat):
            udp.sendto(data, ("127.0.0.1", 5353))
            udp.sendto(data, ("224.0.0.251", 5353))
            time.sleep(0.15)
    print(f"Sent {args.repeat} identical DNS-SD test announcements: Android emulator TEST -> wss://{args.host}:{args.port}")


if __name__ == "__main__":
    main()
