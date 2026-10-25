"""Explicit deployment coordination around the pure instance-ID migration.

No constructor, room read, GET, reconnect or game command invokes this module.
The stopped-service tool owns backup verification and the SQLite transaction.
"""
import copy

from .engine import DomainError
from . import multiplayer_field
from .multiplayer_instance_ids import plan_migration
from .multiplayer_preparation_view import PreparationViewRooms, member


def migrate_room(room, catalog, protocol, engine, now):
    states = [row['state'] for row in room.get('seats', []) if row.get('state')]
    if not states or all((state.get('multiplayerInstanceNamespace') or {}).get('version') == 2 for state in states):
        return plan_migration(room)
    original = copy.deepcopy(room)
    field = (original.get('help') or {}).get('fieldLoad')
    live_help = original.get('phase') == 'help' and not original.get('closed')
    if live_help and field and field.get('status') != 'loading':
        raise DomainError('instance_migration_running_help_requires_natural_completion')
    if original.get('phase') == 'battle_countdown' and not original.get('closed'):
        # Its two-second own-world barrier cannot be truthfully retained after
        # retiring every native instance/prepare proof. Do not buy more time.
        raise DomainError('instance_migration_countdown_requires_natural_completion')
    unsigned = copy.deepcopy(original)
    if field: unsigned['help'].pop('fieldLoad')
    migrated, audit = plan_migration(unsigned)
    if not audit['changed']:
        return copy.deepcopy(room), audit
    coordinator = PreparationViewRooms.__new__(PreparationViewRooms)
    coordinator.c = catalog; coordinator.protocol = protocol; coordinator.e = engine
    coordinator.clock = lambda: now
    from .multiplayer_field_assets import ASSETS, CATALOG_HASH
    if catalog.hash != CATALOG_HASH:
        raise DomainError('native_field_catalog_mismatch')
    coordinator.field_assets = copy.deepcopy(ASSETS)
    migration = migrated['multiplayerInstanceMigration']
    # Retired signed permissions are kept as audit evidence, never attached to
    # the new live plan or treated as a native world readiness declaration.
    if original.get('preparationViews') is not None:
        migration['retiredPreparationViews'] = copy.deepcopy(original['preparationViews'])
    if field:
        migration['retiredFieldLoad'] = copy.deepcopy(field)
        if live_help:
            multiplayer_field.validate_binding(original, field)
            coordinator.clock = lambda: field['loadDeadlineTs'] - multiplayer_field.LOAD_TIMEOUT_SECONDS
            multiplayer_field.arm(coordinator, migrated, coordinator.field_assets)
            coordinator.clock = lambda: now
            current = migrated['help']['fieldLoad']
            for name in ('loadStartedAtTs', 'loadDeadlineTs', 'skippedObserverSeats'):
                current[name] = copy.deepcopy(field[name])
            current['loadAttemptBySeat'] = {key: value + 1 for key, value in field['loadAttemptBySeat'].items()}
            current['requiredSeats'] = copy.deepcopy(field['requiredSeats'])
            migrated['help']['startedAtTs'] = original['help']['startedAtTs']
            migrated['help']['deadlineTs'] = original['help']['deadlineTs']
            migrated['deadlineTs'] = original['deadlineTs']; migrated['settleAfterTs'] = original.get('settleAfterTs')
            multiplayer_field.validate_binding(migrated, current)
            audit['helpPermissionsReissuedWithOriginalBudget'] = True
    if migrated.get('phase') in ('preparation', 'special') and not migrated.get('closed'):
        old_views = original.get('preparationViews') or {}
        old_barrier = old_views.get('returnBarrier')
        views = coordinator._views(migrated)
        if old_barrier:
            views['returnBarrier'] = copy.deepcopy(old_barrier)
            views['returnBarrier']['readySeats'] = []
        original_seats = {row['seat']: row for row in original['seats']}
        for seat in migrated['seats']:
            if not seat.get('state') or seat.get('left') or seat.get('quit') or seat.get('sceneLeft') or seat.get('returned'):
                continue
            old_seat = original_seats[seat['seat']]
            entry = coordinator._entry(migrated, seat); entry['needsOwnRecovery'] = True
            old_entry = old_views.get('entries', {}).get(str(seat['seat']), {})
            pending = old_entry.get('pending') or old_entry.get('lastCommitted') or {}
            deadline = (old_barrier.get('deadlineTs') if old_barrier else pending.get('deadlineTs'))
            if deadline is None: deadline = original.get('deadlineTs')
            if deadline is None:
                raise DomainError('instance_migration_preparation_budget_missing')
            index = seat['seat'] if old_barrier else old_seat.get('watchSeat', seat['seat'])
            try: target = member(migrated, index)
            except DomainError: target = seat
            if seat['state']['player']['hp'] <= 0: target = coordinator._observer_target(migrated, seat)
            coordinator._issue(migrated, seat, target, 'return-own' if target is seat else 'observe',
                               deadline=deadline, reason='instance-namespace-migration')
        audit['preparationPermissionsReissuedWithOriginalBudget'] = True
    audit['sourceRoomRetainedForInverseVerification'] = True
    audit['sqlReceiptsRewritten'] = False
    return migrated, audit
