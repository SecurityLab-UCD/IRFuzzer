from functools import reduce
import re
from typing import Callable, Iterable, Literal, Optional

from lib.triple import Triple


class Target:
    triple: Triple
    cpu: Optional[str]
    attrs: set[str]

    @property
    def arch(self) -> str:
        return self.triple.arch

    def __init__(
        self,
        triple: Triple | str,
        cpu: Optional[str] = None,
        attrs: Iterable[str] | str | None = None,
    ) -> None:
        self.triple = triple if isinstance(triple, Triple) else Triple.parse(triple)
        self.cpu = None if cpu is None or cpu == "" else cpu

        if isinstance(attrs, str):
            attrs = attrs.split(",")

        self.attrs = (
            set(
                (("+" + attr) if not attr.startswith(("+", "-")) else attr)
                for attr in attrs
            )
            if attrs
            else set()
        )

    def __repr__(self) -> str:
        def get_parts() -> Iterable[str]:
            yield str(self.triple)

            if self.cpu:
                yield self.cpu

            for attr in sorted(self.attrs):
                yield attr

        return ",".join(get_parts())

    def __eq__(self, __o: object) -> bool:
        if not isinstance(__o, Target):
            return False

        return (
            self.triple == __o.triple
            and self.cpu == __o.cpu
            and self.attrs == __o.attrs
        )

    def __hash__(self) -> int:
        return hash(str(self))

    @staticmethod
    def parse(s: str) -> "Target":
        """
        Acceptable formats:
        "<triple> [<cpu>] [<attr1> <attr2> ...]",
        "<triple> [<cpu>] [<attr1>,<attr2>,...]", or
        "<triple>[,<cpu>][,<attr1>,<attr2>,...]".
        An attribute must start with '+' or '-' to avoid ambiguity.
        """

        parts = [part for part in re.split(r" |,", s) if part != ""]
        n = len(parts)

        assert n > 0

        # triple only
        if n == 1:
            return Target(triple=parts[0])

        # triple with attributes
        if parts[1].startswith(("+", "-")):
            return Target(
                triple=parts[0],
                cpu=None,
                attrs=parts[1:],
            )

        # triple with cpu
        if n == 2:
            return Target(
                triple=parts[0],
                cpu=parts[1],
            )

        # triple with cpu and attributes
        return Target(triple=parts[0], cpu=parts[1], attrs=parts[2:])


TargetFilter = Callable[[Target], bool]
TargetProp = Literal["triple", "arch-with-sub", "vendor", "os", "abi", "cpu", "attrs"]


def get_target_prop_selector(
    prop: TargetProp,
) -> Callable[[Target], Triple | str | set[str] | None]:
    match prop:
        case "triple":
            return lambda target: target.triple
        case "arch-with-sub":
            return lambda target: target.triple.arch_with_sub
        case "vendor":
            return lambda target: target.triple.vendor
        case "os":
            return lambda target: target.triple.os
        case "abi":
            return lambda target: target.triple.abi
        case "cpu":
            return lambda target: target.cpu
        case "attrs":
            return lambda target: target.attrs


def get_target_prop_equality_checker(
    target: Target, prop: TargetProp
) -> Callable[[Target], bool]:
    prop_selector = get_target_prop_selector(prop)
    return lambda candidate: prop_selector(candidate) == prop_selector(target)


def create_target_filter(
    target: Target, props_to_match: Iterable[TargetProp]
) -> TargetFilter:
    return reduce(
        lambda curr_filter, prop: (
            lambda candidate: (
                curr_filter(candidate)
                and get_target_prop_equality_checker(target, prop)(candidate)
            )
        ),
        sequence=props_to_match,
        initial=lambda _: True,
    )
