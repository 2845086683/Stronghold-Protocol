"""Lossless, versioned storage for immutable multiplayer ACKs.

The HTTP reply and its idempotency fingerprint do not change. Old TEXT rows
remain readable; new large ACKs use SQLite BLOBs to avoid rewriting hundreds
of kilobytes of repeated DTO JSON for every heartbeat. No receipt is expired.

An old server cannot read this BLOB format. Before rolling back its code, run
the bounded ``decode_legacy_compatible_batch`` until ``complete`` is true.
The maintenance API never changes fingerprints, keys, saves, or profiles.
"""
import hashlib
import json
import time
import zlib

from .storage import dump

PREFIX = b'SP-ACK\x00\x01'
MIN_BYTES = 4096


def encode_text(text):
    raw = text.encode('utf8')
    if len(raw) < MIN_BYTES:
        return text
    packed = PREFIX + zlib.compress(raw, 1)
    return packed if len(packed) < len(raw) else text


def decode_text(value, *, max_bytes=None):
    if isinstance(value, bytes) and value.startswith(PREFIX):
        if max_bytes is not None:
            if type(max_bytes) is not int or max_bytes < 1:
                raise ValueError('invalid receipt decoding bound')
            decoder = zlib.decompressobj()
            raw = decoder.decompress(value[len(PREFIX):], max_bytes + 1)
            if len(raw) > max_bytes or not decoder.eof:
                raise ValueError('receipt exceeds decoding bound')
            return raw.decode('utf8')
        return zlib.decompress(value[len(PREFIX):]).decode('utf8')
    if max_bytes is not None and len(value.encode('utf8') if isinstance(value, str) else value) > max_bytes:
        raise ValueError('receipt exceeds decoding bound')
    return value


def encode_reply(reply):
    return encode_text(dump(reply))


def decode_reply(value):
    return json.loads(decode_text(value))


def _batch(db, *, decode, after_rowid=0, max_rows=16, max_bytes=2 * 1024 * 1024,
           max_seconds=0.15):
    """One bounded transaction; persist returned cursor between invocations.

    The byte limit applies before reading/allocating an individual value. An
    oversized row is left untouched and reported explicitly; the caller must
    increase its budget deliberately. ``complete`` means no matching rows
    remain after this cursor, not permission to roll back yet: rollback must
    additionally check ``compressed_receipts_remaining(db) == 0`` while the
    service is stopped. Running writers can add new compressed ACKs.
    """
    if (type(after_rowid) is not int or after_rowid < 0 or type(max_rows) is not int or
            not 1 <= max_rows <= 128 or type(max_bytes) is not int or
            not 1 <= max_bytes <= 8 * 1024 * 1024 or
            not isinstance(max_seconds, (int, float)) or not 0 < max_seconds <= 1):
        raise ValueError('invalid receipt maintenance bounds')
    if db.in_transaction:
        raise ValueError('receipt maintenance owns its transaction')
    started = time.monotonic(); cursor = after_rowid
    count = read_bytes = stored_before = stored_after = 0
    logical_before = hashlib.sha256(); logical_after = hashlib.sha256()
    oversized = None
    kind = 'blob' if decode else 'text'
    db.execute('BEGIN IMMEDIATE')
    try:
        while count < max_rows and time.monotonic() - started < max_seconds:
            row = db.execute('SELECT rowid,length(CAST(reply AS BLOB)) FROM multiplayer_receipts '
                'WHERE rowid>? AND typeof(reply)=? ORDER BY rowid LIMIT 1', (cursor, kind)).fetchone()
            if row is None:
                break
            ident, size = row
            if size > max_bytes - read_bytes:
                if count == 0:
                    oversized = {'rowid': ident, 'storedBytes': size}
                break
            old = db.execute('SELECT reply FROM multiplayer_receipts WHERE rowid=?', (ident,)).fetchone()[0]
            if decode and not old.startswith(PREFIX):
                # Never reinterpret an unrelated future/foreign BLOB.
                raise ValueError('unknown compressed receipt format')
            if decode:
                decoder = zlib.decompressobj()
                raw = decoder.decompress(old[len(PREFIX):], max_bytes - read_bytes + 1)
                if len(raw) > max_bytes - read_bytes or not decoder.eof:
                    if count == 0:
                        oversized = {'rowid': ident, 'storedBytes': size,
                            'decodedBytesGreaterThan': max_bytes - read_bytes}
                    break
                text = raw.decode('utf8')
            else:
                text = old
            new = text if decode else encode_text(text)
            raw = text.encode('utf8')
            logical_before.update(str(ident).encode() + b'\x00' + raw)
            logical_after.update(str(ident).encode() + b'\x00' + decode_text(new).encode('utf8'))
            db.execute('UPDATE multiplayer_receipts SET reply=? WHERE rowid=?', (new, ident))
            cursor = ident; count += 1; read_bytes += len(raw)
            stored_before += size
            stored_after += len(new) if isinstance(new, bytes) else len(new.encode('utf8'))
        complete = db.execute('SELECT 1 FROM multiplayer_receipts WHERE rowid>? AND typeof(reply)=? LIMIT 1',
            (cursor, kind)).fetchone() is None
        before_sha, after_sha = logical_before.hexdigest(), logical_after.hexdigest()
        if before_sha != after_sha:
            raise AssertionError('receipt logical content changed')
        db.commit()
    except Exception:
        db.rollback(); raise
    return {'rows': count, 'cursor': cursor, 'readBytes': read_bytes,
        'storedBytesBefore': stored_before, 'storedBytesAfter': stored_after,
        'logicalSHA256Before': before_sha, 'logicalSHA256After': after_sha,
        'elapsedSeconds': time.monotonic() - started, 'complete': complete,
        'oversized': oversized}


def encode_legacy_receipts(db, **bounds):
    return _batch(db, decode=False, **bounds)


def decode_legacy_compatible_batch(db, **bounds):
    return _batch(db, decode=True, **bounds)


def compressed_receipts_remaining(db):
    # This scans only the SQLite type/header, without decoding any ACK JSON.
    return db.execute("SELECT count(*) FROM multiplayer_receipts WHERE typeof(reply)='blob'").fetchone()[0]
