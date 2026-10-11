"""Read retained benchmark data in plain or losslessly compressed form."""
from contextlib import contextmanager
import csv
import gzip
import json
from pathlib import Path


def resolve_path(path):
    path = Path(path)
    if path.is_file():
        return path
    compressed = path.with_name(path.name + ".gz")
    if compressed.is_file():
        return compressed
    raise FileNotFoundError(path)


@contextmanager
def open_text(path, newline=None):
    path = resolve_path(path)
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8", newline=newline) as stream:
        yield stream


def read_csv(path):
    with open_text(path, newline="") as stream:
        return list(csv.DictReader(stream))


def read_json(path):
    with open_text(path) as stream:
        return json.load(stream)
