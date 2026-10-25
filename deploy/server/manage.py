"""Stdlib-only deployment control. No game/client lifecycle and no remote admin API."""
from __future__ import annotations

import argparse
import contextlib
import csv
import datetime as dt
import hashlib
import ipaddress
import json
import logging
from logging.handlers import RotatingFileHandler
import os
from pathlib import Path
import re
import secrets
import signal
import sqlite3
import stat
import subprocess
import sys
import threading
import time
import urllib.request
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_STATE = ROOT / 'local-server' / 'deployment'
PUBLIC_PREFIX = '/bridge/act2autochess/multiplayer/'
STARTED_PROCESSES = {}
OWNER_MARKER = '.stronghold-deployment-owner.json'
sys.path.insert(0, str(Path(__file__).resolve().parent))
import network_info


def emit(value):
    print(json.dumps(value, ensure_ascii=False), flush=True)


def checked_path(path):
    raw = Path(path)
    if '..' in raw.parts:
        raise RuntimeError('parent traversal is not allowed in a deployment directory')
    path = Path(os.path.abspath(raw))
    if path.parent == path:
        raise RuntimeError('a filesystem root cannot be a private state directory')
    # Inspect the original path before resolve(), which would conceal a
    # symlink/junction. Every existing ancestor must also be a real directory.
    for part in reversed((path, *path.parents)):
        try: info = part.lstat()
        except FileNotFoundError: continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
            raise RuntimeError('deployment directory crosses a symlink/reparse point')
        if not stat.S_ISDIR(info.st_mode):
            raise RuntimeError('deployment directory crosses a non-directory')
    return path


def verify_owner(path):
    marker = path / OWNER_MARKER
    info = marker.lstat()
    if not stat.S_ISREG(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400 or info.st_size > 2048:
        raise RuntimeError('invalid deployment ownership marker')
    value = read_json(marker)
    if (not isinstance(value, dict) or set(value) != {'product', 'format', 'directoryId'} or value.get('product') != 'StrongholdProtocol'
            or value.get('format') != 1 or not isinstance(value.get('directoryId'), str)
            or not re.fullmatch(r'[0-9a-f]{32}', value['directoryId'])):
        raise RuntimeError('invalid deployment ownership marker')


def owned_read_directory(path):
    """Validate an existing source without creating files or changing its ACL."""
    path = checked_path(path)
    if not path.is_dir():
        raise RuntimeError('owned source directory is missing')
    try: verify_owner(path)
    except FileNotFoundError:
        raise RuntimeError('source directory is not an owned deployment directory') from None
    return path


def private_dir(path):
    path = checked_path(path)
    if path.exists():
        if (path / OWNER_MARKER).exists() or (path / OWNER_MARKER).is_symlink():
            verify_owner(path)
        elif any(path.iterdir()):
            # Never tighten ACLs on an unrelated non-empty directory selected
            # by mistake. No file or permission has been changed at this point.
            raise RuntimeError('non-empty directory is not an owned deployment directory')
    path.mkdir(parents=True, exist_ok=True, mode=0o700)
    checked_path(path)
    marker = path / OWNER_MARKER
    if not marker.exists():
        descriptor = os.open(marker, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
            json.dump({'product': 'StrongholdProtocol', 'format': 1, 'directoryId': secrets.token_hex(16)}, stream)
    verify_owner(path)
    if os.name == 'nt':
        row = next(csv.reader(subprocess.check_output(
            ['whoami', '/user', '/fo', 'csv', '/nh'], text=True, encoding='utf-8', errors='replace').splitlines()))
        sid = row[-1]
        if not re.fullmatch(r'S-1-[0-9-]+', sid):
            raise RuntimeError('cannot determine current Windows SID')
        subprocess.run(['icacls', str(path), '/inheritance:r', '/grant:r',
                        '*' + sid + ':(OI)(CI)F', '*S-1-5-18:(OI)(CI)F'],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    else:
        path.chmod(0o700)
    return path


def atomic_json(path, value):
    temp = path.with_name(path.name + '.' + secrets.token_hex(6) + '.tmp')
    with temp.open('x', encoding='utf-8') as stream:
        if os.name != 'nt':
            os.fchmod(stream.fileno(), 0o600)
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp, path)


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


@contextlib.contextmanager
def instance_lock(state):
    # Kernel locks release on process death. No stale PID can authorize a kill.
    stream = (state / 'server.lock').open('a+b')
    stream.seek(0)
    if os.name == 'nt':
        import msvcrt
        stream.seek(0)
        try:
            msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        except OSError:
            stream.close()
            raise RuntimeError('this state directory is already running or being restored') from None
        if os.fstat(stream.fileno()).st_size == 0:
            stream.write(b'0'); stream.flush()
    else:
        import fcntl
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            stream.close()
            raise RuntimeError('this state directory is already running or being restored') from None
    try:
        yield
    finally:
        stream.close()


def operator_token(state):
    folder = private_dir(state / 'secrets')
    target = folder / 'operator.token'
    if not target.exists():
        descriptor = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, 'w', encoding='ascii') as stream:
            stream.write(secrets.token_urlsafe(48) + '\n')
    if os.name != 'nt':
        target.chmod(0o600)
    token = target.read_text(encoding='ascii').strip()
    if len(token) < 32 or not all(33 <= ord(c) <= 126 for c in token):
        raise RuntimeError('invalid local operator token file')
    return token


def file_hash(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            digest.update(block)
    return digest.hexdigest()


def snapshot_database(source, target):
    if source.is_symlink() or not stat.S_ISREG(source.stat().st_mode):
        raise RuntimeError('database source is not a regular owned file')
    with contextlib.closing(sqlite3.connect(source.resolve().as_uri() + '?mode=ro', uri=True)) as src:
        with contextlib.closing(sqlite3.connect(target)) as dst:
            src.backup(dst, pages=256, sleep=0.05)
    if os.name != 'nt': target.chmod(0o600)
    database_check(target)


def package_identity():
    sources = {path.name: file_hash(path) for path in sorted((ROOT / 'src/StrongholdServer/stronghold_server').glob('*.py'))}
    repair = file_hash(ROOT / 'server-data/cn-autochess-blackboard-repair.json')
    sources['data:cn-autochess-blackboard-repair.json'] = repair
    build_id = hashlib.sha256(json.dumps(sources, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    return {'catalogSha256': file_hash(ROOT / 'server-data/cn-autochess.json'),
            'repairSha256': repair, 'serverBuildId': build_id}


def database_check(path):
    with contextlib.closing(sqlite3.connect(path.resolve().as_uri() + '?mode=ro', uri=True)) as db:
        result = db.execute('PRAGMA quick_check').fetchall()
        if result != [('ok',)]:
            raise RuntimeError('SQLite integrity check failed')


def backup(state, destination=None):
    state = private_dir(state)
    source = state / 'stronghold.sqlite3'
    if not source.is_file():
        raise RuntimeError('database does not exist; nothing to back up')
    base = private_dir(destination or state / 'backups')
    name = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ') + '-' + secrets.token_hex(4)
    output = private_dir(base / name)
    target = output / 'stronghold.sqlite3'
    with contextlib.closing(sqlite3.connect(source.resolve().as_uri() + '?mode=ro', uri=True)) as src:
        with contextlib.closing(sqlite3.connect(target)) as dst:
            src.backup(dst, pages=256, sleep=0.05)
    if os.name != 'nt':
        target.chmod(0o600)
    database_check(target)
    manifest = {'format': 1, 'createdUtc': dt.datetime.now(dt.timezone.utc).isoformat(),
                'databaseSha256': file_hash(target), **package_identity(),
                'operatorTokenIncluded': False, 'participantCredentialsInDatabase': True}
    participant_root = checked_path(state / 'participant-solo')
    participant_rows = []
    if participant_root.exists():
        for folder in sorted(participant_root.iterdir()):
            if not re.fullmatch(r'[0-9a-f]{32}', folder.name):
                raise RuntimeError('unexpected participant database directory')
            checked_path(folder)
            database = folder / 'stronghold.sqlite3'
            if not database.exists(): continue
            destination = private_dir(output / 'participant-solo' / folder.name) / 'stronghold.sqlite3'
            snapshot_database(database, destination)
            participant_rows.append({'actor': folder.name, 'databaseSha256': file_hash(destination)})
    manifest['participantSoloDatabases'] = participant_rows
    atomic_json(output / 'manifest.json', manifest)
    return output


def restore(state, source):
    source = owned_read_directory(source)
    state = private_dir(state)
    manifest = read_json(source / 'manifest.json')
    incoming = source / 'stronghold.sqlite3'
    if manifest.get('format') != 1 or file_hash(incoming) != manifest.get('databaseSha256'):
        raise RuntimeError('backup manifest/hash mismatch')
    if any(manifest.get(key) != value for key, value in package_identity().items()):
        raise RuntimeError('backup code/catalog differs; restore using the matching release first')
    database_check(incoming)
    participant_rows = manifest.get('participantSoloDatabases', [])
    if not isinstance(participant_rows, list): raise RuntimeError('invalid participant backup index')
    participants = set()
    for row in participant_rows:
        if (not isinstance(row, dict) or set(row) != {'actor', 'databaseSha256'} or
                not isinstance(row['actor'], str) or not re.fullmatch(r'[0-9a-f]{32}', row['actor']) or row['actor'] in participants):
            raise RuntimeError('invalid participant backup identity')
        participants.add(row['actor'])
        folder = owned_read_directory(source / 'participant-solo' / row['actor'])
        database = folder / 'stronghold.sqlite3'
        if database.is_symlink() or file_hash(database) != row['databaseSha256']:
            raise RuntimeError('participant backup hash mismatch')
        database_check(database)
    with instance_lock(state):
        # Preserve the previous DB before changing any file. The connection
        # backup API includes committed WAL data; raw sqlite-file copies do not.
        previous = backup(state) if (state / 'stronghold.sqlite3').exists() else None
        temporary = state / ('restore-' + secrets.token_hex(8) + '.sqlite3')
        with contextlib.closing(sqlite3.connect(incoming.as_uri() + '?mode=ro', uri=True)) as src:
            with contextlib.closing(sqlite3.connect(temporary)) as dst:
                src.backup(dst)
        database_check(temporary)
        # All participant DBs are staged and checked before replacing any
        # current data. The old directory is retained, never recursively erased.
        participant_root = checked_path(state / 'participant-solo')
        staged = state / ('restore-participant-solo-' + secrets.token_hex(8))
        staged.mkdir(mode=0o700)
        for row in participant_rows:
            folder = staged / row['actor']; folder.mkdir(mode=0o700)
            snapshot_database(source / 'participant-solo' / row['actor'] / 'stronghold.sqlite3', folder / 'stronghold.sqlite3')
        if os.name != 'nt':
            temporary.chmod(0o600)
        # All names here are fixed children of the locked state directory.
        # No user-supplied archive paths are extracted or recursively deleted.
        for suffix in ('-wal', '-shm'):
            (state / ('stronghold.sqlite3' + suffix)).unlink(missing_ok=True)
        os.replace(temporary, state / 'stronghold.sqlite3')
        if participant_root.exists():
            os.replace(participant_root, state / ('retired-participant-solo-' + secrets.token_hex(8)))
        os.replace(staged, participant_root)
    return previous


def public_route(path):
    try: route = urlsplit(path).path
    except ValueError: return False
    return route in ('/health', '/activity/autochessSeason/multiBattleStart', '/activity/autochessSeason/multiBattleFinish') or route.startswith(PUBLIC_PREFIX)


def request_route_label(path):
    """A fixed vocabulary, never URL identifiers, queries or user text."""
    try: route = urlsplit(path).path
    except ValueError: return 'invalid'
    known = {'/health': 'health',
        '/activity/autochessSeason/multiBattleStart': 'native.multiBattleStart',
        '/activity/autochessSeason/multiBattleFinish': 'native.multiBattleFinish',
        '/activity/autochessSeason/settleGame': 'native.settleGame',
        '/activity/autochessSeason/syncInfo': 'native.syncInfo'}
    if route in known: return known[route]
    if route.startswith(PUBLIC_PREFIX):
        parts = route[len(PUBLIC_PREFIX):].split('/')
        if len(parts) == 5 and parts[0] == 'rooms' and parts[2] == 'native-settlement': return 'room.native-settlement'
        if len(parts) == 3 and parts[0] == 'rooms' and parts[2] == 'commands': return 'room.commands'
        if len(parts) == 2 and parts[0] == 'rooms': return 'room.read-or-join'
        return 'multiplayer.other'
    return 'other'


def serve(state, host, port, public_only):
    if sys.version_info < (3, 12):
        raise RuntimeError('the packaged server requires Python 3.12 or newer')
    address = ipaddress.ip_address(host)
    if address.version != 4 or (not public_only and not address.is_loopback):
        raise RuntimeError('remote listeners must use --public-only; local legacy listener is loopback-only')
    logs = private_dir(state / 'logs')
    logger = logging.getLogger('stronghold-deployment'); logger.setLevel(logging.INFO)
    handler = RotatingFileHandler(logs / 'server.log', maxBytes=5 * 1024 * 1024, backupCount=5, encoding='utf-8')
    handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
    console = logging.StreamHandler(sys.stdout); console.setFormatter(handler.formatter)
    logger.addHandler(handler); logger.addHandler(console)
    with instance_lock(state):
        token = operator_token(state)
        sys.path.insert(0, str(ROOT / 'src/StrongholdServer'))
        from stronghold_server.catalog import Catalog
        from stronghold_server.engine import Engine
        from stronghold_server.protocol import Protocol
        from stronghold_server.storage import Store
        from stronghold_server.http_service import make_server
        catalog = Catalog(ROOT / 'server-data/cn-autochess.json')
        store = Store(state / 'stronghold.sqlite3', Engine(catalog))
        server = None
        stop = threading.Event()
        thread = None
        try:
            # Local solo remains the existing loopback-only contract. Do not
            # give an operator credential to the game. Public mode denies all
            # legacy routes even to its container-local caller.
            server = make_server(store, Protocol(catalog), port, host=host, access_token=token if public_only else None)
            server.daemon_threads = False
            original_setup = server.RequestHandlerClass.setup
            def setup(request):
                request.request.settimeout(10)
                return original_setup(request)
            server.RequestHandlerClass.setup = setup
            original_auth = server.RequestHandlerClass.authorize_route
            original_reply = server.RequestHandlerClass.reply
            def authorize(request):
                if public_only and not public_route(request.path):
                    request.reply(404, {'ok': False, 'error': 'unknown_route', 'officialFallback': False})
                    return False
                return original_auth(request)
            def reply(request, status, value):
                try: route = urlsplit(request.path).path
                except ValueError: route = ''
                if route == '/health' and status == 200 and isinstance(value, dict):
                    value = {**value, 'publicMultiplayerOnly': public_only}
                if status >= 400 or (request.command != 'GET' and route != '/health'):
                    # Never log Authorization, request bodies, participant
                    # credentials, room codes, or URL query strings.
                    logger.log(logging.WARNING if status >= 400 else logging.INFO,
                        'request method=%s status=%d public=%s route=%s', request.command, status, public_only, request_route_label(request.path))
                return original_reply(request, status, value)
            server.RequestHandlerClass.authorize_route = authorize
            server.RequestHandlerClass.reply = reply
            instance = secrets.token_hex(24)
            atomic_json(state / 'process.json', {'pid': os.getpid(), 'instance': instance,
                        'host': host, 'port': server.server_port, 'publicOnly': public_only,
                        'root': str(ROOT), 'startedUtc': dt.datetime.now(dt.timezone.utc).isoformat()})
            for name in ('SIGINT', 'SIGTERM'):
                if hasattr(signal, name):
                    signal.signal(getattr(signal, name), lambda *_: stop.set())
            thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': 0.2}, daemon=True)
            thread.start()
            logger.info('ready pid=%d address=%s:%d publicOnly=%s', os.getpid(), host, server.server_port, public_only)
            network_info.show(logger, host, server.server_port, public_only, state)
            while thread.is_alive() and not stop.wait(0.2):
                request = state / 'stop.request'
                if request.exists():
                    try:
                        if read_json(request).get('instance') == instance:
                            stop.set()
                    finally:
                        request.unlink(missing_ok=True)
            logger.info('stopping pid=%d', os.getpid())
        finally:
            if server:
                if thread and thread.is_alive(): server.shutdown()
                server.server_close()
            store.close()
            (state / 'process.json').unlink(missing_ok=True)
            logger.removeHandler(handler); logger.removeHandler(console); handler.close(); console.close()


def live_record(state):
    record_path = state / 'process.json'
    if not record_path.is_file():
        return None
    record = read_json(record_path)
    host = record['host'] if record['host'] != '0.0.0.0' else '127.0.0.1'
    # Never use proxy environment settings for the local management probe.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    try:
        with opener.open('http://' + host + ':' + str(record['port']) + '/health', timeout=2) as response:
            health = json.load(response)
    except (OSError, ValueError):
        return None
    if health.get('pid') != record.get('pid') or health.get('purpose') != 'CN native activity private backend':
        raise RuntimeError('health endpoint belongs to a different process; no action taken')
    return record


def start(state, port, host='127.0.0.1', public_only=False):
    address=ipaddress.ip_address(host)
    if address.version!=4 or (not public_only and not address.is_loopback):
        raise RuntimeError('remote listeners must use --public-only; local legacy listener is loopback-only')
    current=live_record(state)
    if current:
        if current['host']!=host or current['publicOnly']!=public_only or (port and current['port']!=port):
            raise RuntimeError('this instance is running with different bind/port/mode; stop it before changing settings')
        return {'running': True, 'alreadyRunning': True, 'host':host, 'port':current['port'], 'publicOnly':public_only}
    previous = read_json(state / 'process.json').get('instance') if (state / 'process.json').is_file() else None
    with instance_lock(state):
        pass
    bootstrap = private_dir(state / 'logs') / 'bootstrap.log'
    if bootstrap.exists() and bootstrap.stat().st_size > 1048576:
        bootstrap.write_bytes(b'')
    command = [sys.executable, '-I', str(Path(__file__).resolve()), 'serve', '--state-dir', str(state), '--port', str(port), '--host', host]
    if public_only: command.append('--public-only')
    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {'start_new_session': True}
    with bootstrap.open('ab') as output:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output, stderr=output, **options)
    for _ in range(150):
        if process.poll() is not None:
            raise RuntimeError('server startup failed; inspect logs/bootstrap.log')
        current = live_record(state)
        # Windows venv launchers may own a different PID than the interpreter.
        # The kernel-held state lock + fresh local nonce + health PID check
        # identify the owned server without relying on the launcher PID.
        if current and current['instance'] != previous and current.get('root') == str(ROOT):
            STARTED_PROCESSES[current['instance']] = process
            rows,warning=network_info.adapters() if host=='0.0.0.0' else ([],None)
            info=network_info.connection_info(host,current['port'],rows)
            return {'running': True, 'pid': current['pid'], 'url': info['local'], 'host':host,'port':current['port'],
                    'publicOnly':public_only,'connections':info,'adapterWarning':warning,'logFile':str(state/'logs/server.log')}
        time.sleep(0.2)
    raise RuntimeError('startup health timed out; inspect logs before retrying')


def stop_server(state):
    record_file = state / 'process.json'
    if not record_file.is_file():
        return {'running': False}
    record = read_json(record_file)
    if not isinstance(record.get('instance'), str) or not re.fullmatch(r'[0-9a-f]{48}', record['instance']):
        raise RuntimeError('invalid local process record; no action taken')
    atomic_json(state / 'stop.request', {'instance': record['instance']})
    for _ in range(150):
        if not record_file.is_file() or read_json(record_file).get('instance') != record['instance']:
            child = STARTED_PROCESSES.pop(record['instance'], None)
            if child:
                child.wait(timeout=5)
            return {'running': False, 'stopped': True}
        time.sleep(0.2)
    raise RuntimeError('graceful stop timed out; no unrelated PID was terminated')


def configure(folder, domain, lan_address, python_image, caddy_image):
    folder = private_dir(folder)
    if bool(domain) == bool(lan_address):
        raise RuntimeError('choose exactly one --domain or --lan-address')
    for image, prefix in ((python_image, 'python:'), (caddy_image, 'caddy:')):
        if not re.fullmatch(re.escape(prefix) + r'[A-Za-z0-9_.-]+@sha256:[0-9a-f]{64}', image):
            raise RuntimeError('use an official version tag with its locked @sha256 digest; see README')
    if domain:
        if len(domain) > 253 or not re.fullmatch(r'(?=.{1,253}$)(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[A-Za-z]{2,63}', domain):
            raise RuntimeError('a plain DNS domain is required, without a scheme/path/port')
        mode, site, lan = 'public', domain.lower(), '127.0.0.1'
    else:
        address = ipaddress.ip_address(lan_address)
        networks = ('10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16', '127.0.0.0/8')
        if address.version != 4 or not any(address in ipaddress.ip_network(net) for net in networks):
            raise RuntimeError('LAN mode requires an explicit RFC1918/loopback IPv4 address; never 0.0.0.0/public IP')
        mode, site, lan = 'lan', 'http://:8080', str(address)
    config = {'mode': mode, 'site': site, 'lanAddress': lan, 'pythonImage': python_image, 'caddyImage': caddy_image}
    atomic_json(folder / 'deployment.json', config)
    values = {'STRONGHOLD_SITE': site, 'STRONGHOLD_LAN_ADDRESS': lan,
              'STRONGHOLD_PYTHON_IMAGE': python_image, 'STRONGHOLD_CADDY_IMAGE': caddy_image}
    env = folder / 'compose.env'
    with env.open('w', encoding='ascii', newline='\n') as stream:
        stream.write(''.join(key + '=' + value + '\n' for key, value in values.items()))
    if os.name != 'nt': env.chmod(0o600)
    return config


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('start', 'stop', 'status', 'serve', 'backup', 'restore', 'configure', 'logs'))
    parser.add_argument('--state-dir', type=Path, default=DEFAULT_STATE)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=16680)
    parser.add_argument('--public-only', action='store_true')
    parser.add_argument('--backup-dir', type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--config-dir', type=Path, default=Path(__file__).resolve().parent / 'instance')
    parser.add_argument('--domain'); parser.add_argument('--lan-address')
    parser.add_argument('--python-image'); parser.add_argument('--caddy-image')
    args = parser.parse_args()
    if args.action == 'configure':
        emit(configure(args.config_dir, args.domain, args.lan_address, args.python_image or '', args.caddy_image or '')); return
    state = private_dir(args.state_dir)
    if not 0 <= args.port <= 65535: raise RuntimeError('invalid TCP port')
    if args.action == 'serve': serve(state, args.host, args.port, args.public_only)
    elif args.action == 'start': emit(start(state, args.port, args.host, args.public_only))
    elif args.action == 'stop': emit(stop_server(state))
    elif args.action == 'status':
        record=live_record(state)
        emit({'running':bool(record),**({k:record[k] for k in ('pid','host','port','publicOnly','startedUtc')} if record else {}),'logFile':str(state/'logs/server.log')})
    elif args.action == 'logs':
        path=state/'logs/server.log'
        if not path.is_file(): raise RuntimeError('No server log yet; start this instance first.')
        print('Following server.log; Ctrl+C closes this viewer only.',flush=True)
        try:
            position=max(0,path.stat().st_size-16384)
            while True:
                with path.open('r',encoding='utf8',errors='replace') as stream:
                    if path.stat().st_size<position:position=0
                    stream.seek(position);print(stream.read(),end='',flush=True);position=stream.tell()
                time.sleep(0.5)
        except KeyboardInterrupt: pass
    elif args.action == 'backup': emit({'backup': str(backup(state, args.backup_dir))})
    elif args.action == 'restore':
        if not args.source: raise RuntimeError('restore requires --source backup-folder')
        previous = restore(state, args.source)
        emit({'restored': True, 'previousBackup': str(previous) if previous else None})


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, sqlite3.Error) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
