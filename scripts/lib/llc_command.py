import re
from typing import Iterable, Optional

from lib.triple import Triple
from lib.target import Target


class LLCCommand:
    target: Target
    global_isel: bool

    def __init__(self, command: str, default_triple: Optional[Triple] = None) -> None:
        assert "llc" in command

        triple = self.__get_triple_from_command(command)

        if triple is None:
            triple = default_triple

        assert triple is not None, f"Cannot determine triple"

        self.target = Target(
            triple=triple,
            cpu=self.__get_cpu_from_command(command),
            attrs=self.__get_attrs_from_command(command),
        )

        self.global_isel = re.match(r".*-global-isel", command) is not None

    @staticmethod
    def __get_triple_from_command(command: str) -> Optional[Triple]:
        if (match := re.match(r".*-mtriple[= ]\"?([a-z0-9_-]+)", command)) is not None:
            return Triple.parse(match.group(1))

        if (match := re.match(r".*-march[= ]\"?([a-z0-9_-]+)", command)) is not None:
            return Triple(arch=match.group(1))

        return None

    @staticmethod
    def __get_cpu_from_command(command: str) -> Optional[str]:
        if (match := re.match(r".*-mcpu[= ]\"?([a-z0-9-]+)", command)) is not None:
            return match.group(1)
        else:
            return None

    @staticmethod
    def __get_attrs_from_command(command: str) -> Iterable[str]:
        return (
            attr
            for arg_val in re.findall(r"-mattr[= ]\"?([A-Za-z0-9,\+-]+)", command)
            for attr in arg_val.split(",")
        )
