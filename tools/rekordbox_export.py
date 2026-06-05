#!/usr/bin/env python3
"""
Rekordbox master.db exporter for RAIDJ.
Outputs track list and hot cues as JSON to stdout.

Usage:
    python rekordbox_export.py [--db-path PATH] [--content-id ID]
"""

import json
import sys
import argparse
import os


def open_db(db_path):
    from pyrekordbox.db6 import Rekordbox6Database
    # pyrekordbox prints debug `{}` to stdout during init; suppress by
    # temporarily redirecting the C-level stdout file descriptor.
    import io
    devnull_fd = os.open(os.devnull, os.O_WRONLY)
    saved_fd = os.dup(1)
    os.dup2(devnull_fd, 1)
    os.close(devnull_fd)
    try:
        if db_path:
            db = Rekordbox6Database(db_path)
        else:
            db = Rekordbox6Database()
    finally:
        os.dup2(saved_fd, 1)
        os.close(saved_fd)
    return db


def build_cue(c):
    return {
        "id": c.ID,
        "in_msec": c.InMsec,
        "out_msec": c.OutMsec if c.OutMsec else -1,
        "kind": c.Kind,
        "color": c.Color if c.Color else -1,
        "comment": c.Comment or "",
        "is_hot_cue": c.is_hot_cue,
    }


def build_track(t, cues_for_id):
    return {
        "id": t.ID,
        "title": t.Title or "",
        "artist": t.ArtistName or "",
        "album": t.AlbumName or "",
        "genre": t.GenreName or "",
        "bpm": float(t.BPM) / 100.0 if t.BPM else 0.0,
        "key": t.KeyName or "",
        "duration_sec": int(t.Length) if t.Length else 0,
        "file_path": t.FolderPath or "",
        "rating": int(t.Rating) if t.Rating else 0,
        "cues": cues_for_id,
    }


def export_tracks(db_path=None):
    try:
        db = open_db(db_path)
    except Exception as e:
        sys.stdout.write(json.dumps({"error": f"Cannot open database: {e}"}) + "\n")
        sys.exit(1)

    # cue をトラックID別に事前インデックス
    cue_map = {}
    for c in db.get_cue():
        cue_map.setdefault(c.ContentID, []).append(build_cue(c))

    tracks = [
        build_track(t, cue_map.get(t.ID, []))
        for t in db.get_content()
    ]
    sys.stdout.write(json.dumps({"tracks": tracks}, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def export_single(content_id, db_path=None):
    try:
        db = open_db(db_path)
    except Exception as e:
        sys.stdout.write(json.dumps({"error": f"Cannot open database: {e}"}) + "\n")
        sys.exit(1)

    cue_map = {}
    for c in db.get_cue():
        cue_map.setdefault(c.ContentID, []).append(build_cue(c))

    for t in db.get_content():
        if t.ID == content_id:
            sys.stdout.write(
                json.dumps(build_track(t, cue_map.get(t.ID, [])), ensure_ascii=False) + "\n"
            )
            sys.stdout.flush()
            return

    sys.stdout.write(json.dumps({"error": f"ContentID {content_id} not found"}) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--db-path", default=None)
    parser.add_argument("--content-id", type=int, default=None)
    args = parser.parse_args()

    if args.content_id is not None:
        export_single(args.content_id, args.db_path)
    else:
        export_tracks(args.db_path)
