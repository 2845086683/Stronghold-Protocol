"""Derived, transactional clock index: idle history never needs a JSON poll.

Room JSON remains authoritative. Triggers invalidate externally changed rows;
the normal save writes its exact next deadlines in that same transaction.
Restart rebuilds this small index one room at a time, never loading receipts.
"""
import json

VIEW_PHASES = frozenset(('preparation', 'special', 'battle_countdown', 'battle', 'boss'))


def initialize(rooms):
    db = rooms.store.db
    db.executescript('''
        CREATE TABLE IF NOT EXISTS multiplayer_room_clocks(
            room TEXT PRIMARY KEY, tick_at REAL, view_at REAL);
        CREATE INDEX IF NOT EXISTS multiplayer_clock_tick ON multiplayer_room_clocks(tick_at);
        CREATE INDEX IF NOT EXISTS multiplayer_clock_view ON multiplayer_room_clocks(view_at);
        CREATE TRIGGER IF NOT EXISTS multiplayer_clock_insert AFTER INSERT ON multiplayer_rooms
            BEGIN INSERT OR REPLACE INTO multiplayer_room_clocks VALUES(NEW.id,0,0); END;
        CREATE TRIGGER IF NOT EXISTS multiplayer_clock_update AFTER UPDATE OF state ON multiplayer_rooms
            BEGIN INSERT OR REPLACE INTO multiplayer_room_clocks VALUES(NEW.id,0,0); END;
        CREATE TRIGGER IF NOT EXISTS multiplayer_clock_delete AFTER DELETE ON multiplayer_rooms
            BEGIN DELETE FROM multiplayer_room_clocks WHERE room=OLD.id; END;
    ''')
    db.execute('DELETE FROM multiplayer_room_clocks')
    for ident, raw in db.execute('SELECT id,state FROM multiplayer_rooms'):
        try:
            room = json.loads(raw)
            if room.get('catalogHash') != rooms.c.hash or room.get('version') != 1:
                raise ValueError('old room contract')
            schedule(rooms, room)
        except (ValueError, KeyError, TypeError):
            # The original room clock isolates incompatible saves as faults.
            # Let it inspect the exact original row on its next invocation.
            db.execute('INSERT OR REPLACE INTO multiplayer_room_clocks VALUES(?,0,0)', (ident,))


def deadlines(rooms, room):
    if room.get('closed'):
        return None, None
    from .multiplayer_reconstruction import PRESENCE_TIMEOUT_SECONDS
    candidates = [seat['lastSeenTs'] + PRESENCE_TIMEOUT_SECONDS + 1 for seat in room['seats'] if seat.get('connected')]
    from .multiplayer_lobby import in_lobby
    if in_lobby(room):
        idle = room.get('idleDeadlineTs')
        candidates.append(0 if 'idleDeadlineTs' not in room else idle)
    expiry = room.get('settleAfterTs') or room.get('deadlineTs')
    if expiry is not None:
        candidates.append(expiry)
    if room['phase'] == 'boss':
        native = room.get('nativeBoss') or {}
        # Original HTTP service_actions runs at most every 0.5 seconds. Keep
        # the original 100ms RevStep catch-up; do not add a busy timer thread.
        candidates.append((native.get('startedAtMs', 0) +
            (native.get('lastSeq', -1) + 1) * native.get('durationMs', 100)) / 1000)
        if native.get('settleAtMs') is not None:
            candidates.append(native['settleAtMs'] / 1000)
        # Overtime applies at integer seconds, while the expired fight clock
        # must not make an otherwise unchanged room permanently due.
        if expiry is not None and expiry <= rooms.clock():
            candidates.remove(expiry)
            candidates.append(int(rooms.clock()) + 1)
    candidates = [value for value in candidates if value is not None]
    view_times = []
    if room['phase'] in VIEW_PHASES:
        views = room.get('preparationViews') or {}
        barrier = views.get('returnBarrier')
        if barrier and barrier.get('status') == 'loading':
            view_times.append(barrier['deadlineTs'])
        for seat in room['seats']:
            if not seat.get('state') or seat.get('quit') or seat.get('left'):
                continue
            entry = views.get('entries', {}).get(str(seat['seat']), {})
            pending = entry.get('pending')
            if not pending:
                continue
            observer = pending['plan']['observerOnly'] and (pending['plan']['kind'] == 'return-own' or
                entry.get('needsOwnRecovery') or room['phase'] in ('battle_countdown', 'battle', 'boss'))
            if observer or room['phase'] in ('preparation', 'special'):
                view_times.append(pending['deadlineTs'])
    return min(candidates, default=None), min(view_times, default=None)


def schedule(rooms, room):
    tick, view = deadlines(rooms, room)
    rooms.store.db.execute('INSERT OR REPLACE INTO multiplayer_room_clocks VALUES(?,?,?)',
        (room['roomId'], tick, view))


def due(rooms, *, views=False):
    column = 'view_at' if views else 'tick_at'
    # Only tiny IDs are materialized. Room state is fetched one at a time in
    # the inherited per-room transaction and retains its fault isolation.
    return rooms.store.db.execute('SELECT room FROM multiplayer_room_clocks WHERE ' + column +
        '<=? AND room NOT IN (SELECT room FROM multiplayer_room_faults) ORDER BY ' + column,
        (rooms.clock(),)).fetchall()
