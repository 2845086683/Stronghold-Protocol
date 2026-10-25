# SPDX-License-Identifier: GPL-3.0-or-later
"""Native scene emotes, delivered through the existing authenticated room outbox."""
from .engine import DomainError
from .emoticon_catalog import BATTLE_EMOTES


def apply(rooms, room, seat, name, data):
    if name != 'AutoChessBattleSceneChatReq':
        return False
    if set(data) != {'emojiGroup', 'emojiId'}:
        raise DomainError('invalid_chat_payload')
    theme, emoji = data['emojiGroup'], data['emojiId']
    if (not isinstance(theme, str) or not isinstance(emoji, str)
            or theme not in rooms.c.common['EnabledEmoticonThemeIdList']
            or emoji not in BATTLE_EMOTES.get(theme, ())):
        raise DomainError('invalid_scene_emote')
    if room['phase'] not in ('preparation', 'special', 'battle_countdown', 'battle', 'help', 'boss'):
        raise DomainError('wrong_multiplayer_phase', room['phase'])
    if any(seat.get(key) for key in ('left', 'teamLeft', 'sceneLeft', 'quit')) or not seat.get('state'):
        raise DomainError('multiplayer_seat_quit')
    now = int(rooms.clock() * 1000)
    # Private rate limit; native UI retains its own cooldown. Silently coalesce
    # pathological repeats without turning a cosmetic request into a disconnect.
    previous = seat.get('lastChatAtMs')
    if previous is not None and now - previous < 1000:
        return True
    seat['lastChatAtMs'] = now
    rooms._event(room, 'chat', {'uid': 'private-' + seat['participantId'],
        'emojiGroup': theme, 'emojiId': emoji, 'expiresTs': now // 1000 + 8})
    return True
