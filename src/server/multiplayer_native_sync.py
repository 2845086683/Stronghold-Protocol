"""Flat native syncInfo response for an authenticated private participant.

The result page can issue syncInfo through UISender after its scene detaches.
Reuse participant bootstrap presence/configuration; never enter the legacy
shared profile route or execute another finish/reward operation.
"""
import copy

from .activity_profile import ACTIVITY
from .engine import DomainError


def response(rooms, solo, actor, body):
    if not isinstance(body, dict) or set(body) != {'actId'} or body['actId'] != ACTIVITY:
        raise DomainError('invalid_native_participant_sync')
    bootstrap = solo.bootstrap(actor, rooms.bootstrap(actor))
    result = copy.deepcopy(bootstrap['syncInfo'])
    return {'result': 0, **result, 'playerDataDelta': {'modified': {}, 'deleted': {}}}
