"""Private alliance trophies, awarded atomically with the committed match.

Rules: user supplied alliance reward table, 2026-10-22. The hidden round is
the catalog's additional boss round (15), never a bonus for merely entering it.
No official inventory, account endpoint or client-supplied score is used.
"""
import json
from .engine import DomainError, integer
from .storage import dump

REWARDS = {
    'mode_multi_funny': (0, 1, 2, 2, 3, 3),
    'mode_multi_normal': (0, 1, 2, 3, 4, 5),
    'mode_multi_hard': (0, 1, 2, 3, 5, 7),
    'mode_multi_abyss': (0, 1, 2, 3, 6, 8),
}


def reward(mode, passed):
    integer(passed, 'passed_rounds', 0, 15)
    if mode not in REWARDS: return 0
    index = 0 if passed <= 4 else 1 if passed <= 8 else 2 if passed <= 11 else 3 if passed <= 13 else 4 if passed == 14 else 5
    return REWARDS[mode][index]


def personal_rounds(room, member):
    state = member['state']
    last = max(map(int, state['completedReports']), default=0)
    passed = room['result']['roundsPassed'] if room.get('result') else last
    if member.get('quit') or state['player']['hp'] <= 0:
        passed = min(passed, max(0, last - (state['player']['hp'] <= 0)))
    return passed


def initialize(db):
    db.execute('''CREATE TABLE IF NOT EXISTS multiplayer_trophy_awards(
        room TEXT NOT NULL, match_sequence INTEGER NOT NULL, actor TEXT NOT NULL,
        award TEXT NOT NULL, PRIMARY KEY(room,match_sequence,actor))''')


def commit(rooms, room):
    """Caller owns the room transaction; GET/settleGame never grants rewards."""
    from .multiplayer_settlement import settlement_committed
    sequence = room.get('matchSequence', 1)
    for member in room['seats']:
        if not settlement_committed(room, member): continue
        actor = member['participantId']; key = (room['roomId'], sequence, actor)
        saved = rooms.store.db.execute('SELECT award FROM multiplayer_trophy_awards WHERE room=? AND match_sequence=? AND actor=?', key).fetchone()
        if saved:
            award = json.loads(saved[0])
        else:
            passed = personal_rounds(room, member)
            amount = reward(room['modeId'], passed)
            profile = rooms._profile(actor)
            before = integer(profile.get('trophyNum', 0), 'trophy_total', 0, 2147483647)
            total = before + amount
            if total > 2147483647: raise DomainError('trophy_total_overflow')
            award = {'version': 1, 'modeId': room['modeId'], 'runId': member['state']['runId'],
                     'passedRounds': passed, 'earned': amount, 'before': before, 'total': total}
            rooms.store.db.execute('INSERT INTO multiplayer_trophy_awards VALUES(?,?,?,?)', (*key, dump(award)))
            profile['trophyNum'] = total
            rooms.store.db.execute('UPDATE multiplayer_participants SET profile=? WHERE id=?', (dump(profile), actor))
        if award['runId'] != member['state']['runId']: raise DomainError('trophy_match_owner_changed')
        member['trophyAward'] = award
        member['trophyNum'] = award['total']
        # A result produced before this feature may already be archived. Only
        # attach this run's ledger entry; never recompute the archived outcome.
        for history in room.get('matchHistory', []):
            if history['matchSequence'] != sequence: continue
            for prior in history['seats']:
                if prior['participantId'] == actor and prior.get('state', {}).get('runId') == award['runId']:
                    prior['trophyAward'] = award
                    prior['trophyNum'] = award['total']
