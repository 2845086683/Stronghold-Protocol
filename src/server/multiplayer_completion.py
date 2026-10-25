"""Durable, phase-scoped replay identity for native terminal reports.

The official client retransmits SelfBattleFinish/HelpBattleFinish with a new
transport request even after a phase changes (TC 2026-10-20). HTTP requestId
deduplication alone therefore cannot protect round settlement. This private
ledger supplements, rather than replaces, immutable command receipts.
"""
from .engine import DomainError, integer


def request(room, seat, op, payload, digest, protocol_ids=None):
    if not isinstance(op, str): return None
    if op == 'protocol':
        name = payload.get('type')
        if type(name) is int and protocol_ids:
            name = next((key for key, value in protocol_ids.items() if value == name), '')
        if not isinstance(name, str): return None
        name = name.rsplit('+', 1)[-1]
        phase = {'AutoChessBattleSceneSelfBattleFinishUp': 'self',
                 'AutoChessBattleSceneHelpBattleFinishUp': 'help'}.get(name)
        data = payload.get('payload', {})
    else:
        phase = {'battle.finish': 'self', 'help.finish': 'help'}.get(op)
        data = payload
    if phase is None: return None
    if not isinstance(data, dict): raise DomainError('invalid_payload')
    # Rejoin reports describe a lost world, not an ordinary terminal replay.
    # Their existing committed-kill recovery policy remains the only owner.
    if data.get('fromRejoin') is True: return None
    round_id = integer(data.get('round'), 'round', 1, 15)
    key = ':'.join(map(str, (room.get('matchSequence', 1), seat['seat'], phase, round_id)))
    canonical = {**data, 'fromRejoin': data.get('fromRejoin', False)}
    from .multiplayer_instance_ids import historical_fingerprints
    return {'key': key, 'fingerprint': digest(canonical),
            'compatibleFingerprints': historical_fingerprints(room, canonical), 'phase': phase,
            'round': round_id, 'seat': seat['seat'], 'data': data}


def replayed(room, candidate):
    if candidate is None: return False
    if candidate['phase'] == 'help':
        from .joint_defense import authority_seat
        current = room.get('help')
        history = next((row for row in room.get('roundHistory', []) if row['round'] == candidate['round']), None)
        plan = (current if current and current.get('round') == candidate['round'] else
                history.get('jointDefense') if history else None)
        if plan and candidate['seat'] in plan['spectatorSeats'] and candidate['seat'] != authority_seat(plan):
            # A renderer can finish/retry with a different local journal. Its
            # ACK is never a competing outcome, including after an earlier ACK.
            if plan.get('completedReport'): return True
            if room['phase'] == 'help' and room['round'] == candidate['round']: return False
    previous = room.get('terminalReceipts', {}).get(candidate['key'])
    if previous is None:
        # Another authenticated observer may report the same completed shared
        # field after its defender already advanced the room. Validate the
        # complete partition against the immutable original plan; no current
        # phase, HP, RNG, effects or controller authority is changed.
        if candidate['phase'] == 'help':
            import copy
            from .joint_defense import authority_seat, finish
            history = next((row for row in room.get('roundHistory', [])
                            if row['round'] == candidate['round']), None)
            plan = history.get('jointDefense') if history else None
            if plan and plan.get('completedReport'):
                if candidate['seat'] in plan['spectatorSeats'] and candidate['seat'] != authority_seat(plan):
                    # A late observer has no settlement authority. Accept its
                    # phase-scoped ACK even when its local simulation differed;
                    # never replace the saved defender partition or HP/reward.
                    return True
                finish(copy.deepcopy(plan), candidate['data'], reporter_seat=candidate['seat'])
                return True
        return False
    if previous not in candidate.get('compatibleFingerprints', {candidate['fingerprint']}):
        raise DomainError('conflicting_round_report')
    return True


def commit(room, candidate):
    if candidate is not None:
        room.setdefault('terminalReceipts', {})[candidate['key']] = candidate['fingerprint']


def special_replayed(room, seat, op, payload, generation, protocol_ids):
    if op != 'protocol': return False
    name = payload.get('type')
    if type(name) is int:
        name = next((key for key, value in protocol_ids.items() if value == name), '')
    if not isinstance(name, str) or name.rsplit('+', 1)[-1] != 'AutoChessBattleSceneSpPreparationPickUp':
        return False
    previous = room.get('specialPickReceipts', {}).get(str(seat['seat']))
    if not previous or previous['round'] != room['round']:
        return False
    data = payload.get('payload', {})
    if not isinstance(data, dict) or set(data) != {'slotId'}:
        raise DomainError('invalid_special_pick_payload')
    slot = integer(data['slotId'], 'slotId', 0, 5)
    if slot != previous['slotId']:
        raise DomainError('conflicting_special_choice')
    if not previous['generation'] <= generation <= room['generation']:
        return False
    return True
