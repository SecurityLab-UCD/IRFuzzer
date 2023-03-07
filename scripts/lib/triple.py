from ctypes import CDLL, c_char_p, cdll
from typing import ClassVar, Optional
from lib import LLVM

from lib.arch import ARCH_TO_BACKEND_MAP, normalize_arch


LIB_LLVM_TARGET_PATH = LLVM + "/build-release/lib/libLLVMTarget.so"

class Triple:
    llvm_lib: ClassVar[Optional[CDLL]] = None

    arch: str
    vendor: Optional[str]
    os: Optional[str]
    abi: Optional[str]

    @property
    def backend(self) -> str:
        return ARCH_TO_BACKEND_MAP[self.arch]

    def __init__(
        self,
        arch: str,
        vendor: Optional[str] = None,
        os: Optional[str] = None,
        abi: Optional[str] = None,
    ) -> None:
        assert len(arch) > 0
        self.arch = normalize_arch(arch)
        self.vendor = self.normalize_component(vendor)
        self.os = self.normalize_component(os)
        self.abi = self.normalize_component(abi)

    def __eq__(self, __o: object) -> bool:
        if not isinstance(__o, Triple):
            return False

        return (
            self.arch == __o.arch
            and self.vendor == __o.vendor
            and self.os == __o.os
            and self.abi == __o.abi
        )

    def __hash__(self) -> int:
        return hash(str(self))

    def __repr__(self) -> str:
        s = "-".join(
            (component if component else "")
            for component in [self.arch, self.vendor, self.os, self.abi]
        )

        return s.rstrip("-")

    @classmethod
    def normalize_component(cls, s: Optional[str]) -> Optional[str]:
        return None if s in [None, "", "none", "unknown"] else s

    @classmethod
    def normalize(cls, s: str) -> str:
        if cls.llvm_lib is None:
            cls.llvm_lib = cdll.LoadLibrary(LIB_LLVM_TARGET_PATH)
            cls.llvm_lib.LLVMNormalizeTargetTriple.restype = c_char_p

        c_arg = c_char_p(s.encode("ascii"))
        c_ret = cls.llvm_lib.LLVMNormalizeTargetTriple(c_arg)
        return c_ret.decode("ascii")

    @classmethod
    def parse(cls, s: str) -> "Triple":
        assert len(s) > 0

        parts = cls.normalize(s).split("-")
        n = len(parts)

        assert n > 0 and n <= 4

        return Triple(
            arch=parts[0],
            vendor=parts[1] if n >= 2 else None,
            os=parts[2] if n >= 3 else None,
            abi=parts[3] if n == 4 else None,
        )
