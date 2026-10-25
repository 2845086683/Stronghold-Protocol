"""Authenticated, display-only CN profile snapshots; no account authority."""
import copy
import re

from .engine import DomainError
from .storage import dump

DEFAULT = {'nickName': '本地博士', 'nickNumber': '0000', 'avatarType': 3, 'avatarId': '0',
    'secretary': 'char_002_amiya', 'secretarySkinId': 'char_002_amiya#1', 'secretarySkinSp': False,
    'nameCardSkinId': '', 'nameCardSkinTmpl': 0, 'level': 120}
LIMITS = {'nickName': 64, 'nickNumber': 32, 'avatarId': 256, 'secretary': 256,
          'secretarySkinId': 256, 'nameCardSkinId': 256, 'displayUid': 128}

def validate(value):
    if not isinstance(value, dict) or set(value) - (set(DEFAULT) | {'displayUid'}):
        raise DomainError('invalid_display_profile')
    out = {}
    for key, item in value.items():
        if key in LIMITS:
            if not isinstance(item, str) or len(item) > LIMITS[key] or any(ord(c) < 32 or ord(c) == 127 for c in item):
                raise DomainError('invalid_display_profile')
            if key in ('nickName', 'nickNumber', 'displayUid') and not item:
                raise DomainError('invalid_display_profile')
            if key in ('nickNumber', 'displayUid') and not re.fullmatch(r'[0-9]+', item):
                raise DomainError('invalid_display_profile')
        elif key == 'secretarySkinSp':
            if type(item) is not bool: raise DomainError('invalid_display_profile')
        else:
            if key == 'avatarType' and isinstance(item, str):
                item = {'NONE': 0, 'ASSISTANT': 1, 'ICON': 2, 'DEFAULT': 3}.get(item)
            maximum = 3 if key == 'avatarType' else 1000 if key == 'level' else 2147483647
            minimum = 1 if key == 'level' else 0
            if type(item) is not int or not minimum <= item <= maximum: raise DomainError('invalid_display_profile')
        out[key] = item
    return out

def capture(rooms, actor, body, seat=None):
    if 'displayProfile' not in body: return False
    value = validate(body['displayProfile']); profile = rooms._profile(actor)
    merged = {**profile.get('displayProfile', {}), **value}
    changed = merged != profile.get('displayProfile')
    profile['displayProfile'] = merged
    if merged.get('nickName'): profile['nickname'] = merged['nickName']
    rooms.store.db.execute('UPDATE multiplayer_participants SET profile=? WHERE id=?', (dump(profile), actor))
    if seat is not None:
        changed = changed or merged != seat.get('displayProfile')
        seat['displayProfile'] = copy.deepcopy(merged); seat['nickname'] = profile['nickname']
    return changed

def display(member):
    return {**DEFAULT, 'nickName': member.get('nickname', DEFAULT['nickName']),
            **member.get('displayProfile', {})}

def settlement_card(card, member):
    value = display(member)
    for source, target in (('nickName','nickname'), ('nickNumber','nicknameNumber'),
            ('avatarType','avatarType'), ('avatarId','avatarId'), ('secretary','secretary'),
            ('secretarySkinId','secretarySkinId'), ('secretarySkinSp','secretarySkinSp'),
            ('nameCardSkinId','nameCardSkinId'), ('nameCardSkinTmpl','nameCardSkinTmpl'), ('level','level')):
        # Settlement declares avatarType as String; TeamStatus/live cards use
        # the enum integer. Keep these original CN contracts distinct.
        card[target] = ('NONE','ASSISTANT','ICON','DEFAULT')[value[source]] if source == 'avatarType' else copy.deepcopy(value[source])

