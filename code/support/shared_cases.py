"""T-047 五列 TSV 的零依赖读取器。"""

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Case:
    name: str
    operation: str
    input: str
    expected: str
    expected_error: str


def load(path: str = "cases.tsv") -> list[Case]:
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    header = "name\toperation\tinput\texpected\texpected_error"
    if not lines or lines[0] != header:
        raise ValueError("bad shared-case header")
    cases = []
    for line in lines[1:]:
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 5:
            raise ValueError("bad shared-case row")
        cases.append(Case(*fields))
    return cases


def integers(text: str, separator: str = ",") -> list[int]:
    return [] if not text else [int(value) for value in text.split(separator)]
