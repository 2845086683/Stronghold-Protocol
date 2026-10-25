"""Keep saved loadout intent distinct from native default-fallback semantics.

The CN converter uses an empty currentEquip as 'shop default' and cannot consume
a null DIY bond list. Selected values are frozen once; only wire_squad encodes a
saved 'no module' as the CN original-equipment ID. No troop maximum replaces a
selected skill, module, cultivation rune, operator or affiliation.
"""
import copy
from .operator_affiliations import OPERATOR_POWERS


def diy_bonds(catalog, char_id):
    powers = set(OPERATOR_POWERS.get(char_id, ()))
    result = [key for key, row in catalog.common['BondInfoDict'].items()
              if row.get('IsPower') and powers.intersection(row.get('PowerIdList') or ())
              and key in catalog.season['BondInfoDict']]
    return result or [catalog.season['ConstData']['FallbackBondId']]


def base_bonds(catalog, state, chess_id):
    """One source for regular/elite DIY bonds, native runes, bans and economy."""
    normal = catalog.season['ChessNormalIdLookupDict'].get(chess_id, chess_id)
    pick = state.get('diyChessPool', {}).get(normal)
    if pick:
        return list(pick.get('bondList') or diy_bonds(catalog, pick['charId']))
    snapshot = next((row for row in state.get('nativeSquad', ()) if row['chessId'] == normal), None)
    if snapshot and normal in catalog.season['DiyChessDict']:
        return list(snapshot.get('bondList') or diy_bonds(catalog, snapshot['charId']))
    return list(catalog.chess[chess_id].get('BondIds') or ())


def snapshot_profile(catalog, configured):
    """Return domain DIY choices and the user's unencoded native squad snapshot."""
    diy, squad = {}, []
    for key, value in sorted(configured.items()):
        is_diy = value.get('type') == 3
        equip = value.get('currentEquip', catalog.season['CharShopChessDatas'][key]['DefaultUniEquipId'])
        bonds = diy_bonds(catalog, value['charId']) if is_diy else list(catalog.chess[key].get('BondIds') or ())
        cultivate = value.get('cultivateEffect')
        if cultivate is None:
            cultivate = catalog.cultivate_effect(value['charId'])
        if is_diy:
            diy[key] = {'charId': value['charId'], 'skillIndex': value['skillIndex'],
                        'currentEquip': equip, 'skinId': value.get('skin', ''),
                        'cultivateEffect': cultivate, 'bondList': list(bonds),
                        'diyBackupChessId': value.get('diyBackupChessId')}
        squad.append({'chessId': key, 'charId': value['charId'], 'cultivateEffectId': cultivate,
                      'currentEquip': equip, 'skinId': value.get('skin', ''),
                      'type': value['type'], 'potentialRank': value['potentialRank'],
                      'skillIndex': value['skillIndex'], 'bondList': bonds})
    return diy, squad


def wire_squad(catalog, state):
    """Project a copy; leave explicit None and all selected fields in state intact."""
    if 'nativeSquad' in state:
        result = copy.deepcopy(state['nativeSquad'])
    else:
        result = []
        for key in state['chessPool']:
            config = catalog.season['CharShopChessDatas'][key]
            pick = state.get('diyChessPool', {}).get(key, {})
            char_id = pick.get('charId') or config['CharId'] or config['BackupCharId']
            result.append({'chessId': key, 'charId': char_id,
                           'cultivateEffectId': pick.get('cultivateEffect') or catalog.cultivate_effect(char_id),
                           'currentEquip': pick.get('currentEquip') if pick else config['DefaultUniEquipId'],
                           'skinId': pick.get('skinId', ''), 'type': config['ChessType'],
                           'potentialRank': config['BackupCharPotRank'],
                           'skillIndex': pick.get('skillIndex', config['DefaultSkillIndex']),
                           'bondList': base_bonds(catalog, state, key)})
    for row in result:
        if 'currentEquip' not in row:
            row['currentEquip'] = catalog.season['CharShopChessDatas'][row['chessId']]['DefaultUniEquipId']
        elif row['currentEquip'] is None:
            original = next((equip for equip in catalog.equipment_ids(row['charId'])
                             if catalog.equipment_is_original(equip)), None)
            if original is not None:
                row['currentEquip'] = original
        if not row.get('bondList'):
            row['bondList'] = base_bonds(catalog, state, row['chessId'])
    # This is the native battle configuration registry, not the recruitment
    # pool. Native _GatherSquadsViaPlayer builds ordinary/elite configurations
    # only for these slots, before LoadingReady can grant an excluded/hidden
    # operator. Keep the saved rows (and their order) first; fill absent valid
    # shop definitions without adding cards to chessPool or the finite pool.
    registered = {row['chessId'] for row in result}
    characters = catalog.data['characterDefinitions']
    for key, config in sorted(catalog.season['CharShopChessDatas'].items()):
        if key in registered or key not in catalog.chess or config['ChessType'] not in (0, 1, 2):
            continue
        is_diy = config['ChessType'] == 1
        pick = state.get('diyChessPool', {}).get(key) if is_diy else None
        # Unselected DIY slots have neither a native default nor a backup
        # character. A real saved selection is required; never invent one.
        char_id = pick.get('charId') if pick else config.get('BackupCharId')
        if not char_id or char_id not in characters:
            continue
        if is_diy and not pick:
            continue
        result.append({'chessId': key, 'charId': char_id,
                       'cultivateEffectId': (pick.get('cultivateEffect') or catalog.cultivate_effect(char_id)) if pick else catalog.cultivate_effect(char_id),
                       'currentEquip': pick.get('currentEquip') if pick else config['BackupCharUniEquipId'],
                       'skinId': pick.get('skinId', '') if pick else '',
                       # CharShop ChessType PRESET=2 differs from the wire
                       # AutoChessCharType PRESET=4 / BACK_UP=1 / DIY=3.
                       'type': 3 if is_diy else 4 if config['ChessType'] == 2 else 1,
                       'potentialRank': config['BackupCharPotRank'],
                       'skillIndex': pick.get('skillIndex', config['BackupCharSkillIndex']) if pick else config['BackupCharSkillIndex'],
                       'bondList': base_bonds(catalog, state, key)})
        # Retain the existing explicit-None module wire rule for saved DIY
        # entries missing from an older registry, just as for saved rows above.
        if pick and result[-1]['currentEquip'] is None:
            original = next((equip for equip in catalog.equipment_ids(char_id)
                             if catalog.equipment_is_original(equip)), None)
            if original is not None:
                result[-1]['currentEquip'] = original
    return result
