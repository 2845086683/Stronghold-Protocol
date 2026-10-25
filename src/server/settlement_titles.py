"""Deterministic title assignment for future private multiplayer settlement.

SPDX-License-Identifier: GPL-3.0-or-later
Adapted from sganggs/Stronghold-Protocol, cf7a9400270d5aa79b2e601e405dab782e25f120,
server/match/results.js and data/tuning.json. Attribution/license are retained
in third_party/stronghold-protocol-reference. Names/icons use the CN catalog.
The ranking/greedy tie policy is the reference's [ASSUMED] server policy, not
an observed official server contract. No route accepts client-supplied scores.
"""
from fractions import Fraction


# Order is significant when rank and relative performance tie. In particular,
# comment_3 uses *loss*, never remaining HP (different strategies start with
# different HP). The reference tuning corrects its generated config here.
RULES = (
    ('comment_1', 'bossDamage', 'max', True),
    ('comment_2', 'activatedLayers', 'max', False),
    ('comment_3', 'lpLost', 'min', False),
    ('comment_4', 'merges', 'max', False),
    ('comment_5', 'itemsEquipped', 'max', False),
    ('comment_6', 'fundsSpent', 'max', False),
)


def assign_titles(catalog, participants, *, victory, is_solo):
    """Return uidIndex -> native title ID from trusted final participant stats.

    A participant has uidIndex (seat 0..3), alive, and stats containing the
    six nonnegative integer totals named in RULES. Missing totals earn no
    positive-stat title; LP-loss eligibility requires an explicit total.
    Unavailable historical data must not be invented as a zero-loss record.
    Each participant/title can be used once. Single-player has no team page
    or team titles. This is a pure boundary ready for a future match service;
    it neither starts multiplayer nor changes existing solo settlements.
    """
    if type(victory) is not bool or type(is_solo) is not bool:
        raise ValueError('invalid_title_mode')
    if not isinstance(participants, list) or not 1 <= len(participants) <= 4:
        raise ValueError('invalid_title_participants')
    seats = set()
    for row in participants:
        if (not isinstance(row, dict) or type(row.get('uidIndex')) is not int or
                not 0 <= row['uidIndex'] <= 3 or row['uidIndex'] in seats or
                type(row.get('alive')) is not bool or not isinstance(row.get('stats'), dict)):
            raise ValueError('invalid_title_participant')
        seats.add(row['uidIndex'])
        for _, stat, _, _ in RULES:
            if stat in row['stats'] and (type(row['stats'][stat]) is not int or row['stats'][stat] < 0):
                raise ValueError('invalid_title_stat')
    if is_solo:
        return {}
    definitions = catalog.season['PlayerTitleDataDict']
    candidates = []
    for order, (title, stat, rule, only_win) in enumerate(RULES):
        if title not in definitions:
            raise ValueError('missing_native_title')
        if only_win and not victory:
            continue
        values = [(row['uidIndex'], row['stats'][stat]) for row in participants
                  if stat in row['stats'] and (rule != 'min' or row['alive'])]
        if not values:
            continue
        ranked = sorted((value for _, value in values), reverse=rule == 'max')
        best = ranked[0]
        if rule == 'max' and best == 0:
            continue
        for seat, value in values:
            if rule == 'max' and value == 0:
                continue
            relative = Fraction(best + 1, value + 1) if rule == 'min' else Fraction(value, best)
            candidates.append((ranked.index(value), -relative, order, seat, title))
    assigned, used = {}, set()
    for _, _, _, seat, title in sorted(candidates):
        if seat not in assigned and title not in used:
            assigned[seat] = title
            used.add(title)
    return assigned
