"""Validated replacement of one participant's private CN preparation roster.

The existing activity configuration validator owns operator/equipment/skill
rules. This bridge only batches those same operations into one atomic receipt.
Neither an official account object nor another participant is involved.
"""
from .engine import DomainError


def replace_squad(activity, payload):
    if not isinstance(payload, dict) or set(payload) != {'chessSquad'}:
        raise DomainError('invalid_multiplayer_squad_snapshot')
    squad = payload['chessSquad']
    if not isinstance(squad, dict) or len(squad) > len(activity.c.season['CharShopChessDatas']):
        raise DomainError('invalid_multiplayer_squad_snapshot')
    proxy = {'playerActivity': {'chessSquad': {}}}
    normal, diy = {}, {}
    for key, value in squad.items():
        definition = activity.c.season['CharShopChessDatas'].get(key)
        if not definition or not isinstance(value, dict):
            raise DomainError('invalid_chess_id')
        if value.get('chessId') != key:
            raise DomainError('invalid_multiplayer_squad_snapshot')
        item = {name: value.get(name) for name in ('skillIndex', 'currentEquip', 'cultivateEffect')}
        if value.get('type') == 3:
            item.update(diyChar=value.get('charId'), origChessId=value.get('diyBackupChessId'))
            diy[key] = item
        else:
            if value.get('type') != 0 or value.get('charId') != (definition.get('CharId') or definition.get('BackupCharId')):
                raise DomainError('invalid_multiplayer_squad_snapshot')
            # A non-DIY fallback slot has the catalog's BackupCharId. Seed that
            # established value for the same existing deploy validator.
            proxy['playerActivity']['chessSquad'][key] = {'charId': value['charId']}
            normal[key] = item
    activity.apply_configuration(proxy, 'setChessPoolDeploy', {'chessPool': normal})
    activity.apply_configuration(proxy, 'setChessPoolDiyChar', {'diyChessPool': diy})
    return proxy['playerActivity']['chessSquad']
