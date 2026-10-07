#!/usr/bin/env python3
"""Build the CA bundle embedded in the firmware (src/net/ca_bundle.bin).

arduino-esp32 2.0.x ships the bundle *loader* but not a bundle blob, so we
generate one from the Mozilla root store (https://curl.se/ca/cacert.pem) in
the ESP-IDF x509 bundle format: >H count, then per cert (sorted by subject
DER) >H name_len, >H key_len, subject DER, SubjectPublicKeyInfo DER.

Flash is tight (app bin <= 1,572,864 bytes), so the shipped bundle keeps only
the roots Funnel/Let's Encrypt and the common public CAs chain to (see KEEP).
Pass --all to build the full ~55 KB Mozilla store instead.

Usage: python scripts/gen_ca_bundle.py [--all] cacert.pem src/net/ca_bundle.bin
Needs: pip install cryptography
"""
import struct
import sys

from cryptography import x509
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat


KEEP = (
    "ISRG Root X1", "ISRG Root X2", "ISRG Root YE", "ISRG Root YR",
    "GTS Root R1", "GTS Root R2", "GTS Root R3", "GTS Root R4",
    "DigiCert Global Root G2", "DigiCert Global Root G3",
    "USERTrust RSA Certification Authority", "USERTrust ECC Certification Authority",
)


def main(pem_path: str, out_path: str, keep_all: bool = False) -> None:
    data = open(pem_path, "rb").read()
    certs = x509.load_pem_x509_certificates(data)
    entries = {}
    for c in certs:
        cn = " ".join(a.value for a in c.subject.get_attributes_for_oid(x509.NameOID.COMMON_NAME))
        if not keep_all and cn not in KEEP:
            continue
        name = c.subject.public_bytes()
        key = c.public_key().public_bytes(Encoding.DER, PublicFormat.SubjectPublicKeyInfo)
        entries[(name, key)] = None
    ordered = sorted(entries, key=lambda e: e[0])
    blob = struct.pack(">H", len(ordered))
    for name, key in ordered:
        blob += struct.pack(">HH", len(name), len(key)) + name + key
    open(out_path, "wb").write(blob)
    print(f"{len(ordered)} roots, {len(blob)} bytes -> {out_path}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--all"]
    main(args[0], args[1], "--all" in sys.argv)
