"""Keep absolute audit paths in artifacts; redact user roots in published JSON."""
import re

_USER_ROOT = re.compile(
    r"(?:[A-Z]:[\\/]+(?:Users|Documents and Settings)[\\/]+[^\\/]+|/(?:home|Users)/[^/]+)(?=[\\/]|$)",
    re.IGNORECASE)


def sanitize_publication(value):
    if isinstance(value, str):
        return _USER_ROOT.sub("<runner>", value)
    if isinstance(value, list):
        return [sanitize_publication(item) for item in value]
    if isinstance(value, dict):
        result = {}
        for key, item in value.items():
            normalized = sanitize_publication(key)
            cleaned = sanitize_publication(item)
            if normalized in result and result[normalized] != cleaned:
                raise ValueError("Runner path normalization would hide conflicting metadata")
            result[normalized] = cleaned
        return result
    return value
