# SPDX-License-Identifier: GPL-3.0-or-later
"""Authenticated preparation observation and return-own loading barriers.

Original room transactions, economy, special choices and battle accounting
remain inherited. A client ACK is a bound declaration, not Unity attestation.
"""
import copy
from .engine import DomainError, integer
from .multiplayer import PREP_OPS
from .multiplayer_field import NativeFieldRooms, digest, source
from .multiplayer_projection import project_room, project_team

VERSION = 1
TIMEOUT = 60
AUTOMATIC_RECOVERY_LIMIT = 1
FAILURES = frozenset(('resource', 'scene', 'view', 'terrain', 'prepare', 'runtime'))
ACK_FIELDS = frozenset(('planHash', 'attempt', 'stageId', 'targetSeat', 'terrainSha256', 'targetRevision'))


def member(room, index):
    value = next((s for s in room['seats'] if s['seat'] == index and not s.get('left') and not s.get('quit')), None)
    if value is None or not value.get('state'):
        raise DomainError('preparation_view_target_unavailable')
    return value


def presentation_member(room, index):
    # A departed target keeps its immutable roster/state until the match ends.
    # Show that last committed board while loading own; departure never proves
    # that the observer's physical scene has already returned.
    value = next((s for s in room['seats'] if s['seat'] == index and not s.get('left')), None)
    if value is None or not value.get('state'):
        raise DomainError('preparation_view_presentation_unavailable')
    return value


class PreparationViewRooms(NativeFieldRooms):
    """Unavailable returners retain their durable board and reconnect grace."""

    def _views(self, room):
        value = room.get('preparationViews')
        if value and value['round'] == room['round'] and value['matchSequence'] == room.get('matchSequence', 1):
            return value
        value = {'version': VERSION, 'round': room['round'], 'matchSequence': room.get('matchSequence', 1),
                 'entries': {}, 'returnBarrier': None, 'nativeRuntimeValidated': False}
        for seat in room['seats']:
            if seat.get('state') and not seat.get('left') and not seat.get('quit'):
                value['entries'][str(seat['seat'])] = {'committedSeat': seat['seat'], 'proof': None,
                    'pending': None, 'attempt': 0, 'needsOwnRecovery': False}
                seat['watchSeat'] = seat['seat']
        room['preparationViews'] = value
        return value

    def _entry(self, room, seat):
        return self._views(room)['entries'][str(seat['seat'])]

    def _observer_target(self, room, seat):
        alive = [row for row in self._alive(room) if row.get('state') and row is not seat]
        return next((row for row in alive if row['seat'] == seat.get('watchSeat')),
                    min(alive, key=lambda row: row['seat']) if alive else seat)

    def _initial_views(self, room):
        self._views(room)
        for seat in room['seats']:
            if seat.get('state') and not seat.get('left') and not seat.get('quit') and not seat.get('transportSuspension'):
                entry = self._entry(room, seat)
                if not entry['proof'] and not entry['pending'] and not entry.get('lastFailure'):
                    entry['needsOwnRecovery'] = True
                    target = self._observer_target(room, seat) if seat['state']['player']['hp'] <= 0 else seat
                    self._issue(room, seat, target, 'return-own' if target is seat else 'observe',
                                reason='initial-round-view')

    def target_projection(self, room, actor, target):
        """Reuse complete native projection; change recipient, never write owner.

        The target's own projection supplies that target's field and board.
        The actor's real team/self identity and the complete static roster are
        retained. This is a presentation copy, never an economy command scope.
        """
        # project_room/Protocol.project own their presentation copies. Only
        # watchSeat changes here; cloning durable history, checkpoints and the
        # whole boss step journal on every proof check is unnecessary work.
        presentation = dict(room)
        presentation['seats'] = [dict(row) for row in room['seats']]
        selected = presentation_member(presentation, target['seat'])
        selected['watchSeat'] = selected['seat']
        projection = project_room(self.protocol, presentation, target['seat'])
        projection.update(project_team(self.protocol, room, actor))
        projection['view'].update(recipientSeat=actor['seat'], observedSeat=target['seat'], writeSeat=actor['seat'])
        for packet in ('allStateSync', 'sceneStatus', 'preparation'):
            if projection.get(packet) is None:
                raise DomainError('preparation_view_projection_unavailable')
        all_data = projection['allStateSync']['payload']['allStateSyncData']
        all_data['uidIndex'] = actor['seat']
        all_data['sceneDetail']['obUidIndex'] = target['seat']
        all_data['sceneDetail']['preparationStatus']['uidIndex'] = target['seat']
        projection['sceneStatus']['payload']['sceneStatus']['obUidIndex'] = target['seat']
        projection['sceneStatus']['payload']['sceneStatus']['preparationStatus']['uidIndex'] = target['seat']
        # Standalone preparation RESP completes an actor command. Only scene
        # Detail's preparation is the observed target's presentation.
        projection['preparation'] = self.protocol.project(actor['state'])['preparation']
        projection['preparation']['payload']['preparationStatus']['uidIndex'] = actor['seat']
        return projection

    def _terrain(self, room, actor, target):
        projection = self.target_projection(room, actor, target)
        effects = projection['allStateSync']['payload']['allStateSyncData']['sceneDetail']['specialEffects']
        return digest(effects), projection

    def _issue(self, room, seat, target, kind, *, deadline=None, reason='requested'):
        entry = self._entry(room, seat)
        if not reason.startswith('native-'):
            entry['automaticRecoveryFailures'] = 0
        room['preparationViewSequence'] = room.get('preparationViewSequence', 0) + 1
        entry['attempt'] += 1
        terrain, _ = self._terrain(room, seat, target)
        stage = target['state']['stageId']
        plan = {'version': VERSION, 'roomId': room['roomId'], 'matchSequence': room.get('matchSequence', 1),
            'round': room['round'], 'seat': seat['seat'], 'runId': seat['state']['runId'],
            'kind': kind, 'phase': room['phase'], 'targetSeat': target['seat'],
            'observerOnly': seat['state']['player']['hp'] <= 0,
            'targetRunId': target['state']['runId'], 'targetStageId': stage,
            'targetLevelId': 'activities/' + stage.split('_', 1)[0] + '/level_' + stage,
            'source': source(self.c, self.field_assets, stage), 'terrainSha256': terrain,
            'viewGeneration': room['preparationViewSequence']}
        # Attempts/revisions cannot buy more time for a single loading cycle.
        deadline = int(self.clock()) + TIMEOUT if deadline is None else deadline
        entry['pending'] = {'version': VERSION, 'plan': plan, 'planHash': digest(plan), 'attempt': entry['attempt'],
                            'deadlineTs': deadline, 'status': 'loading'}
        entry.pop('lastFailure', None)
        self._event(room, 'preparation-view-loading', {'seat': seat['seat'], 'targetSeat': target['seat'],
                    'kind': kind, 'planHash': digest(plan), 'attempt': entry['attempt'], 'reason': reason}, [seat['seat']])
        return entry['pending']

    def _validate_plan(self, room, seat, pending):
        plan = pending['plan']
        if (pending['planHash'] != digest(plan) or plan['roomId'] != room['roomId'] or
            plan['matchSequence'] != room.get('matchSequence', 1) or plan['round'] != room['round'] or
            plan['seat'] != seat['seat'] or plan['runId'] != seat['state']['runId'] or
            (plan['phase'] != room['phase'] and not (plan['observerOnly'] and room['phase'] in ('battle_countdown', 'battle', 'boss'))) or plan['kind'] not in ('observe', 'return-own')):
            raise DomainError('preparation_view_binding_mismatch')
        target = member(room, plan['targetSeat'])
        terrain, projection = self._terrain(room, seat, target)
        if (target['state']['runId'] != plan['targetRunId'] or target['state']['stageId'] != plan['targetStageId'] or
            plan['source'] != source(self.c, self.field_assets, target['state']['stageId']) or terrain != plan['terrainSha256']):
            raise DomainError('preparation_view_target_changed')
        return target, projection

    def _own_confirmed(self, room, seat):
        entry = self._entry(room, seat); proof = entry['proof']
        if entry['pending'] or entry['needsOwnRecovery'] or entry['committedSeat'] != seat['seat'] or not proof:
            return False
        terrain, _ = self._terrain(room, seat, seat)
        return (proof['roomId'] == room['roomId'] and proof['matchSequence'] == room.get('matchSequence', 1) and
                proof['round'] == room['round'] and proof['runId'] == seat['state']['runId'] and
                (proof['phase'] == room['phase'] or proof['phase'] == 'preparation' and room['phase'] in ('battle_countdown', 'battle', 'boss')) and
                proof['targetSeat'] == seat['seat'] and proof['stageId'] == seat['state']['stageId'] and
                proof['terrainSha256'] == terrain)

    def _arm_return(self, room, trigger):
        views = self._views(room)
        if views['returnBarrier'] and views['returnBarrier']['status'] == 'loading':
            return
        now = int(self.clock())
        from .multiplayer_reconstruction import suspend, PRESENCE_TIMEOUT_SECONDS
        for seat in self._alive(room):
            if not seat.get('transportSuspension') and (not seat.get('connected') or
                    now - seat['lastSeenTs'] > PRESENCE_TIMEOUT_SECONDS):
                suspend(self, room, seat, 'absent-at-preparation-return')
        required = [s['seat'] for s in self._alive(room) if s.get('connected') and not s.get('transportSuspension')]
        views['returnBarrier'] = {'version': VERSION, 'status': 'loading', 'trigger': trigger,
            'requiredSeats': required, 'deadlineTs': now + TIMEOUT, 'readySeats': [], 'withdrawnSeats': [],
            'startedAtTs': now, 'countdownStartedAtTs': None, 'policy': 'all-alive-return-or-withdraw-v1'}
        # Remain in PREPARE; native forceEndTs is suppressed by _view below.
        if room['phase'] != 'preparation':
            NativeFieldRooms._phase(self, room, 'preparation')
        room['deadlineTs'] = now + TIMEOUT; room['settleAfterTs'] = None
        for seat in room['seats']:
            if not seat.get('state') or seat.get('left') or seat.get('quit'): continue
            if seat['state']['player']['hp'] > 0 and (not seat.get('connected') or seat.get('transportSuspension')): continue
            entry = self._entry(room, seat); entry['pending'] = None
            if seat['seat'] in required or seat['state']['player']['hp'] <= 0:
                entry['needsOwnRecovery'] = True
                target = self._observer_target(room, seat) if seat['state']['player']['hp'] <= 0 else seat
                self._issue(room, seat, target, 'return-own' if target is seat else 'observe',
                            deadline=now + TIMEOUT, reason=trigger)
        self._event(room, 'preparation-return-own', {'requiredSeats': required, 'deadlineTs': now + TIMEOUT})
        self._countdown(room)

    def _countdown(self, room, *, timeout_release=False):
        barrier = self._views(room)['returnBarrier']
        if not barrier or barrier['status'] != 'loading': return False
        if int(self.clock()) >= barrier['deadlineTs'] and not timeout_release:
            raise DomainError('preparation_return_expired')
        alive = [s for s in self._alive(room) if not s.get('transportSuspension')]
        if any(not s.get('connected') or not self._own_confirmed(room, s) for s in alive): return False
        barrier['readySeats'] = [s['seat'] for s in alive]
        barrier['status'] = 'countdown'; barrier['countdownStartedAtTs'] = int(self.clock())
        # The bound own-world acknowledgements are the start barrier. Keep the
        # historical status for saved countdown compatibility, but do not add
        # another timed phase after every live world has actually returned.
        self._event(room, 'preparation-own-views-ready', {'seats': barrier['readySeats'], 'countdownSeconds': 0})
        self._start_battle(room)
        return True

    def _phase(self, room, phase, seconds=None):
        if phase == 'battle_countdown' and room['phase'] == 'preparation':
            self._arm_return(room, 'all-ready-or-deadline'); return
        super()._phase(room, phase, seconds)
        if phase in ('preparation', 'special') and room['round'] > 0:
            self._initial_views(room)

    def _start_battle(self, room):
        barrier = self._views(room)['returnBarrier']
        if (not barrier or barrier['status'] != 'countdown' or
            any(not seat.get('connected') or not self._own_confirmed(room, seat)
                for seat in self._alive(room) if not seat.get('transportSuspension'))):
            self._arm_return(room, 'world-changed-during-countdown'); return
        super()._start_battle(room)
        barrier['status'] = 'started'
        from .multiplayer_dead_battle import capture
        capture(self, room)

    def _refresh_pending(self, room):
        if room['phase'] not in ('preparation', 'special'): return
        views = self._views(room)
        for seat in room['seats']:
            if not seat.get('state') or seat.get('quit') or seat.get('left') or seat.get('transportSuspension'): continue
            entry = self._entry(room, seat); pending = entry['pending']
            if not pending:
                proof = entry['proof']
                if not proof: continue
                target = next((s for s in room['seats'] if s['seat'] == entry['committedSeat'] and s.get('state') and not s.get('quit') and not s.get('left')), seat)
                if target is seat and seat['state']['player']['hp'] <= 0:
                    target = self._observer_target(room, seat)
                terrain, _ = self._terrain(room, seat, target)
                if (entry['committedSeat'] != target['seat'] or proof['phase'] != room['phase'] or
                        proof['stageId'] != target['state']['stageId'] or proof['terrainSha256'] != terrain):
                    self._issue(room, seat, target, 'return-own' if target is seat else 'observe', reason='committed-terrain-changed')
                continue
            try: self._validate_plan(room, seat, pending)
            except DomainError as error:
                if error.code not in ('preparation_view_target_changed', 'preparation_view_binding_mismatch', 'preparation_view_target_unavailable'): raise
                target = next((s for s in room['seats'] if s['seat'] == pending['plan']['targetSeat'] and s.get('state') and not s.get('quit') and not s.get('left')), seat)
                if target is seat and seat['state']['player']['hp'] <= 0:
                    target = self._observer_target(room, seat)
                kind = pending['plan']['kind'] if target is not seat else 'return-own'
                deadline = None if pending['plan']['phase'] != room['phase'] else pending['deadlineTs']
                self._issue(room, seat, target, kind, deadline=deadline, reason='target-changed')

    def _ack(self, room, seat, payload):
        if set(payload) != ACK_FIELDS: raise DomainError('invalid_preparation_view_ack')
        entry = self._entry(room, seat); pending = entry['pending']
        if not pending: raise DomainError('preparation_view_no_pending_plan')
        observer_return = (seat['state']['player']['hp'] <= 0 and pending['plan']['observerOnly'] and
                           room['phase'] in ('battle_countdown', 'battle', 'boss'))
        if not observer_return: self._require(room, 'preparation', 'special')
        if int(self.clock()) >= pending['deadlineTs']: raise DomainError('preparation_view_loading_expired')
        target, _ = self._validate_plan(room, seat, pending)
        from .multiplayer_dead_battle import binding as dead_battle_binding
        observed_battle = dead_battle_binding(room, seat)
        revision = observed_battle[1]['revision'] if observed_battle else target['state']['revision']
        if (payload['planHash'] != pending['planHash'] or
            integer(payload['attempt'], 'attempt', 1) != pending['attempt'] or
            payload['stageId'] != pending['plan']['targetStageId'] or
            integer(payload['targetSeat'], 'targetSeat', 0, 3) != target['seat'] or
            payload['terrainSha256'] != pending['plan']['terrainSha256'] or
            integer(payload['targetRevision'], 'targetRevision') != revision):
            raise DomainError('preparation_view_stale_ack')
        if not seat.get('connected'): raise DomainError('preparation_view_reconnect_required')
        barrier = self._views(room)['returnBarrier']
        if barrier and not pending['plan']['observerOnly'] and (barrier['status'] != 'loading' or seat['seat'] not in barrier['requiredSeats'] or pending['plan']['kind'] != 'return-own'):
            raise DomainError('preparation_view_superseded_by_return')
        entry['committedSeat'] = target['seat']; seat['watchSeat'] = target['seat']
        entry['proof'] = {'roomId': room['roomId'], 'matchSequence': room.get('matchSequence', 1), 'round': room['round'],
            'phase': room['phase'], 'seat': seat['seat'], 'targetRevision': payload['targetRevision'],
            'runId': seat['state']['runId'], 'targetSeat': target['seat'], 'stageId': payload['stageId'],
            'terrainSha256': payload['terrainSha256'], 'planHash': pending['planHash'], 'attempt': pending['attempt']}
        entry['lastCommitted'] = {**copy.deepcopy(pending), 'status': 'committed'}
        entry['pending'] = None; entry['needsOwnRecovery'] = False
        self._event(room, 'preparation-view-committed', {'seat': seat['seat'], 'targetSeat': target['seat'], 'planHash': payload['planHash']}, [seat['seat']])
        if barrier and not pending['plan']['observerOnly']: self._countdown(room)

    def _apply(self, room, seat, op, payload, **kwargs):
        if not isinstance(op, str): raise DomainError('invalid_preparation_operation')
        if op == 'heartbeat' and seat.get('transportSuspension'):
            # An authenticated recovered transport is an automatic reconnect.
            # Keep this round's lost-field waiting; preparation gets its own
            # fresh native view proof, and absence stops accumulating rounds.
            op = 'reconnect'
        # A native QuitUp is the same authenticated scene departure as the
        # private adapter operation. Resolve it before view/loading gates:
        # an unavailable world must never prevent a player from abandoning it.
        if op == 'protocol':
            name = payload.get('type')
            if type(name) is int:
                name = next((key for key, value in self.c.constants['protocolIds'].items() if value == name), '')
            if isinstance(name, str) and name.rsplit('+', 1)[-1] == 'AutoChessBattleSceneQuitUp':
                op, payload = 'scene.quit', payload.get('payload', {})
                if not isinstance(payload, dict): raise DomainError('invalid_protocol_type')
        if op in ('leave', 'scene.quit'):
            result = super()._apply(room, seat, op, payload, **kwargs)
            if room['phase'] in ('preparation', 'special'):
                self._refresh_pending(room)
                barrier = self._views(room)['returnBarrier']
                if (barrier and barrier['status'] == 'loading' and
                        int(self.clock()) < barrier['deadlineTs']):
                    self._countdown(room)
            return result
        if seat.get('quit') or seat.get('sceneLeft') or seat.get('returned'):
            # Retained party members have no world/view proof in later rounds.
            # Their heartbeat, reconnect and result-return still reach the
            # original party handler; they cannot recreate a view lease.
            return super()._apply(room, seat, op, payload, **kwargs)
        if op == 'reconnect' and seat.get('transportSuspension'):
            # An expired view lease is not a voluntary Quit. Give the returning
            # actor a fresh own-view proof, without restoring another seat's
            # scene or extending the team's original return deadline.
            if room['phase'] in ('preparation', 'special'):
                barrier = self._views(room)['returnBarrier']
                if barrier and barrier['status'] == 'loading' and int(self.clock()) >= barrier['deadlineTs']:
                    self._expire_views(room)
                if room['phase'] in ('preparation', 'special'):
                    from .multiplayer_reconstruction import reconnect
                    reason = seat['transportSuspension']['reason']
                    reconnect(self, room, seat)
                    entry = self._entry(room, seat)
                    preserve = (payload.get('unityLost') is not True and
                        reason in ('transport-disconnected', 'presence-timeout') and
                        (entry['pending'] is not None or entry['proof'] is not None))
                    if not preserve:
                        entry['pending'] = None; entry['proof'] = None; entry['needsOwnRecovery'] = True
                        target = self._observer_target(room, seat) if seat['state']['player']['hp'] <= 0 else seat
                        self._issue(room, seat, target, 'return-own' if target is seat else 'observe',
                            deadline=barrier['deadlineTs'] if barrier and barrier['status'] == 'loading' else None,
                            reason='transport-reconnected')
                    if barrier and seat['seat'] not in barrier['requiredSeats']:
                        barrier['requiredSeats'].append(seat['seat'])
            if room['phase'] in ('battle_countdown', 'battle', 'help', 'boss'):
                payload = {**payload, 'unityLost': True}
        if room['phase'] in ('preparation', 'special'): self._initial_views(room)
        expiry_choice = op == 'choice.pick' and getattr(self, '_server_expiry_depth', 0) > 0
        if (room['phase'] in ('preparation', 'special', 'battle_countdown', 'battle', 'boss') and op == 'reconnect' and
                payload.get('unityLost') is not True and seat.get('state') and seat['state']['player']['hp'] <= 0):
            entry = self._entry(room, seat)
            if not entry['pending'] and entry.get('lastFailure'):
                target = self._observer_target(room, seat)
                self._issue(room, seat, target, 'return-own' if target is seat else 'observe',
                            reason='observer-reconnected-after-exclusion')
        if room['phase'] in ('battle_countdown', 'battle', 'boss') and seat.get('state') and seat['state']['player']['hp'] <= 0:
            pending = self._entry(room, seat)['pending']
            if pending and int(self.clock()) >= pending['deadlineTs'] and op not in ('leave', 'disconnect', 'scene.quit'):
                raise DomainError('preparation_view_loading_expired')
        if op.startswith('view.') and room['phase'] not in ('preparation', 'special'):
            if op == 'view.ready' and seat.get('state') and seat['state']['player']['hp'] <= 0:
                return self._ack(room, seat, payload)
            if op == 'view.failed' and seat.get('state') and seat['state']['player']['hp'] <= 0:
                pending = self._entry(room, seat)['pending']
                self._validate_failure(room, seat, pending, payload)
                if not pending['plan']['observerOnly']:
                    raise DomainError('preparation_view_not_in_rest')
                self._exclude_observer(room, seat, pending, 'native-' + payload['reason']); return
            raise DomainError('preparation_view_not_in_rest')
        if (room['phase'] in ('battle_countdown', 'battle', 'boss') and op == 'reconnect' and payload.get('unityLost') is True and
                seat.get('state') and seat['state']['player']['hp'] <= 0):
            entry = self._entry(room, seat); pending = entry['pending']; entry['proof'] = None; entry['needsOwnRecovery'] = True
            target = self._observer_target(room, seat)
            self._issue(room, seat, target, 'return-own' if target is seat else 'observe',
                        deadline=pending['deadlineTs'] if pending else None, reason='observer-unity-lost')
            return super()._apply(room, seat, op, {**payload, 'unityLost': False}, **kwargs)
        if room['phase'] == 'battle_countdown' and op == 'reconnect' and payload.get('unityLost') is True:
            self._arm_return(room, 'unity-lost-during-countdown')
        if room['phase'] in ('preparation', 'special'):
            entry = self._entry(room, seat); pending = entry['pending']; barrier = self._views(room)['returnBarrier']
            if (pending and int(self.clock()) >= pending['deadlineTs'] or barrier and barrier['status'] == 'loading' and int(self.clock()) >= barrier['deadlineTs']):
                if op not in ('leave', 'disconnect', 'scene.quit') and not expiry_choice: raise DomainError('preparation_view_loading_expired')
            if op in PREP_OPS:
                # Native identity is still this actor even while viewing another
                # seat. Neither cancel-ready nor valid own instIds bypass it.
                if not expiry_choice and (barrier or not self._own_confirmed(room, seat)): raise DomainError('preparation_own_view_required')
            if op == 'ready' and barrier:
                # The original Ready handler can retransmit after all members
                # have readied and the return-own barrier has begun. It is an
                # acknowledgement of the same readiness, not a second start.
                if (integer(payload.get('readyType'), 'readyType', 0, 1) == 1 and
                        seat.get('ready') and seat in self._alive(room)):
                    return
                raise DomainError('preparation_return_locked')
            if op == 'watch':
                if barrier: raise DomainError('preparation_return_locked')
                if set(payload) - {'seat'}: raise DomainError('invalid_preparation_watch')
                target = member(room, integer(payload.get('seat', seat['seat']), 'seat', 0, 3))
                if target is seat and seat['state']['player']['hp'] <= 0:
                    target = self._observer_target(room, seat)
                self._issue(room, seat, target, 'return-own' if target is seat else 'observe'); return
            if op == 'view.ready': return self._ack(room, seat, payload)
            if op == 'view.refresh':
                if payload or not pending: raise DomainError('preparation_view_no_pending_plan')
                target = member(room, pending['plan']['targetSeat'])
                self._issue(room, seat, target, pending['plan']['kind'], deadline=pending['deadlineTs'], reason='requested-refresh'); return
            if op == 'view.failed':
                committed_failure = pending is None
                if committed_failure:
                    # A retained world can fail after its view.ready ACK. Bind
                    # that report to the exact current committed proof, rather
                    # than treating the absent pending slot as malformed input.
                    committed, proof = entry.get('lastCommitted'), entry.get('proof')
                    if (committed and proof and proof.get('planHash') == committed.get('planHash') and
                            proof.get('attempt') == committed.get('attempt')):
                        pending = committed
                self._validate_failure(room, seat, pending, payload)
                entry['needsOwnRecovery'] = True
                if pending['plan']['observerOnly']:
                    self._exclude_observer(room, seat, pending, 'native-' + payload['reason'])
                else:
                    failures = entry.get('automaticRecoveryFailures', 0) + 1
                    entry['automaticRecoveryFailures'] = failures
                    deadline = (barrier['deadlineTs'] if barrier and barrier['status'] == 'loading' else
                                None if committed_failure else pending['deadlineTs'])
                    if (failures > AUTOMATIC_RECOVERY_LIMIT or
                            deadline is not None and int(self.clock()) >= deadline):
                        self._withdraw(room, seat, 'native-recovery-exhausted')
                        if barrier: self._countdown(room)
                    else:
                        self._issue(room, seat, seat, 'return-own', deadline=deadline,
                                    reason='native-' + payload['reason'])
                return
            if op == 'reconnect' and payload.get('unityLost') is True:
                target = seat if barrier else member(room, pending['plan']['targetSeat'] if pending else entry['committedSeat'])
                if seat['state']['player']['hp'] <= 0:
                    target = self._observer_target(room, seat)
                entry['needsOwnRecovery'] = True; entry['proof'] = None
                self._issue(room, seat, target, 'return-own' if barrier or target is seat else 'observe',
                            deadline=barrier['deadlineTs'] if barrier else pending['deadlineTs'] if pending else None, reason='unity-lost')
        elif room['phase'] == 'battle_countdown' and op in ('watch', 'view.ready', 'view.refresh', 'view.failed'):
            raise DomainError('preparation_view_not_in_rest')
        result = super()._apply(room, seat, op, payload, **kwargs)
        if room['phase'] in ('preparation', 'special'):
            self._initial_views(room)
            self._refresh_pending(room)
            barrier = self._views(room)['returnBarrier']
            if barrier and barrier['status'] == 'loading': self._countdown(room)
        return result

    def _validate_failure(self, room, seat, pending, payload):
        if set(payload) != {'planHash', 'attempt', 'reason'} or not pending or payload['planHash'] != pending['planHash'] or integer(payload['attempt'], 'attempt', 1) != pending['attempt'] or payload['reason'] not in FAILURES:
            raise DomainError('invalid_preparation_view_failure')
        self._validate_plan(room, seat, pending)

    def _exclude_observer(self, room, seat, pending, reason):
        entry = self._entry(room, seat)
        entry['lastFailure'] = {**copy.deepcopy(pending), 'status': 'failed', 'failure': reason}
        entry['pending'] = None; entry['proof'] = None; entry['needsOwnRecovery'] = True
        self._event(room, 'preparation-observer-return-excluded', {'seat': seat['seat'], 'reason': reason}, [seat['seat']])

    def _withdraw(self, room, seat, reason):
        barrier = self._views(room)['returnBarrier']
        if barrier and seat['seat'] not in barrier['withdrawnSeats']: barrier['withdrawnSeats'].append(seat['seat'])
        self._entry(room, seat)['pending'] = None
        self._entry(room, seat)['proof'] = None
        self._entry(room, seat)['needsOwnRecovery'] = True
        from .multiplayer_reconstruction import suspend
        suspend(self, room, seat, reason)
        self._event(room, 'preparation-view-withdrawn', {'seat': seat['seat'], 'reason': reason})

    def _expire_views(self, room):
        if room['phase'] not in ('preparation', 'special', 'battle_countdown', 'battle', 'boss'): return False
        views = self._views(room); barrier = views['returnBarrier']; now = int(self.clock()); changed = False
        if barrier and barrier['status'] == 'loading' and now >= barrier['deadlineTs']:
            for seat in list(self._alive(room)):
                if not seat.get('connected') or not self._own_confirmed(room, seat):
                    self._withdraw(room, seat, 'return-timeout'); changed = True
                    if room['phase'] == 'result': return True
            self._countdown(room, timeout_release=True); return True
        for seat in room['seats']:
            if not seat.get('state') or seat.get('quit') or seat.get('left'): continue
            entry = self._entry(room, seat); pending = entry['pending']
            if not pending or now < pending['deadlineTs']: continue
            if pending['plan']['observerOnly'] and (pending['plan']['kind'] == 'return-own' or entry['needsOwnRecovery'] or room['phase'] in ('battle_countdown', 'battle', 'boss')):
                self._exclude_observer(room, seat, pending, 'observer-return-timeout')
                changed = True; continue
            if room['phase'] in ('battle_countdown', 'battle', 'boss'): continue
            if entry['needsOwnRecovery']:
                self._withdraw(room, seat, 'own-recovery-timeout')
            else:
                entry['needsOwnRecovery'] = True
                self._issue(room, seat, seat, 'return-own', reason='observe-timeout')
            changed = True
        return changed

    def _expire(self, room):
        barrier = (room.get('preparationViews') or {}).get('returnBarrier')
        if room['phase'] == 'preparation' and barrier and barrier['status'] == 'loading':
            self._expire_views(room); return
        # Only the original deterministic special-choice expiry receives this
        # bypass. An HTTP choice never gets physical ownership from a timeout.
        self._server_expiry_depth = getattr(self, '_server_expiry_depth', 0) + 1
        try: super()._expire(room)
        finally: self._server_expiry_depth -= 1
        if room['phase'] in ('preparation', 'special'): self._refresh_pending(room)

    def tick(self):
        super().tick()
        with self.store.lock:
            from .multiplayer_clock import due, schedule
            for row in due(self, views=True):
                self.store.db.execute('BEGIN IMMEDIATE')
                try:
                    room = self._room(row[0])
                    if self._expire_views(room) or self._countdown(room): self._save(room)
                    else: schedule(self, room)
                    self.store.db.commit()
                except DomainError as error:
                    self.store.db.rollback()
                    # Match the inherited clock's per-room fault isolation.
                    # A corrupt loading record cannot stop other rooms.
                    with self.store.db:
                        self.store.db.execute('INSERT OR REPLACE INTO multiplayer_room_faults VALUES(?,?)', (row[0], error.code))
                        try: room = self._room(row[0])
                        except DomainError: continue
                        room['deadlineTs'] = None; room['settleAfterTs'] = None
                        room['fault'] = {'error': error.code, 'detail': error.detail}
                        self._event(room, 'room-fault', room['fault']); self._save(room)
                except Exception:
                    self.store.db.rollback(); raise

    def _view(self, room, actor, *args, **kwargs):
        view = super()._view(room, actor, *args, **kwargs)
        if view.get('left'): return view
        seat = self._seat(room, actor)
        if seat.get('quit') or seat.get('sceneLeft') or seat.get('returned'):
            view.update(preparationView=None, returnOwnBarrier=None,
                        preparationWriteAllowed=False, observerOnly=False)
            return view
        from .multiplayer_reconstruction import waiting
        if waiting(room, seat):
            view.update(selfReconstruction=copy.deepcopy(seat['reconstruction']),
                preparationView=None, returnOwnBarrier=None, preparationWriteAllowed=False,
                observerOnly=False, bossNative=None)
            return view
        observer_only = seat.get('state') and seat['state']['player']['hp'] <= 0
        # Native observation validates this role during ordinary combat too.
        # Do not drop an alive player's explicit false at the phase boundary.
        view['observerOnly'] = bool(observer_only)
        if observer_only and room['phase'] == 'battle':
            from .multiplayer_dead_battle import project as project_dead_battle
            if project_dead_battle(self, room, seat, view): return view
        if room['phase'] in ('battle', 'boss') and not observer_only:
            views = room.get('preparationViews') or {}
            barrier = views.get('returnBarrier') or {}
            proof = (views.get('entries', {}).get(str(seat['seat'])) or {}).get('proof')
            if (barrier.get('status') == 'started' and seat['seat'] in barrier.get('readySeats', []) and
                    proof and proof.get('phase') == 'preparation' and proof.get('seat') == seat['seat'] and
                    proof.get('roomId') == room['roomId'] and proof.get('matchSequence') == room.get('matchSequence', 1) and
                    proof.get('round') == room['round'] and proof.get('runId') == seat['state']['runId'] and
                    'targetRevision' in proof):
                # The last view.ready now returns the running battle directly.
                # Retain its exact accepted input, not the post-PREP_FIN board
                # revision, so the client can commit its already prepared world
                # without another prepare, loader or guessed battle permission.
                view['preparationStartReceipt'] = {'version': VERSION, **copy.deepcopy(proof)}
        # HELP/BOSS already carry the shared defenders' native fight. An
        # eliminated member must receive that shared scene, not BATTLE_WAITING
        # with its boss input discarded. Ordinary self battles stay read-only
        # waiting because the native UI permits observation only in PREPARE.
        rollover = observer_only and room['phase'] in ('battle_countdown', 'battle')
        if room['phase'] not in ('preparation', 'special', 'battle_countdown') and not rollover: return view
        views = room.get('preparationViews')
        if not views or views['round'] != room['round']:
            view.update(preparationViewBootstrapRequired=True, preparationWriteAllowed=False, observerOnly=bool(observer_only))
            return view
        entry = views['entries'].get(str(seat['seat']))
        if not entry: return view
        if seat.get('quit') or seat.get('left'):
            view.update(preparationView=None, returnOwnBarrier=copy.deepcopy(views['returnBarrier']), preparationWriteAllowed=False)
            if seat.get('teamLeft'): view['left'] = True
            return view
        target = presentation_member(room, entry['committedSeat'])
        view['dtoProjection'] = self.target_projection(room, seat, target)
        public = target['state']['player']
        view['observedBoard'] = {k: copy.deepcopy(public[k]) for k in ('charChess', 'equipChess', 'trapChess', 'positions', 'bonds')}
        view['observedBoard'].update(seat=target['seat'], stageId=target['state']['stageId'], nativeSquad=copy.deepcopy(target['state'].get('nativeSquad', [])), specialEffects=copy.deepcopy(target['state']['specialEffects']))
        view['observedSeat'] = target['seat']; view['preparationView'] = None
        pending = entry['pending'] or entry.get('lastFailure') or entry.get('lastCommitted')
        if pending:
            presented = copy.deepcopy(pending)
            selected = presentation_member(room, presented['plan']['targetSeat'])
            presented['targetProjection'] = self.target_projection(room, seat, selected)
            presented['requiredForBattle'] = not presented['plan']['observerOnly']
            presented['targetProjectionSha256'] = digest(presented['targetProjection'])
            presented['targetRevision'] = selected['state']['revision']
            view['preparationView'] = presented
        view['returnOwnBarrier'] = copy.deepcopy(views['returnBarrier'])
        view['observerOnly'] = bool(observer_only)
        view['preparationWriteAllowed'] = room['phase'] == 'preparation' and not views['returnBarrier'] and self._own_confirmed(room, seat) and seat in self._alive(room) and not seat['ready']
        if observer_only and not entry['pending']:
            from .multiplayer_projection import dead_observation
            view['nativeDeadAutoObservation'] = dead_observation(self.protocol, room, seat, view['dtoProjection'])
        if views['returnBarrier'] and views['returnBarrier']['status'] == 'loading':
            for packet in ('allStateSync', 'sceneStatus'):
                payload = view['dtoProjection'][packet]['payload']
                scene = payload['allStateSyncData']['sceneDetail'] if packet == 'allStateSync' else payload['sceneStatus']
                scene['forceEndTs'] = -1
        if rollover:
            # Restore the eliminated actor's base scene asynchronously, but
            # never expose their historical self enemy ledger as a new fight.
            # The native candidate must keep OwnStart false in this role;
            # live battle spectating is a separate original Observe contract.
            view['preparationWriteAllowed'] = False
            view['observerRollover'] = {'version': VERSION, 'readOnly': True, 'mayStartSelfBattle': False,
                'currentRoomRound': room['round'], 'personalRound': seat['state']['round'],
                'presentation': 'BATTLE_WAITING', 'nativeRuntimeValidated': False}
            for projection in [view['dtoProjection'], *((view['preparationView']['targetProjection'],) if view['preparationView'] else ())]:
                for packet in ('allStateSync', 'sceneStatus'):
                    payload = projection[packet]['payload']
                    scene = payload['allStateSyncData']['sceneDetail'] if packet == 'allStateSync' else payload['sceneStatus']
                    scene.update(state=self.c.enum('Torappu.AutoChessGameStateType', 'BATTLE_WAITING'), forceEndTs=-1,
                                 selfBattleInfo=None, helpBattleInfo=None, bossBattleInfo=None)
            if view['preparationView']:
                view['preparationView']['targetProjectionSha256'] = digest(view['preparationView']['targetProjection'])
        return view
