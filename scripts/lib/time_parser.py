SECONDS_PER_UNIT: dict[str, int] = {
    "s": 1,
    "m": 60,
    "h": 3600,
    "d": 86400,
    "w": 604800,
}


def get_time_in_seconds(s: str) -> int:
    return int(s[:-1]) * SECONDS_PER_UNIT[s[-1]]
