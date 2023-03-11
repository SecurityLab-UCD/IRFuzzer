from pathlib import Path
import re
from typing import Iterable, NamedTuple, Optional

from lib import LLC
from lib.triple import Triple
from lib.target import Target


class LLCCommand(NamedTuple):
    target: Target
    global_isel: bool

    def get_options(self, output: Optional[str | Path] = None) -> Iterable[str]:
        yield f"-mtriple={self.target.triple}"

        if self.target.cpu:
            yield f"-mcpu={self.target.cpu}"

        if len(self.target.attrs) > 0:
            yield f"-mattr={','.join(self.target.attrs)}"

        if self.global_isel:
            yield "-global-isel"

        if output:
            yield f"-o"
            yield str(output)

    def get_args(
        self, input: str | Path, output: Optional[str | Path] = None
    ) -> list[str]:
        return [
            str(LLC),
            *self.get_options(output),
            str(input),
        ]

    @classmethod
    def parse(
        cls, command: str, default_triple: Optional[Triple] = None
    ) -> "LLCCommand":
        assert "llc" in command

        triple = cls.__get_triple_from_command(command)

        if triple is None:
            triple = default_triple

        assert triple is not None, f"Cannot determine triple"

        return LLCCommand(
            target=Target(
                triple=triple,
                cpu=cls.__get_cpu_from_command(command),
                attrs=cls.__get_attrs_from_command(command),
            ),
            global_isel=re.match(r".*-global-isel", command) is not None,
        )

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
