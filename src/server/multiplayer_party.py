"""Private custom-party return barrier, distinct from leaving a battle scene.

Pinned CN BackToRoom sends BattleSceneQuitUp before TeamProtocol.BackTeamUp.
All remaining party members must return or explicitly leave before another
match starts. Offline members are retained and may reconnect; no bot is added.
"""
import copy

from .engine import DomainError


def archive_result(room):
    sequence = room.get('matchSequence', 1)
    previous = next((row for row in room.get('matchHistory', []) if row['matchSequence'] == sequence), None)
    if previous is None:
        previous = {'matchSequence': sequence, 'modeId': room['modeId'],
            'result': copy.deepcopy(room['result']),
            'nativeStartedAtTs': room.get('nativeStartedAtTs', 0),
            'nativeEndedAtTs': room.get('nativeEndedAtTs', 0),
            'seats': copy.deepcopy(room['seats']), 'roundHistory': copy.deepcopy(room['roundHistory'])}
        room.setdefault('matchHistory', []).append(previous)
    migration = room.get('multiplayerInstanceMigration')
    if migration is not None:
        # Historical aliases document this exact completed match. They must
        # never authorize a different run created in the retained party.
        if migration.get('roomId') != room['roomId'] or migration.get('matchSequence') != sequence:
            raise DomainError('invalid_multiplayer_instance_namespace', 'party archive alias owner')
        if previous.get('multiplayerInstanceMigration') not in (None, migration):
            raise DomainError('invalid_multiplayer_instance_namespace', 'party archive alias changed')
        previous['multiplayerInstanceMigration'] = copy.deepcopy(migration)


def members(room):
    return [row for row in room['seats'] if not row.get('left') and not row.get('teamLeft')]


def finish_return(rooms, room):
    party = members(room)
    if room['phase'] != 'result' or not party or any(not row.get('returned') for row in party):
        return
    archive_result(room)
    old = room.get('matchSequence', 1)
    room['matchSequence'] = old + 1
    room['seats'] = party
    for row in party:
        profile = rooms._profile(row['participantId'])
        row['displayProfile'] = copy.deepcopy(profile.get('displayProfile', {}))
        row['nickname'] = profile['nickname']
        row['trophyNum'] = profile.get('trophyNum', 0)
        row.pop('trophyAward', None)
        row.update(state=None, connected=row.get('connected', True), loaded=False, ready=row['participantId'] == room['owner'],
                   confirmed=False, watchSeat=row['seat'], returned=False, sceneLeft=False)
        for key in ('quit', 'unityRecovery', 'sceneLeftAtTs', 'transportSuspension', 'reconstruction', 'reconstructionBaseline'):
            row.pop(key, None)
    # Every old run and result remains in matchHistory. A new run is created
    # only by the next explicit owner start and retains the same private party.
    for key in ('nativeStartedAtTs', 'nativeEndedAtTs', 'selectionQueue', 'queueIndex',
                'specialDraft', 'specialEventId', 'specialChoices', 'specialRound', 'specialFinTs',
                'specialPickReceipts', 'battleSeats',
                'bossTimeline', 'nativeBoss', 'bossOvertimeCharged', 'baselineWon', 'fault'):
        room.pop(key, None)
    room.pop('multiplayerInstanceMigration', None)
    room.update(round=0, reports={}, roundHistory=[], help=None, result=None)
    rooms._phase(room, 'building')
    rooms._event(room, 'party-return-complete', {'matchSequence': old, 'nextMatchSequence': old + 1})


def team_left(rooms, room, seat):
    seat['teamLeft'] = True
    party = members(room)
    if party and room['owner'] == seat['participantId']:
        room['owner'] = party[0]['participantId']
    if room['phase'] == 'result':
        finish_return(rooms, room)


def finish_departed_field(rooms, room, seat):
    """An explicit departure cannot remain a live completion-barrier member."""
    if (room['phase'] == 'battle' and seat.get('state') and
            seat in rooms._battle_members(room) and
            str(seat['seat']) not in room['reports']):
        from .multiplayer_reconnect import unresolved_battle_report
        rooms._self_finish(room, seat, unresolved_battle_report(rooms, room, seat, from_rejoin=False),
                           native_terminal=False)


def apply_party(rooms, room, seat, op, payload):
    if op not in ('scene.quit', 'room.return'):
        return False
    if payload:
        raise DomainError('invalid_party_return_payload')
    if seat.get('teamLeft') or seat.get('left'):
        raise DomainError('participant_left_party')
    if room['phase'] == 'building' and op == 'room.return':
        # A repeated original BackTeamUp cannot reset the next lobby.
        return True
    if op == 'scene.quit':
        if seat.get('sceneLeft'):
            return True  # Native duplicate QuitUp cannot reopen a retired scene.
        if room['phase'] != 'building': seat.setdefault('sceneLeftAtTs', int(rooms.clock()))
        if room['phase'] == 'result':
            seat['sceneLeft'] = True
            rooms._event(room, 'scene-left', {'seat': seat['seat']})
        elif room['phase'] == 'building':
            return True
        else:
            # A mid-match give-up is a lost field, while party membership stays
            # available for the native return button after the team settles.
            seat['sceneLeft'] = True
            rooms._apply(room, seat, 'leave', {}, _retain_party=True)
        return True
    # An individual Quit/Kick may finish its own native return UI while the
    # other fields still fight. Keep the original party without re-entering
    # this actor's abandoned scene or ending the surviving team's match.
    if not (seat.get('quit') and seat.get('sceneLeft')):
        rooms._require(room, 'result')
    if not seat.get('sceneLeft'):
        raise DomainError('leave_battle_scene_before_return')
    if seat.get('returned'):
        return True
    seat.update(returned=True, ready=False, connected=True, lastSeenTs=int(rooms.clock()))
    seat.pop('reconnectSettlementPending', None)
    from .multiplayer_lobby import touch
    touch(rooms, room)
    rooms._event(room, 'party-member-returned', {'seat': seat['seat']})
    finish_return(rooms, room)
    return True
