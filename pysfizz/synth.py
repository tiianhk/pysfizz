from . import _sfizz
import os
import sys
from contextlib import contextmanager
from pathlib import Path
import numpy as np

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

class Synth:
    def __init__(self, sample_rate=48000, block_size=1024):
        self._synth = _sfizz.Synth(sample_rate, block_size)
        self._synth.enable_freewheeling()
        self.path = None
        self.playable_keys = []
        # expose _sfizz.Synth methods
        self.get_sample_rate = self._synth.get_sample_rate
        self.set_sample_rate = self._synth.set_sample_rate
        self.get_block_size = self._synth.get_block_size
        self.set_block_size = self._synth.set_block_size

    def load_sfz_file(self, path, quiet=True):
        path = Path(path)
        if not path.is_file():
            raise FileNotFoundError(f"File not found: {path}")
        if path.suffix.lower() != ".sfz":
            raise ValueError(f"File is not a SFZ file: {path}")
        path = str(path)
        if quiet:
            with suppress_stderr():
                success = self._synth.load_sfz_file(path)
        else:
            success = self._synth.load_sfz_file(path)
        if success and self._synth.get_num_regions() > 0:
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
            if len(self._synth.get_regions_for_note(i)) > 0
        ]

    def render_note(self, pitch, vel, note_on_dur, render_dur):
        nsamples_note_on = int(self.get_sample_rate() * note_on_dur)
        nsamples_render = int(self.get_sample_rate() * render_dur)
        block_size = self.get_block_size()
        nblocks_note_on = nsamples_note_on // block_size
        note_off_delay = nsamples_note_on % block_size
        self._synth.note_on(0, pitch, vel)
        left, right = [], []
        for _ in range(nblocks_note_on):
            left_block, right_block = self._synth.render_block()
            left.append(left_block)
            right.append(right_block)
        self._synth.note_off(note_off_delay, pitch, 0)
        nsamples_current = nblocks_note_on * block_size
        while self._synth.get_num_active_voices() > 0 and nsamples_current < nsamples_render:
            left_block, right_block = self._synth.render_block()
            left.append(left_block)
            right.append(right_block)
            nsamples_current += block_size
        nblocks_silent = (nsamples_render - nsamples_current) // block_size
        for _ in range(nblocks_silent):
            left_block, right_block = self._synth.render_block()
            left.append(left_block)
            right.append(right_block)
        rendered_audio = np.array([
            np.concatenate(left),
            np.concatenate(right)
        ])
        return rendered_audio