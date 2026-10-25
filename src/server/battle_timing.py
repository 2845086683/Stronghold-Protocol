"""Versioned timing for private SINGLE combat, distinct from rest-phase limits.

The per-round display table is the user's specified private policy. Native
fixedPlayTime advances at FAST=2.0 and already excludes pause time. Reports keep
their established simulation-second unit; only native UI projection divides by
two. The original boss UI subtracts BossTurnHpReduceTime from forceEndTs.
"""
from .engine import DomainError, integer

VERSION = 1
SINGLE_MODES = ('mode_single_funny', 'mode_single_normal', 'mode_single_hard', 'mode_single_abyss')
NORMAL_DISPLAY_SECONDS = (45, 45, 55, 55, 55, 70, 85, 85, 95, 95, 115, 115, 115)


def configured(catalog, mode_id, round_id):
    """Create a snapshot only at a new manual Ready boundary, never on GET."""
    integer(round_id, 'round', 1, 15)
    if mode_id not in SINGLE_MODES or catalog.mode(mode_id)['ModeType'] != 1:
        raise DomainError('native_battle_timing_mode')
    turn = catalog.turns(mode_id).get(round_id)
    if turn is None:
        raise DomainError('native_battle_timing_round')
    boss = turn['IsBossTurn']
    if not boss and round_id > len(NORMAL_DISPLAY_SECONDS):
        raise DomainError('native_battle_timing_round')
    display = 120 if boss else NORMAL_DISPLAY_SECONDS[round_id - 1]
    offset = integer(turn['BossTurnHpReduceTime'], 'nativeBossOffsetSeconds', 0, 3600) if boss else 0
    return {'version': VERSION, 'clock': 'native-fixed-play-time',
            'elapsedUnit': 'simulation-seconds', 'displayUnit': 'effective-real-seconds',
            'speedNumerator': 2, 'speedDenominator': 1,
            'displayDurationSeconds': display, 'simulationDurationSeconds': display * 2,
            'nativeBossOffsetSeconds': offset, 'nativeForceEndDurationSeconds': display + offset}


def persisted(catalog, mode_id, round_id, source):
    """Absent means legacy; malformed/present snapshots never fall back.

    A completed report supplies its own snapshot even after the run advances.
    Do not normalize accepted legacy reports or change their digest on retry.
    """
    if 'battleTiming' not in source:
        return None
    value = source['battleTiming']
    expected = configured(catalog, mode_id, round_id)
    if (type(value) is not dict or value.keys() != expected.keys() or
            any(type(value[k]) is not type(v) or value[k] != v for k, v in expected.items())):
        raise DomainError('native_battle_timing_snapshot')
    return value


def simulation_duration(catalog, mode_id, round_id, source):
    value = persisted(catalog, mode_id, round_id, source)
    if value is not None:
        return value['simulationDurationSeconds']
    turn = catalog.turns(mode_id)[round_id]
    # Preserve pre-0.1.47 ongoing battles and immutable terminal reports.
    return integer(turn['NormalPhaseTime'], 'legacyNormalPhaseTime', 1, 86400)


def force_end_duration(catalog, mode_id, round_id, source):
    value = persisted(catalog, mode_id, round_id, source)
    return value['nativeForceEndDurationSeconds'] if value is not None else simulation_duration(catalog, mode_id, round_id, source)


def boss_overtime_cost(catalog, mode_id, round_id, source, elapsed):
    integer(elapsed, 'elapsedSeconds', 0, 86400)
    value = persisted(catalog, mode_id, round_id, source)
    if value is not None:
        # First complete effective second AFTER countdown zero costs one HP.
        return max(0, elapsed - value['simulationDurationSeconds']) * value['speedDenominator'] // value['speedNumerator']
    turn = catalog.turns(mode_id)[round_id]
    return max(0, elapsed - (turn['NormalPhaseTime'] - turn['BossTurnHpReduceTime']))
