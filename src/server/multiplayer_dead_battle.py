"""Read-only, round-bound native combat input for continuous death observers.

Freeze once at the live battle boundary, never reconstruct a lost Unity battle
from a historical personal run. Observer preparation ACKs never block fighters.
"""
import copy


def capture(rooms, room):
    room.pop('deadBattleViews', None)
    if room['phase'] != 'battle': return
    entries = (room.get('preparationViews') or {}).get('entries', {})
    targets, actors = {}, {}
    for actor in room['seats']:
        if (not actor.get('state') or actor['state']['player']['hp'] > 0 or
                actor.get('quit') or actor.get('left') or not actor.get('connected')): continue
        entry = entries.get(str(actor['seat']), {})
        prepared = entry.get('pending') or entry.get('lastCommitted') or {}
        plan = prepared.get('plan', {})
        target = next((s for s in rooms._alive(room) if s['seat'] == plan.get('targetSeat')), None)
        if (not target or not plan.get('observerOnly') or plan.get('kind') != 'observe' or
                plan.get('phase') != 'preparation' or plan.get('round') != room['round']): continue
        key = str(target['seat'])
        if key not in targets:
            targets[key] = {'runId': target['state']['runId'], 'stageId': target['state']['stageId'],
                'revision': target['state']['revision'],
                'projection': rooms.target_projection(room, actor, target)}
        actors[str(actor['seat'])] = {'runId': actor['state']['runId'], 'targetSeat': target['seat'],
            'planHash': prepared['planHash'], 'attempt': prepared['attempt']}
    room['deadBattleViews'] = {'round': room['round'], 'matchSequence': room.get('matchSequence', 1),
        'targets': targets, 'actors': actors}


def binding(room, seat):
    frozen = room.get('deadBattleViews') or {}
    if (room['phase'] != 'battle' or frozen.get('round') != room['round'] or
            frozen.get('matchSequence') != room.get('matchSequence', 1) or
            not seat.get('state') or seat['state']['player']['hp'] != 0 or
            seat.get('quit') or seat.get('left') or seat.get('sceneLeft') or seat.get('returned') or
            seat.get('reconstruction') or seat.get('transportSuspension')): return None
    actor = frozen.get('actors', {}).get(str(seat['seat']))
    entry = (room.get('preparationViews') or {}).get('entries', {}).get(str(seat['seat']), {})
    prepared = entry.get('pending') or entry.get('lastCommitted') or {}
    if (not actor or actor['runId'] != seat['state']['runId'] or entry.get('lastFailure') or
            prepared.get('planHash') != actor['planHash'] or prepared.get('attempt') != actor['attempt']): return None
    target = frozen['targets'][str(actor['targetSeat'])]
    return actor, target, entry


def project(rooms, room, seat, view):
    found = binding(room, seat)
    if not found: return False
    actor, target, entry = found
    from .multiplayer_projection import project_team, dead_observation
    from .multiplayer_field import digest
    projection = copy.deepcopy(target['projection'])
    # A shared target snapshot can serve multiple dead recipients. Keep every
    # recipient identity and response DTO personal; only presentation is shared.
    projection.update(project_team(rooms.protocol, room, seat))
    projection['view'].update(recipientSeat=seat['seat'], writeSeat=seat['seat'])
    projection['allStateSync']['payload']['allStateSyncData']['uidIndex'] = seat['seat']
    projection['preparation'] = copy.deepcopy(view['dtoProjection']['preparation'])
    live = view['dtoProjection']['allStateSync']['payload']['allStateSyncData']['sceneDetail']['runtimePlayers']
    pending = entry.get('pending')
    status = 'preparing' if pending else 'running'
    for packet in ('allStateSync', 'sceneStatus'):
        payload = projection[packet]['payload']
        scene = payload['allStateSyncData']['sceneDetail'] if packet == 'allStateSync' else payload['sceneStatus']
        scene['runtimePlayers'] = copy.deepcopy(live)
        if pending:
            scene.update(state=3, forceEndTs=-1, selfBattleInfo=None, helpBattleInfo=None, bossBattleInfo=None)
    envelope = {'version': 1, 'roomId': room['roomId'], 'matchSequence': room.get('matchSequence', 1),
        'round': room['round'], 'seat': seat['seat'], 'runId': seat['state']['runId'],
        'targetSeat': actor['targetSeat'], 'targetRunId': target['runId'], 'stageId': target['stageId'],
        'status': status, 'readOnly': True, 'mayReportCombat': False,
        'planHash': actor['planHash'], 'attempt': actor['attempt'],
        'deadlineTs': room['deadlineTs'], 'proof': copy.deepcopy(entry.get('proof'))}
    view.update(dtoProjection=projection, nativeDeadBattleObservation=envelope,
        preparationWriteAllowed=False, observedSeat=actor['targetSeat'], preparationView=None)
    if pending:
        side = copy.deepcopy(pending)
        side.update(targetProjection=copy.deepcopy(projection), targetProjectionSha256=digest(projection),
                    targetRevision=target['revision'], requiredForBattle=False)
        view['preparationView'] = side
        view['observerRollover'] = {'version': 1, 'readOnly': True, 'mayStartSelfBattle': False,
            'currentRoomRound': room['round'], 'personalRound': seat['state']['round'],
            'presentation': 'PREPARE', 'nativeRuntimeValidated': False}
    else:
        view.pop('observerRollover', None)
    view['nativeDeadAutoObservation'] = dead_observation(rooms.protocol, room, seat, projection, allow_battle=True)
    return True
