"""15-minute human-activity lobby lease, separate from match phase clocks."""
IDLE_SECONDS = 15 * 60

def ready(room, seat):
    # The native lobby owner has a Start button, never a readiness toggle.
    # Also project old saved lobbies correctly without rewriting them on GET.
    return room['phase'] == 'building' and seat['participantId'] == room['owner'] or bool(seat.get('ready'))

def in_lobby(room):
    return room['phase'] == 'building' or room['phase'] == 'result' and any(
        row.get('returned') and not row.get('teamLeft') and not row.get('left') for row in room['seats'])

def touch(rooms, room):
    if in_lobby(room) and not room.get('closed'):
        room['idleDeadlineTs'] = int(rooms.clock()) + IDLE_SECONDS

def close(rooms, room, reason):
    if room.get('closed'): return
    room['closed'] = {'reason': reason, 'atTs': int(rooms.clock())}
    room['idleDeadlineTs'] = None
    for seat in room['seats']:
        seat.update(left=True, teamLeft=True, connected=False)
    rooms._event(room, 'room-closed', {'reason': reason})

def closed_view(room, actor):
    from .engine import DomainError
    if not any(row['participantId'] == actor for row in room['seats']):
        raise DomainError('not_a_room_member')
    return {'ok': True, 'roomId': room['roomId'], 'left': True,
            'roomClosed': room['closed'], 'officialFallback': False}

def tick(rooms, room):
    if room.get('closed'): return False
    if not in_lobby(room): return False
    changed = False
    if 'idleDeadlineTs' not in room:
        # Old lobby saves have join times but no activity lease. Heartbeats
        # must never grant an old inactive room a new 15-minute lease.
        room['idleDeadlineTs'] = max((r.get('joinedTs', 0) for r in room['seats']), default=int(rooms.clock())) + IDLE_SECONDS
        changed = True
    if room.get('idleDeadlineTs') is not None and int(rooms.clock()) >= room['idleDeadlineTs']:
        close(rooms, room, 'idle-timeout'); return True
    return changed
