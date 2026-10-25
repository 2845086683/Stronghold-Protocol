"""Durable non-voluntary absence; Unity snapshots are never fabricated.

An interrupted self field forfeits this round's combat gains and all issued
enemies. Preparation remains durable. Three missed round boundaries withdraw
the member; HTTP retries shorter than the existing presence lease do not.
"""
import copy

POLICY = 'private-two-round-reconstruction-v1'
PRESENCE_TIMEOUT_SECONDS = 12  # Two missed 5-second heartbeats plus transport jitter.
_KEEP = frozenset(('battle', 'history', 'seq', 'revision', 'completedReports'))


def capture_round(seat):
    # Combat has no shop writes. A single checkpoint before Unity starts lets
    # us roll back layers AND their dependent milestone grants atomically,
    # without erasing preparation purchases or previous rounds' gains.
    state = seat['state']
    if state['phase'] != 'battle':
        # Shared Boss combat forbids layer gains. Do not carry a ~50KiB self
        # field checkpoint through every high-frequency shared step write.
        seat.pop('reconstructionBaseline', None)
        return
    seat['reconstructionBaseline'] = {
        key: copy.deepcopy(value) for key, value in state.items() if key not in _KEEP}


def waiting(room, seat):
    value = seat.get('reconstruction') or {}
    return (value.get('round') == room['round'] and
            room['phase'] in ('battle_countdown', 'battle', 'help', 'boss') and
            not seat.get('quit'))


def lose_self(rooms, room, seat):
    if str(seat['seat']) in room['reports']:
        return  # An already committed completed field is not lost retroactively.
    state = seat['state']; battle = state['battle']
    baseline = seat.get('reconstructionBaseline')
    if baseline:
        preserved = {key: value for key, value in state.items() if key in _KEEP}
        state.clear(); state.update(copy.deepcopy(baseline)); state.update(preserved)
    seat.setdefault('interruptedEvidence', {})[str(room['round'])] = {
        'killed': copy.deepcopy(battle['killed']), 'escaped': copy.deepcopy(battle['escaped']),
        'reports': copy.deepcopy(battle.get('reports', {}))}
    battle['killed'] = []; battle['escaped'] = []
    report = {'round': room['round'], 'fromRejoin': True,
        'escapedInstIds': [row['instId'] for row in battle['units']],
        'escapedTokenInstIds': copy.deepcopy(battle.get('nativeEscapedTokenIds', [])),
        'killedInsts': [], 'charBattleStatusList': [], 'tokenBattleStatusList': []}
    rooms._self_finish(room, seat, report, native_terminal=False)


def suspend(rooms, room, seat, reason, *, close_field=True):
    if seat.get('quit') or seat.get('left') or not seat.get('state'):
        seat['connected'] = False
        return
    seat['connected'] = False; seat['ready'] = False
    seat.setdefault('transportSuspension', {'round': room['round'],
        'atTs': int(rooms.clock()), 'reason': reason, 'policy': POLICY})
    if seat['state']['player']['hp'] > 0 and room['phase'] in ('battle_countdown', 'battle', 'help', 'boss'):
        seat['reconstruction'] = {'round': room['round'], 'phase': room['phase'], 'policy': POLICY,
            'presentation': 'BATTLE_WAITING', 'reason': reason}
        if close_field and room['phase'] == 'battle': lose_self(rooms, room, seat)
    if room['phase'] == 'help':
        from .multiplayer_field import exclude_reconstruction
        exclude_reconstruction(rooms, room)
    rooms._event(room, 'connection', {'seat': seat['seat'], 'connected': False,
                                    'reason': reason, 'policy': POLICY})


def reconnect(rooms, room, seat):
    seat.pop('transportSuspension', None)
    seat.update(connected=True, lastSeenTs=int(rooms.clock()))
    if room['phase'] in ('preparation', 'special'):
        seat.pop('reconstruction', None)


def round_boundary(rooms, room, next_round):
    for seat in room['seats']:
        if seat.get('quit') or seat.get('left'): continue
        absence = seat.get('transportSuspension')
        if absence and next_round - absence['round'] > 2:
            # At a team settlement boundary no unresolved private field remains.
            # Withdraw from combat, retaining the personal result until this
            # offline player can open and acknowledge it after restarting.
            seat.update(quit=True, connected=False, ready=False, sceneLeft=True,
                        sceneLeftAtTs=int(rooms.clock()), reconnectSettlementPending=True)
            rooms._event(room, 'reconnect-grace-expired', {'seat': seat['seat'],
                'sinceRound': absence['round'], 'round': next_round, 'policy': POLICY})
        elif not absence:
            seat.pop('reconstruction', None)


def projection(protocol, room, seat, projected):
    if not waiting(room, seat): return
    for packet in ('allStateSync', 'sceneStatus'):
        if not projected.get(packet): continue
        payload = projected[packet]['payload']
        scene = payload['allStateSyncData']['sceneDetail'] if packet == 'allStateSync' else payload['sceneStatus']
        scene.update(state=protocol.c.enum('Torappu.AutoChessGameStateType', 'BATTLE_WAITING'),
            forceEndTs=-1, selfBattleInfo=None, helpBattleInfo=None, bossBattleInfo=None)
