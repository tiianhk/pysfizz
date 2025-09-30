from . import _sfizz
import os
import sys
from contextlib import contextmanager
from pathlib import Path

@contextmanager
def suppress_stderr():
    stderr_fd = sys.stderr.fileno()
    original_stderr_fd = os.dup(stderr_fd)
    
    devnull = os.open(os.devnull, os.O_WRONLY)
    os.dup2(devnull, stderr_fd)
    os.close(devnull)
    
    try:
        yield
    finally:
        os.dup2(original_stderr_fd, stderr_fd)
        os.close(original_stderr_fd)

class Parser:
    def __init__(self):
        self._parser = _sfizz.Parser()
        self.path = None
        self.playable_keys = []

    def load_sfz_file(self, path, quiet=True):
        path = Path(path)
        if not path.is_file():
            raise FileNotFoundError(f"File not found: {path}")
        if path.suffix.lower() != ".sfz":
            raise ValueError(f"File is not a SFZ file: {path}")
        path = str(path)
        if quiet:
            with suppress_stderr():
                success = self._parser.load_sfz_file(path)
        else:
            success = self._parser.load_sfz_file(path)
        if success and self._parser.get_num_regions() > 0:
            self.path = path
            self.update_playable_keys()
            return True
        else:
            self.path = None
            return False

    def update_playable_keys(self):
        if self.path is None:
            raise ValueError("No SFZ file loaded")
        self.playable_keys = [
            i for i in range(128) 
            if len(self._parser.get_regions_for_note(i)) > 0
        ]
