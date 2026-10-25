# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Semantics reviewed against match/builtinMeta.js at the pinned reference
# revision documented by ../third_party/stronghold-protocol-reference/NOTICE.md.
"""Room-owned delivery for item effects that send a former carrier to a peer.

Selection happens when the consumable is equipped. Delivery uses the same
native acquisition/merge/hand layout as ordinary grants, at the next rest.
Personal nativeSquad and DIY configuration are never overwritten by a gift.
"""
import copy

from .catalog import draw
from .hand_layout import inventory, reconcile
from .preset_snapshot import wire_squad


def _mates(rooms, room, sender):
    return [row for row in rooms._alive(room) if row['seat'] != sender]


def _select(rooms, room, sender, bonds):
    peers = _mates(rooms, room, sender['seat'])
    if not peers:
        return None
    wanted = {rooms.c.season['BondInfoDict'][key]['Identifier'] for key in bonds
              if key in rooms.c.season['BondInfoDict']}
    scores = {row['seat']: sum(bond['charCnt'] for bond in row['state']['player']['bonds']
                               if bond['bondIdentifier'] in wanted) for row in peers}
    best = max(scores.values())
    candidates = sorted((row for row in peers if scores[row['seat']] == best), key=lambda row: row['seat'])
    return draw(sender['state'], candidates)


def queue(rooms, room, sender):
    """Bind successful server-generated equipment intents to this match once."""
    intents = sender['state'].pop('pendingTeamItemGifts', [])
    for intent in intents:
        recipient = _select(rooms, room, sender, intent['bonds'])
        ident = f"{room.get('matchSequence', 1)}:{sender['seat']}:{intent['sourceItemInstId']}"
        gifts = room.setdefault('itemGifts', [])
        if any(row['id'] == ident for row in gifts):
            continue
        gift = {**copy.deepcopy(intent), 'id': ident, 'senderSeat': sender['seat'],
                'matchSequence': room.get('matchSequence', 1), 'senderRunId': sender['state']['runId'],
                'recipientSeat': recipient['seat'] if recipient else None,
                'dueRound': intent['fromRound'] + 1,
                'status': 'pending' if recipient else 'no-recipient'}
        gifts.append(gift)
        rooms._event(room, 'item-gift-queued' if recipient else 'item-gift-unavailable',
                     {'giftId': ident, 'senderSeat': sender['seat'],
                      'recipientSeat': gift['recipientSeat'], 'dueRound': gift['dueRound'],
                      'reason': None if recipient else 'no-living-teammate'}, [sender['seat']])


def _destination(catalog, gift, target):
    original = gift['chessId']
    base = catalog.season['ChessNormalIdLookupDict'].get(original, original)
    if base not in catalog.season['DiyChessDict']:
        return original
    # A DIY slot denotes its owner's selected operator. Reusing the sender's
    # slot number would silently turn the gift into the recipient's different
    # operator. Map only to a genuine matching registered configuration.
    source = gift.get('configuration')
    if source is None:
        return None
    fields = ('charId', 'skillIndex', 'currentEquip', 'cultivateEffectId', 'potentialRank', 'bondList')
    for row in wire_squad(catalog, target):
        if row['chessId'] not in catalog.season['DiyChessDict']:
            continue
        if any(row.get(key) != source.get(key) for key in fields):
            continue
        key = row['chessId']
        if catalog.chess[original].get('IsGolden'):
            key = catalog.chess[key].get('UpgradeChessId')
        if key:
            return key
    return None


def _unmapped(catalog, state):
    mapped = {row['instId'] for row in reconcile(catalog, state)}
    return set(inventory(catalog, state)) - mapped


def deliver(rooms, room):
    """Deliver after all living seats have started their next rest period.

    Full original hand+temporary cells defer the complete acquisition rather
    than deleting the item or adding invisible inventory. No partial gain,
    RNG, trait or merge event escapes the candidate when a delivery is held.
    A dead sender's pending gift is still considered; a dead receiver is
    reselected among the remaining living teammates by the same rule.
    """
    members = {row['seat']: row for row in room['seats']}
    def defer(gift, status):
        if gift['status'] != status:
            rooms._event(room, 'item-gift-deferred', {'giftId': gift['id'], 'reason': status,
                'senderSeat': gift['senderSeat'], 'recipientSeat': gift['recipientSeat']},
                list({gift['senderSeat'], gift['recipientSeat']} - {None}))
        gift['status'] = status
    for gift in room.get('itemGifts', []):
        if gift['status'] in ('delivered', 'no-recipient', 'expired-match') or gift['dueRound'] > room['round']:
            continue
        sender = members[gift['senderSeat']]
        if (gift.get('matchSequence') != room.get('matchSequence', 1) or
                gift.get('senderRunId') != sender['state']['runId']):
            defer(gift, 'expired-match')
            continue
        eligible = {row['seat']: row for row in _mates(rooms, room, sender['seat'])}
        target = eligible.get(gift['recipientSeat'])
        if target is None:
            target = _select(rooms, room, sender, gift['bonds'])
            if target is None:
                defer(gift, 'awaiting-teammate')
                continue
            gift['recipientSeat'] = target['seat']
        key = _destination(rooms.c, gift, target['state'])
        if key is None:
            defer(gift, 'awaiting-native-slot')
            continue
        candidate = copy.deepcopy(target['state'])
        previously_unmapped = _unmapped(rooms.c, candidate)
        owner = rooms.e._gain(candidate, key, stock='fixed')
        rooms.e.pool.prune_displayed(candidate)
        if _unmapped(rooms.c, candidate) - previously_unmapped:
            defer(gift, 'awaiting-hand-space')
            continue
        saved_players = candidate.get('players')
        candidate['players'] = [{'uidIndex': seat} for seat in sorted(members)]
        rooms.e.broadcasts.event(candidate, 'CHAR_GIFT', senderUidIndex=sender['seat'],
            recipientUidIndex=target['seat'], transferId=gift['id'], chessId=key)
        if saved_players is None: candidate.pop('players')
        else: candidate['players'] = saved_players
        target['state'] = candidate
        gift.update(status='delivered', deliveredRound=room['round'],
                    deliveredChessId=key, deliveredInstId=owner['instId'])
        rooms._event(room, 'item-gift-delivered', {'giftId': gift['id'], 'senderSeat': sender['seat'],
            'recipientSeat': target['seat'], 'chessId': key, 'instId': owner['instId']}, [target['seat']])
