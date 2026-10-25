# Generated from original CN serialized prefab facts. Game data retains its original rights.
"""Private preparation layout marker display supplement; no battle spawn changes."""
SOURCE_ARTIFACT = 'research/native-client/token-prepare-marker-policy/classification.json'
SOURCE_ARTIFACT_SHA256 = '3013429ff2ee7f5c6567a662f77a8521b3e4c3a2d7440d60d4f0bbf84aaf8e8a'
SOURCE_BUNDLE_SHA256 = {'btl_pfb_tokens_0.ab': '0a825931c0073fbf9888454802cbc3f5e068428a48e27b7ad48e0708097b4235', 'btl_pfb_tokens_1.ab': 'de695db4345ad18ab578087145b594bbed3fa4d134bc48f54e2afe874aebab3c', 'btl_pfb_tokens_2.ab': '4812e4965e4df8cb09d9fbfb0378eb844d2fd25a9c1bfa083c27f881c1f46703', 'btl_pfb_tokens_3.ab': 'e119f533c43f037a4b11f290f5f94ca5444de1fd6382d5dd6ff33baf95159530', 'btl_pfb_tokens_4.ab': 'a007714067cb60dc4c2effd74838b7782f95b7656ab4e9bf640bfb5160586ed3', 'btl_pfb_tokens_5.ab': '7311b797f40ce8391afbad5f28342848e6fa511cd27e8bbf2aa162a6370e0bfd', 'btl_pfb_tokens_6.ab': '2083fe270b9eea20c561ef1d222f31fdf95c08e439ae2fd29a5a3155f6502381'}
MARKER_TOKEN_PREFABS = {'token_10071_aglna2_agairp': ('token_10071_aglna2_agairp',)}

def suppress_hand_token(catalog, token_key, prefab_key):
    """Suppress preparation layout markers at any position; never battle entities.

    The historical API name is retained for callers. Only supplement an absent
    activity entry, using the exact pinned token key and resolved prefab key.
    """
    if token_key in catalog.common['ShopStateTokenDict']:
        return False
    return prefab_key in MARKER_TOKEN_PREFABS.get(token_key, ())
