"""Native room rejoin with durable results and an explicit lost-Unity policy.

CN ReqLoadReady (0x601361d) sends fromRejoin=True finish in BATTLE/HELP,
not an enemy-HP/timeline replay. SelfBattleInfoUp contains only character
status/check data. Never treat an ordinary transport reconnect as Unity loss.
"""
import copy

from .engine import DomainError

POLICY = 'private-two-round-reconstruction-v1'


def unresolved_battle_report(rooms, room, seat, *, from_rejoin):
    """Close one lost/withdrawn field without inventing an unobserved kill.

    Departure and lost-Unity recovery use the same committed enemy/carry
    evidence. This is a deterministic private policy, not an assertion about
    the official server's internal accounting for a departed battlefield.
    """
    battle = seat['state']['battle']; killed = set(battle['killed'])
    last = battle.get('reports') or {}
    chars = copy.deepcopy(last.get('charBattleStatusList', []))
    try:
        chars = rooms.combat._characters(chars, {row['instId'] for row in seat['state']['player']['charChess']})
    except DomainError:
        chars = []
    token_statuses = copy.deepcopy(last.get('tokenBattleStatusList', []))
    try:
        from .joint_defense import _statuses
        from .native_tokens import current_tokens
        from .protocol import decode_position
        valid = {(row['instId'], position['x'], position['y'])
                 for row in current_tokens(rooms.c, seat['state']) if row['position'] > 0
                 for position in [decode_position(row['position'])]}
        decoded = _statuses({'tokenBattleStatusList': token_statuses}, 'tokenBattleStatusList', token=True)
        if any(len(key) != 3 or key not in valid for key in decoded):
            raise DomainError('unknown_native_token_carry_position')
    except DomainError:
        token_statuses = []
    return {'round': room['round'], 'fromRejoin': from_rejoin,
        'escapedInstIds': [row['instId'] for row in battle['units'] if row['instId'] not in killed],
        'escapedTokenInstIds': copy.deepcopy(battle.get('nativeEscapedTokenIds', [])),
        'killedInsts': [{'enemyInstId': ident, 'attackerInstId': -1, 'damageSrc': 0} for ident in sorted(killed)],
        'charBattleStatusList': chars, 'tokenBattleStatusList': token_statuses}


def recover_unity(rooms, room, seat):
    """Resume durable preparation, or wait for the next round after Unity loss."""
    if seat.get('quit'):
        return  # A withdrawn field is already accounted for by the team barrier.
    current = {'round': room['round'], 'phase': room['phase'], 'policy': POLICY}
    previous = seat.get('unityRecovery')
    if previous and previous.get('round') == current['round'] and previous.get('phase') == current['phase']:
        return
    seat['unityRecovery'] = current
    if room['phase'] in ('battle_countdown', 'battle', 'help', 'boss'):
        seat['reconstruction'] = {**current, 'presentation': 'BATTLE_WAITING'}
    rooms._event(room, 'unity-state-lost', {'seat': seat['seat'], **current})
    if room['phase'] == 'battle' and str(seat['seat']) not in room['reports']:
        from .multiplayer_reconstruction import lose_self
        lose_self(rooms, room, seat)
    elif room['phase'] == 'help' and seat['seat'] in room['help']['defenderSeats']:
        defenders = [row for row in room['seats'] if row['seat'] in room['help']['defenderSeats']]
        if all(row.get('unityRecovery', {}).get('round') == room['round'] and row['unityRecovery'].get('phase') == 'help' for row in defenders):
            from .joint_defense import finish
            finish(room['help'], {'round': room['round'], 'escapedEnemies': [], 'killInfos': []},
                   reporter_seat=room['help']['rightSeat'], expired=True)
            rooms._settle(room)


def apply_native_rejoin(rooms, room, seat, name, data):
    """The native fresh-scene ReqLoadReady finish is an adapter handshake."""
    if name not in ('AutoChessBattleSceneSelfBattleFinishUp', 'AutoChessBattleSceneHelpBattleFinishUp'):
        return False
    if data.get('fromRejoin') is not True:
        return False
    if data.get('round') != room['round']:
        # A prior reconnect transaction may already have advanced the room.
        # Its accepted terminal report is proof; do not replay effects again.
        if str(data.get('round')) not in seat['state'].get('completedReports', {}):
            raise DomainError('stale_round')
        return True
    if room['phase'] in ('battle', 'help'):
        recover_unity(rooms, room, seat)
    return True


def presence(rooms, actor):
    """Separate retained party membership from resumable battle presence.

    QuitUp -> PlayerStatus(gameFinish) -> Kick(7) is a personal scene exit,
    even while the other fields continue. Retaining the party must not keep
    advertising that scene through the native Continue/reconnect entry.
    """
    for row in rooms.store.db.execute('SELECT id FROM multiplayer_rooms'):
        try: room = rooms._room(row[0])
        except DomainError: continue
        if not rooms._seat_present(room, actor):
            continue
        seat = rooms._seat(room, actor)
        resumable = (room['phase'] in ('loading', 'special', 'preparation',
            'battle_countdown', 'battle', 'help', 'boss', 'result') and
            (seat.get('reconnectSettlementPending') or not seat.get('quit') and not seat.get('sceneLeft')) and
            not seat.get('returned') and not seat.get('teamLeft'))
        info = {'sceneId': room['roomId'], 'address': 'private-http-room',
                'token': 'private-in-process-adapter', 'modeId': room['modeId'],
                'endTime': room.get('deadlineTs') or int(rooms.clock()) + 3600,
                'curRound': room['round']} if resumable else None
        return info, {'roomId': room['roomId'], 'roomCode': room['code'],
            'generation': room['generation'], 'phase': room['phase'],
            'snapshot': rooms._view(room, actor), 'lostUnityPolicy': POLICY,
            'midBattleUnitySnapshotAvailable': False}
    return None, None
