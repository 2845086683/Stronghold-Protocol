"""Strict local repair for one previously lossy activity JSON export.

The original catalog hash remains the save-compatibility identity.  These string
values are read from its own pinned CN TextAsset, never inferred from text or a
foreign implementation.  The backend package separately pins the sidecar bytes.
"""
import hashlib
import json

LEGACY_CATALOG = '83b73cc5d2b01a829d3d4255379fee83dac344c608266509d983be665591eb9f'
CN_SOURCE = 'research/asset-inspection/CN/streaming/51e39b68a0792abc82111d2527cacad9/activity_table858cbc.bytes'
CN_SOURCE_SHA256 = '56385ac33c0f5c11c00b6a86573053ea9049fd87d24f6d16e1a945b33ab349f6'
REPAIR_FILE = 'cn-autochess-blackboard-repair.json'
PATCH_COUNT = 1125


def apply_repair(path, catalog_hash, data):
    if path.name != 'cn-autochess.json':
        return None
    sidecar = path.with_name(REPAIR_FILE)
    if catalog_hash != LEGACY_CATALOG:
        if sidecar.exists():
            raise ValueError('Blackboard sidecar/catalog binding mismatch')
        return None
    raw = sidecar.read_bytes()  # Missing exact-build repair is an explicit error.
    repair = json.loads(raw)
    sources = {key.replace('\\', '/'): value for key, value in data.get('sourceHashes', {}).items()}
    if (repair.get('schemaVersion') != 1 or repair.get('activityId') != 'act2autochess'
            or repair.get('region') != 'CN' or repair.get('catalogSha256') != catalog_hash
            or repair.get('sourceRaw') != CN_SOURCE or repair.get('sourceRawSha256') != CN_SOURCE_SHA256
            or sources.get(CN_SOURCE) != CN_SOURCE_SHA256):
        raise ValueError('Blackboard sidecar source binding mismatch')
    patches = repair.get('patches')
    if not isinstance(patches, list) or len(patches) != PATCH_COUNT:
        raise ValueError('Blackboard sidecar patch count mismatch')
    targets = []
    seen = set()
    for patch in patches:
        if not isinstance(patch, dict) or set(patch) != {'path', 'old', 'value'}:
            raise ValueError('Invalid Blackboard patch')
        parts = patch['path']
        if not isinstance(parts, list) or not isinstance(patch['value'], str) or type(patch['old']) not in (int, float) or patch['old'] != 0:
            raise ValueError('Invalid Blackboard patch value')
        if (len(parts) == 5 and parts[:2] == ['season', 'GarrisonDataDict']
                and all(isinstance(parts[i], str) for i in (2, 4)) and parts[3] == 'Blackboard'):
            target = data['season']['GarrisonDataDict'].get(parts[2], {}).get('Blackboard')
        elif (len(parts) == 6 and parts[:2] == ['season', 'EffectBuffInfoDataDict']
                and isinstance(parts[2], str) and type(parts[3]) is int and parts[3] >= 0
                and parts[4] == 'Blackboard' and isinstance(parts[5], str)):
            rows = data['season']['EffectBuffInfoDataDict'].get(parts[2], [])
            target = rows[parts[3]].get('Blackboard') if parts[3] < len(rows) else None
        else:
            raise ValueError('Blackboard patch outside allowlisted fields')
        identity = tuple(parts)
        if identity in seen or not isinstance(target, dict) or parts[-1] not in target or type(target[parts[-1]]) not in (int, float) or target[parts[-1]] != patch['old']:
            raise ValueError('Blackboard patch duplicate or old value mismatch')
        seen.add(identity)
        targets.append((target, parts[-1], patch['value']))
    # Validate every path first; a malformed sidecar never leaves a half-repair.
    for target, key, value in targets:
        target[key] = value
    return {'version': 1, 'sha256': hashlib.sha256(raw).hexdigest(), 'patchCount': len(targets),
            'catalogHashPreserved': True, 'sourceRawSha256': CN_SOURCE_SHA256}
