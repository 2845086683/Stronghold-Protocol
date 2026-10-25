"""Private native hand layout; the original Unity map remains the executor.

Pinned MapAreaManager collectors enumerate increasing rows/decreasing columns.
All eleven archived season maps use the canonical leftNormal rectangle with
ten valid hand cells (encoded 0..-9), then five temporary cells (-10..-14).
Char/equipment/magic share that one namespace. Evidence is recorded separately
under research/native-client; this module neither changes GridPosition nor
implements a replacement client deployment/drag system.
"""
from .engine import DomainError,integer

VALID_HAND_COUNT=10
HAND_COUNT=15


def enabled(state):
    return bool(state.get('nativeSessionVersion')) or 'handPositions' in state['player']


def inventory(catalog,state):
    """Owned, unequipped and undeployed instances in native fallback order."""
    p=state['player'];deployed={row['instId'] for row in p['positions']}
    equipped={ident for char in p['charChess'] for ident in char.get('equipInstIds',[])}
    result={}
    for field in ('charChess','equipChess','trapChess'):
        for obj in p[field]:
            ident=obj['instId']
            if ident in deployed or ident in equipped:continue
            kind=0 if field=='charChess' else 1 if field=='equipChess' else catalog.chess[catalog.data['identifiers'][str(obj['chessIdentifier'])]]['ItemType']
            result[ident]=kind
    return result


def reconcile(catalog,state,explicit=None,token_positions=None):
    """Keep valid slots stable; only refill holes from temporary hand cells.

    Missing metadata on an old save is derived on a copy by NativeSession.GET.
    Deleted/deployed/equipped entries disappear; newly returned/gained inventory
    takes empty cells. Inventory exceeding the original fifteen visible cells
    is retained without inventing out-of-range native coordinates.
    """
    if not enabled(state):return []
    p=state['player'];available=inventory(catalog,state);old=p.get('handPositions',[])
    if not isinstance(old,list) or len(old)>128:raise DomainError('invalid_saved_hand_positions')
    if explicit is not None and (not isinstance(explicit,list) or len(explicit)>128):raise DomainError('invalid_hand_positions')
    # Tokens share real hand cells even though their instId belongs to a
    # character and they never enter owned inventory or handPositions.
    from .native_tokens import hand_tokens
    tokens=hand_tokens(catalog,state,token_positions)
    chosen={};occupied={row['position'] for row in tokens}
    def add(row,strict):
        if not isinstance(row,dict):raise DomainError('invalid_hand_positions')
        ident=integer(row.get('instId'),'handInstanceId',1)
        if ident not in available:
            if strict:raise DomainError('instance_not_in_hand',str(ident))
            return
        pos=integer(row.get('position'),'handPosition',1-HAND_COUNT,0)
        kind=integer(row.get('chessType'),'chessType',0,3)
        direction=integer(row.get('direction',1),'direction',0,3)
        if kind!=available[ident]:raise DomainError('hand_chess_type_mismatch',str(ident))
        if ident in chosen or pos in occupied:
            if strict:raise DomainError('position_collision')
            return
        chosen[ident]={'instId':ident,'position':pos,'chessType':kind,'direction':direction};occupied.add(pos)
    if explicit is not None:
        for row in explicit:add(row,True)
    for row in old:
        if isinstance(row,dict) and row.get('instId') in chosen:continue
        add(row,False)
    # Original OptimizeHandMap moves temporary pieces into free valid cells;
    # it never compacts or sorts already-valid occupied cells.
    for row in sorted([*chosen.values(),*tokens],key=lambda value:-value['position']):
        if row['position']> -VALID_HAND_COUNT:continue
        free=next((-index for index in range(VALID_HAND_COUNT) if -index not in occupied),None)
        if free is None:break
        occupied.remove(row['position']);row['position']=free;occupied.add(free)
    for ident,kind in available.items():
        if ident in chosen:continue
        free=next((-index for index in range(HAND_COUNT) if -index not in occupied),None)
        if free is None:continue
        chosen[ident]={'instId':ident,'position':free,'chessType':kind,'direction':1};occupied.add(free)
    p['handPositions']=sorted(chosen.values(),key=lambda value:-value['position'])
    return p['handPositions']


def require_purchase_space(catalog,state):
    if not enabled(state):return
    rows=reconcile(catalog,state)
    from .native_tokens import reserved_hand_positions
    reserved=reserved_hand_positions(catalog,state)
    if len(rows)+len(reserved)>=VALID_HAND_COUNT:
        raise DomainError('hand_full',{'validCapacity':VALID_HAND_COUNT,'sharedBy':['characters','equipment','magic']})


def finish_preparation(catalog,state):
    """Discard temporary/unmapped inventory after the final preparation effects.

    This is destruction, never a sale: no coins, sold-event dispatch, or fake
    native ReqSellOrDestroy. Destroy overflowing characters first, return their
    equipment through normal slot reconciliation, then discard remaining excess
    items. Equipment worn by retained characters is never hand inventory.
    """
    if not enabled(state):return []
    rows=reconcile(catalog,state);safe={row['instId'] for row in rows if row['position']> -VALID_HAND_COUNT}
    p=state['player'];deployed={row['instId'] for row in p['positions']}
    doomed=[obj['instId'] for obj in p['charChess'] if obj['instId'] not in safe|deployed]
    if doomed:
        p['charChess']=[obj for obj in p['charChess'] if obj['instId'] not in doomed]
    # Removing a character makes its equipment eligible for hand slots. Preserve
    # any returned item that can enter a valid gap; never delete an equipped item
    # merely because its ID is absent from handPositions.
    rows=reconcile(catalog,state);safe={row['instId'] for row in rows if row['position']> -VALID_HAND_COUNT}
    excess={ident for ident in inventory(catalog,state) if ident not in safe}
    if excess:
        for field in ('charChess','equipChess','trapChess'):
            p[field]=[obj for obj in p[field] if obj['instId'] not in excess]
        doomed.extend(sorted(excess))
    reconcile(catalog,state)
    return doomed
