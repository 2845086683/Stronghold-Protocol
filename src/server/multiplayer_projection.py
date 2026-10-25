"""Per-recipient CN declaration DTOs for the durable room bridge.

No official binary/socket codec is claimed. Projection is read-only and cannot
authorize a write to an observed seat. Seat ownership stays in MultiplayerRooms.
Evidence: research/recording-reviews/20261014-010340/protocol-contract.json.
"""
import copy

from .engine import DomainError, integer

_DEPLOYMENT = ('uidIndex', 'charChess', 'equipChess', 'trapChess', 'positions', 'bonds')
_PHASES = {'loading': 'LOADING', 'special': 'SP_PREPARE', 'preparation': 'PREPARE',
           'battle_countdown': 'PREPARE',
           'battle': 'BATTLE', 'help': 'HELP_BATTLE', 'boss': 'BOSS_BATTLE', 'result': 'SETTLE'}
_PENDING = ['native custom-room callbacks and persistent dispatcher require Unity acceptance',
            'official socket framing and native multiplayer object acceptance are unverified']
_TEAM_NS = 'Torappu.UI.AutoChess.Server.'
_TEAM_PHASES = {'building': 'TEAM_BUILDING', 'confirm': 'INFO_SHOW',
                'strategy': 'STRATEGY_CHOOSE', 'protocol_start': 'ENTER_BATTLE_COUNT_DOWN',
                'result': 'END'}


def project_team(protocol, room, recipient):
    """The exact CN TeamStatus value consumed by ServiceTeamInfo.Fill.

    Declarations: AutoChessTeamStatus, MsgAutoChessPlayerStatus,
    StrategyDecisionBrief and STSceneInfo in multiplayer-entry-0157. The
    private address/token identify the in-process adapter, never a socket.
    Cosmetic defaults follow Protocol.project's private card policy.
    """
    members = [row for row in room['seats'] if not row.get('left') and not row.get('teamLeft')]
    selected = next((row['state'] for row in members if row.get('state')), None)
    phase = room['phase']; players = []
    queue = room.get('selectionQueue', []); queue_index = room.get('queueIndex', 0)
    current = queue[queue_index] if phase == 'strategy' and queue_index < len(queue) else None
    for row in members:
        state = row.get('state'); band = state.get('bandId') if state else None
        if row.get('returned'):
            player_state = 'TEAM_BUILDING_NOT_READY'
        elif phase == 'building':
            from .multiplayer_lobby import ready
            player_state = 'TEAM_BUILDING_READY' if ready(room, row) else 'TEAM_BUILDING_NOT_READY'
        elif phase == 'confirm': player_state = 'INFO_CONFIRM' if row.get('confirmed') else 'INFO_NOT_CONFIRM'
        elif phase == 'strategy': player_state = 'STRATEGY_CHOSEN' if band else 'STRATEGY_NOT_CHOOSE'
        elif phase == 'result': player_state = 'TEAM_BUILDING_NOT_READY' if row.get('returned') else 'IN_SETTLE_RESULT'
        else: player_state = 'IN_BATTLE'
        connection = 'OFFLINE' if row.get('quit') and not row.get('returned') else 'ONLINE' if row.get('connected', True) else 'MISS'
        from .multiplayer_identity import display
        identity = display(row)
        players.append({'uid': 'private-' + row['participantId'], 'nickName': identity['nickName'],
            # CN PlayerAvatarType.DEFAULT=3, verified field defaults in
            # multiplayer-entry-0157/avatar-enum.json. Managed enum projection
            # needs the integer, unlike a friendly JSON serializer enum name.
            **{key: identity[key] for key in ('nickNumber','avatarType','avatarId','secretary',
                'secretarySkinId','secretarySkinSp','nameCardSkinId','nameCardSkinTmpl','level')},
            'title': [], 'strategy': band,
            'state': protocol.c.enum(_TEAM_NS + 'AutoChessPlayerState', player_state),
            'connState': protocol.c.enum(_TEAM_NS + 'AutoChessPlayerConnectState', connection),
            'jointTs': min(2147483647, row.get('joinedTs', row.get('lastSeenTs', 0))), 'medalCount': row.get('trophyNum', 0), 'gsChannel': 0})
    owner = room.get('owner') or (members[0]['participantId'] if members else recipient['participantId'])
    scene = {'sceneID': room.get('roomId') if not recipient.get('returned') and phase not in ('building', 'confirm', 'strategy', 'protocol_start') else None,
             'address': 'private-http-room', 'token': 'private-in-process-adapter'}
    banned = (sorted(set(selected.get('bannedBondIds', [])) | set(selected.get('inactiveBondIds', [])))
              if selected else [])
    returned = bool(recipient.get('returned'))
    end = room.get('idleDeadlineTs') if phase == 'building' or returned else room.get('deadlineTs')
    status = {'teamStateEndTs': end if end is not None else -1,
        'state': protocol.c.enum(_TEAM_NS + 'AutoChessTeamState', 'TEAM_BUILDING' if returned else _TEAM_PHASES.get(phase, 'IN_BATTLE')),
        'modeRange': protocol.c.enum(_TEAM_NS + 'AutoChessMatchModeRange', 'PRECISE'),
        'modeId': room.get('modeId', selected['modeId'] if selected else 'mode_multi_funny'),
        'ownerUid': 'private-' + owner, 'bannedBond': banned,
        'monsterKeys': copy.deepcopy(selected.get('enemyTypes', [])) if selected else [],
        'bossId': selected.get('bossId') if selected else None, 'players': players, 'scene': scene,
        'strategyDecisionBrief': {'roundFinTs': room.get('deadlineTs') or -1,
            'canSkip': False, 'currUID': next(('private-' + row['participantId'] for row in members if row['seat'] == current), None),
            'choices': [{'uid': player['uid'], 'strategy': player['strategy'], 'passCnt': 0} for player in players]},
        'matchFlag': False}
    return {'teamStatus': status, 'privateSelfUid': 'private-' + recipient['participantId'],
            'privateTeamId': room.get('code', room.get('roomId', '')), 'officialSocketAllowed': False}


def _scene(projected):
    return projected['allStateSync']['payload']['allStateSyncData']['sceneDetail']


def _remap_effects(protocol, rows, seat):
    result = protocol.special_effects(rows)
    for row in result:
        row['affectedUidIndex'] = [seat if value == 0 else value for value in row['affectedUidIndex']]
    return result


def _all_effects(protocol, seats):
    """The native data center owns one room collection, filtered per target.

    Effect instances already share the same per-seat namespace as chess. Keep
    those identities and explicit affected-seat lists; never broaden a buff
    merely because a failed field observes the defending players.
    """
    merged = {}
    for row in seats:
        for effect in _remap_effects(protocol, row['state']['specialEffects'], row['seat']):
            ident = effect['effectInstId']
            if ident in merged and merged[ident] != effect:
                raise DomainError('conflicting_multiplayer_effect_identity')
            merged[ident] = effect
    return [merged[key] for key in sorted(merged)]


def _runtime(protocol, room, row, projected):
    value = copy.deepcopy(_scene(projected)['runtimePlayers'][0]); seat = row['seat']
    phase = room['phase']
    reports = room.get('helpReports', {}) if phase == 'help' else room.get('reports', {})
    completed = str(seat) in reports
    if phase == 'loading': state = 'LOADING_READY' if row.get('loaded') else 'LOADING_NOT_READY'
    elif phase in ('special', 'preparation', 'battle_countdown'): state = 'PREPARATION_READY' if row.get('ready') else 'PREPARATION_NOT_READY'
    else: state = 'BATTLE_COMPLETE' if completed or phase == 'result' else 'BATTLE_NOT_COMPLETE'
    connection = 'QUIT' if row.get('quit') or row.get('left') else 'ONLINE' if row.get('connected', True) else 'MISS_CONN'
    value.update(uidIndex=seat, state=protocol.c.enum('Torappu.AutoChessPlayerGameStateType', state),
        connectState=protocol.c.enum('Torappu.AutoChessPlayerConnectStateType', connection),
        gameFinish=phase == 'result' or row['state']['player']['hp'] <= 0 or row.get('quit', False))
    return value


def dead_observation(protocol, room, recipient, projection, *, allow_battle=False):
    """Bound private envelope around the declared native DeadAutoObDn DTO."""
    if ((room['phase'] not in ('preparation', 'special') and not (allow_battle and room['phase'] == 'battle')) or not recipient.get('state') or
            recipient['state']['player']['hp'] > 0 or recipient.get('quit') or
            recipient.get('left') or recipient.get('sceneLeft') or recipient.get('returned')):
        return None
    index = projection['view']['observedSeat']
    target = next((row for row in room['seats'] if row['seat'] == index and row.get('state') and
                   row['state']['player']['hp'] > 0 and not row.get('quit') and not row.get('left')), None)
    if target is None: return None
    scene = projection['allStateSync']['payload']['allStateSyncData']['sceneDetail']
    prep = scene['preparationStatus']
    if not isinstance(prep, dict) or prep.get('uidIndex') != index: return None
    return {'version': 1, 'roomId': room['roomId'], 'matchSequence': room.get('matchSequence', 1),
        'round': room['round'], 'seat': recipient['seat'], 'runId': recipient['state']['runId'],
        'targetSeat': index, 'targetRunId': target['state']['runId'],
        'protocol': protocol.message('AutoChessBattleSceneDeadAutoObDn', {
            'obIndex': index, 'state': scene['state'], 'preparation': copy.deepcopy(prep)})}


def project_room(protocol, room, recipient_seat):
    integer(recipient_seat, 'recipientSeat', 0, 3)
    members = [row for row in room['seats'] if not row.get('left')]
    recipient = next((row for row in members if row['seat'] == recipient_seat), None)
    if recipient is None:
        raise DomainError('not_a_room_member')
    pending = _PENDING[:]
    result = {'nativeReady': False, 'nativeAccepted': False, 'officialWireCompatible': False,
              'pending': pending, 'view': {'recipientSeat': recipient_seat,
                  'observedSeat': recipient.get('watchSeat', recipient_seat),
                  'writeSeat': recipient_seat, 'observationGrantsWriteAccess': False},
              'allStateSync': None, 'sceneStatus': None, 'preparation': None}
    result.update(project_team(protocol, room, recipient))
    if recipient.get('returned'):
        return result  # This actor left the battle scene and is in its party UI.
    if recipient.get('state') is None and recipient.get('quit'):
        pending.append('recipient quit before an individual battle state was established')
        return result
    # A member can quit during confirmation, before a run exists. Its empty
    # historical roster entry must not suppress every surviving player's DTO.
    seats = [row for row in members if row.get('state') is not None or not row.get('quit')]
    if room['phase'] not in _PHASES or any(row.get('state') is None for row in seats):
        pending.append('native team building/info/strategy object acceptance remains unverified')
        return result
    # Protocol.project makes its own inventory/hand reconciliation copy. Keep
    # every original run, RNG, carry value and instance number untouched.
    snapshots = {row['seat']: protocol.project(row['state']) for row in seats}
    by_seat = {row['seat']: row for row in seats}
    # A preparation observation must never substitute another seat's enemy
    # ledger when the original client begins the recipient's own battle.
    watched = (recipient.get('watchSeat', recipient_seat)
               if room['phase'] in ('preparation', 'special') else recipient_seat)
    if watched not in by_seat and any(row['seat'] == watched and row.get('quit') and row.get('state') is None for row in members):
        watched = recipient_seat
        result['view']['observedSeat'] = watched
    if watched not in by_seat:
        raise DomainError('unknown_observation_target')
    if by_seat[watched]['state']['stageId'] != recipient['state']['stageId']:
        watched = recipient_seat
        pending.append('cross-base-stage observation is blocked until native return-to-self loading is coordinated')
    result['view']['observedSeat'] = watched
    target = by_seat[watched]
    if room['phase'] == 'help' and room.get('help'):
        # All seats watch the same help battlefield. The right field supplies
        # its origin; original segment maps remain in the private plan until
        # the native composition code has been bound and verified.
        watched = room['help']['rightSeat']; target = by_seat[watched]
        result['view']['observedSeat'] = watched
    elif room['phase'] == 'boss' and recipient['state']['player']['hp'] <= 0:
        # A dead player observes an existing live pair and receives that pair's
        # original native Step stream. Its historical own run is never used
        # as a boss battlefield and grants no ActionUp reporting authority.
        groups = (room.get('nativeBoss') or {}).get('groups', [])
        active = [index for row in groups for index in row['players']]
        if active:
            watched = recipient.get('watchSeat')
            if watched not in active: watched = active[0]
            target = by_seat[watched]
            result['view']['observedSeat'] = watched
    projected = snapshots[watched]
    sync = copy.deepcopy(projected['allStateSync']['payload']['allStateSyncData'])
    scene = sync['sceneDetail']
    sync['uidIndex'] = recipient_seat
    sync['staticPlayers'] = []
    for row in seats:
        static = copy.deepcopy(snapshots[row['seat']]['allStateSync']['payload']['allStateSyncData']['staticPlayers'][0])
        static.update(uid='private-' + row['participantId'], index=row['seat'])
        from .multiplayer_identity import display
        identity = display(row)
        for key in ('nickName','nickNumber','avatarId','secretary','secretarySkinId',
                    'secretarySkinSp','nameCardSkinTmpl','level'):
            static['cardInfo'][key] = copy.deepcopy(identity[key])
        # Team status uses PlayerAvatarType, but the original battle card
        # declares a String. Do not copy the lobby enum into this DTO graph.
        static['cardInfo']['avatarType'] = ('NONE','ASSISTANT','ICON','DEFAULT')[identity['avatarType']]
        static['cardInfo']['nameCardSkin'] = identity['nameCardSkinId']
        sync['staticPlayers'].append(static)
    scene.update(obUidIndex=watched, round=room['round'],
        forceEndTs=room.get('deadlineTs') if room.get('deadlineTs') is not None else -1,
        state=protocol.c.enum('Torappu.AutoChessGameStateType', _PHASES[room['phase']]),
        runtimePlayers=[_runtime(protocol, room, row, snapshots[row['seat']]) for row in seats],
        specialEffects=_all_effects(protocol, seats))
    if room['phase'] == 'boss' and scene['forceEndTs'] >= 0:
        # Native OnBattleStart subtracts this offset for the visible countdown
        # in both LOCAL and MULTI. The room deadline itself remains the real
        # overtime boundary; only the native force-end DTO carries the offset.
        scene['forceEndTs'] += protocol.c.turns(room['modeId'])[room['round']]['BossTurnHpReduceTime']
    scene['preparationStatus']['uidIndex'] = watched
    # The official SelfBattleFinish contract only changes that player's runtime
    # state to BATTLE_COMPLETE. The scene remains BATTLE until every field has
    # finished; a synthetic BATTLE_WAITING transition reloads/clears the world.
    # TC 2026-10-20 events-001:258/260/267/274 -> :281 (next PREPARE).
    scene['lastBattleResult'] = []
    if room['phase'] not in ('battle', 'help', 'boss'):
        for row in seats:
            old = _scene(snapshots[row['seat']]).get('lastBattleResult') or []
            scene['lastBattleResult'].extend({**copy.deepcopy(value), 'uidIndex': row['seat']} for value in old)
    if scene.get('bossRoundDetail'):
        scene['bossRoundDetail']['hp'] = sum(row['state']['player']['hp'] for row in seats)
        from .multiplayer_native_boss import groups
        scene['bossRoundDetail']['groupInfos'] = copy.deepcopy(room.get('nativeBoss', {}).get('groups') if room.get('nativeBoss') else groups(seats))
    retained_special = (room['phase'] in ('preparation', 'battle_countdown') and
        room.get('specialDraft') and room.get('specialRound', room['specialDraft'].get('round')) == room['round'])
    if room['phase'] == 'special' or retained_special:
        queue = room.get('selectionQueue', [])
        draft = room.get('specialDraft')
        fallback = next((row['state'].get('specialDraft') for row in seats if row['state'].get('specialDraft')), None)
        if draft is None:
            draft = fallback
            pending.append('shared room special draft has not been persisted')
        event = room.get('specialEventId')
        if event is None:
            event = next((row['state']['player']['selfChoice']['id'] for row in seats
                          if row['state']['player'].get('selfChoice')), None)
        if draft is None or event is None:
            pending.append('shared special event/options are unavailable')
            scene['specialPreparation'] = None
        else:
            choices = room.get('specialChoices', {})
            scene['specialPreparation'] = {'options': copy.deepcopy(draft['slots']),
                'playersQueue': [{'uidIndex': index, 'choice': choices.get(str(index), -1)} for index in queue],
                'spId': event, 'finTs': room.get('specialFinTs', scene['forceEndTs']), 'currQueueIndex': room.get('queueIndex', 0)}
    else:
        scene['specialPreparation'] = None
    if room['phase'] == 'help':
        plan = room.get('help')
        if not plan:
            pending.append('joint-defense plan is unavailable')
        else:
            players = []
            # CN LoadHelpBattlePlayer (0x601348a, b10993) assigns players[1]
            # to rightMapPlayerIndex. The domain encounter plan is right-first;
            # native paired deployment is left-first and must remain distinct.
            native_segments = list(reversed(plan['segments'])) if len(plan['segments']) == 2 else plan['segments']
            for segment in native_segments:
                index = segment['seat']; prep = copy.deepcopy(_scene(snapshots[index])['preparationStatus'])
                prep['uidIndex'] = index
                status = []
                for unit in segment['deployments']:
                    carry = unit.get('carry')
                    # Native _ApplyChessStatus uses an absent status to call
                    # FinishWithNoReason. Sending hp=0 instead takes its normal
                    # HP modifier path and does not prove forced withdrawal.
                    if unit['chessType'] == 0 and carry is not None and not carry['forceRetreat']:
                        status.append({'instId': unit['instId'], 'hp': carry['hp'], 'tech': carry['tech']})
                    elif unit['chessType'] == 3 and carry is None:
                        pending.append('joint-defense TOKEN terminal SP is unresolved')
                players.append({'deployment': {key: prep[key] for key in _DEPLOYMENT},
                                'charBattleStatusList': status})
                if segment.get('unresolvedCarry'):
                    pending.append('joint-defense character terminal carry is incomplete')
            scene['selfBattleInfo'] = None; scene['bossBattleInfo'] = None
            scene['helpBattleInfo'] = {'escapedEnemies': copy.deepcopy(plan['enemies']), 'players': players}
            pending.append('native escaped-field composition and per-field terrain/effect application await Unity validation')
            if any(unit['chessType'] == 3 for segment in plan['segments'] for unit in segment['deployments']):
                pending.append('private TOKEN origin/card SP adapter is implemented; Unity execution remains unverified')
    elif room['phase'] == 'boss':
        native = room.get('nativeBoss')
        if not native:
            scene['bossBattleInfo'] = None
            pending.append('room predates the private native boss clock and requires migration')
        else:
            paired = next((row for row in native['groups'] if watched in row['players']), native['groups'][0])
            scene['bossRoundDetail']['hp'] = native['initialTeamHp']
            players = []
            for index in paired['players']:
                prep = copy.deepcopy(_scene(snapshots[index])['preparationStatus']); prep['uidIndex'] = index
                players.append({key: prep[key] for key in _DEPLOYMENT})
            scene['selfBattleInfo'] = None; scene['helpBattleInfo'] = None
            scene['bossBattleInfo'] = {'group': paired['group'],
                # Shared HP changes travel through operation 8; mutating this
                # seed each poll would make FillBossBattleInfo reset native data.
                'bossHp': native['initialBossHp'],
                'enemyCount': native['initialEnemyCount'],
                'enemyGroup': copy.deepcopy(native['initialEnemyGroup']), 'players': players}
            pending.append('private final-assault clock/pairing adapter is implemented; Unity acceptance and per-field terrain remain unverified')
    # HELP carries each defender's own deployment inside helpBattleInfo. A
    # stale standalone preparation board must not overwrite that paired field.
    # SETTLE likewise has no preparation board (nine official HELP samples and
    # the final SETTLE, TC 2026-10-20 events-001:3282 / events-003:2733).
    if room['phase'] in ('help', 'result'):
        scene['preparationStatus'] = None
    own_prep = copy.deepcopy(_scene(snapshots[recipient_seat])['preparationStatus'])
    own_prep['uidIndex'] = recipient_seat
    # Completion responses always belong to the authenticated actor. A watched
    # board lives only in sceneDetail and is read-only through obUidIndex.
    result['preparation'] = protocol.message('AutoChessScenePreparationStatusResp', {'code': 0, 'preparationStatus': own_prep})
    result['allStateSync'] = protocol.message('AutoChessBattleSceneAllStateSyncDn', {'allStateSyncData': sync})
    result['sceneStatus'] = protocol.message('AutoChessBattleSceneChangeStatusDn', {'sceneStatus': copy.deepcopy(scene)})
    from .multiplayer_reconstruction import projection as reconstruction_projection
    reconstruction_projection(protocol, room, recipient, result)
    result['pending'] = list(dict.fromkeys(pending))
    return result
