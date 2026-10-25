"""Read-only native MULTI result over the committed room, never account writes.

CN _SendSettleGameReq (0x602b798) uses AutoChessSettleGameResponse rather
than a battle socket response. Personal formations belong to the recipient;
teamInfo retains every started seat, including an eliminated or departed ally.
"""
import copy


TYPE = 'Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo'


def personal_departure_committed(room, recipient):
    """A persisted voluntary scene exit owns a personal INTERRUPT result.

    The surviving room/run remains live; do not manufacture a team result or
    change its combat state just to acknowledge this player's native unload.
    """
    return bool(room['phase'] != 'result' and recipient.get('state') and
                recipient.get('quit') and
                (recipient.get('sceneLeft') or recipient.get('teamLeft')) and
                not recipient.get('returned'))


def settlement_committed(room, recipient):
    state = recipient.get('state')
    return bool(state and state.get('runId') and
                (personal_departure_committed(room, recipient) or
                 (room['phase'] == 'result' and room.get('result') and
                  state.get('phase') == 'result' and state.get('result'))))


def project_settlement(protocol, room, recipient):
    partial = personal_departure_committed(room, recipient)
    if (not partial and (room['phase'] != 'result' or not room.get('result'))) or not recipient.get('state'):
        return None
    archived = next((row for row in room.get('matchHistory', [])
                     if row['matchSequence'] == room.get('matchSequence', 1)), None)
    if archived is not None and not partial:
        recipient = next(row for row in archived['seats'] if row['participantId'] == recipient['participantId'])
        room = {**room, **archived}
    own = copy.deepcopy(recipient['state'])
    own['nativeStartedAtTs'] = room.get('nativeStartedAtTs', 0)
    own['nativeEndedAtTs'] = (recipient.get('sceneLeftAtTs', own['nativeStartedAtTs']) if partial
                              else room.get('nativeEndedAtTs', 0))
    if partial:
        # Kick(7) opens the ordinary native settlement view even though the
        # surviving team has not ended. Build a read-only display copy: no
        # room result, run archive, reward or teammate state is committed here.
        own['phase'] = 'result'
        own['result'] = {'success': False, 'abandoned': True,
            'roundsPassed': max(map(int, own['completedReports']), default=0)}
    summary = protocol.settlement(own)
    summary['gameFinished'] = not recipient.get('quit', False)
    team = []
    for member in room['seats']:
        state = member.get('state')
        if state is None:
            continue
        display_state = state
        if partial:
            display_state = copy.deepcopy(state)
            display_state['phase'] = 'result'
            display_state['result'] = {'success': False, 'abandoned': bool(member.get('quit')),
                'roundsPassed': max(map(int, state['completedReports']), default=0)}
        personal = protocol.settlement(display_state)
        # A match-wide win does not turn a lost/abandoned personal field into
        # PASS. The room owns the result; no client can provide a gameCode.
        status = ('INTERRUPT' if member.get('quit') else 'DEAD' if state['player']['hp'] <= 0
                  else 'BATTLING' if partial else 'DEAD' if not room['result']['success'] else 'PASS')
        from .multiplayer_trophies import personal_rounds
        passed = personal_rounds(room, member)
        row = copy.deepcopy(personal['teamInfo'][0])
        row.update(uid='private-' + member['participantId'], uidIndex=member['seat'],
                   passRound=passed, gameCode=protocol.c.enum(
                       'Torappu.UI.AutoChess.AutoChessSettleGamePlayerStatus', status))
        from .multiplayer_identity import settlement_card
        settlement_card(row['card'], member)
        row['card']['trophyNum'] = member.get('trophyNum', 0)
        team.append(row)
    summary['teamInfo'] = team
    summary['recordInfos']['trophyRecord'] = recipient.get('trophyAward', {}).get('earned', 0)
    return summary

