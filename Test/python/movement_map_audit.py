"""Compare server/client movement maps and report portal geometry issues.

Usage: python3 Test/python/movement_map_audit.py --client-root /path/to/client
Exit 1 means data mismatch; geometry warnings require level-design review.
"""
import argparse
import json
import math
from pathlib import Path


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def audit(server_dir, client_root):
    errors, warnings = [], []
    maps = {d["mapId"]: d for d in map(read, sorted(server_dir.glob("*.json")))}
    if not maps:
        return [f"No maps found: {server_dir}"], warnings
    for map_id, server in maps.items():
        path = client_root / "LL2_Client_Win/Data/Maps" / f"{map_id}.json"
        if not path.exists():
            errors.append(f"Missing client map: {map_id}")
            continue
        client = read(path)
        if client.get("mapId") != map_id or client.get("physics") != server["physics"]:
            errors.append(f"{map_id}: physics/mapId mismatch")
        client_portals = {p["id"]: p for p in client.get("portals", [])}
        if len(client_portals) != len(client.get("portals", [])):
            errors.append(f"{map_id}: duplicate client portal ID")
        if set(client_portals) != {p["id"] for p in server["portals"]}:
            errors.append(f"{map_id}: portal ID mismatch")
        for portal in server["portals"]:
            label = f'{map_id}/{portal["id"]}'
            other = client_portals.get(portal["id"], {})
            for key in ("position", "interactionRange", "destinationMapId", "spawnPosition"):
                if portal[key] != other.get(key):
                    errors.append(f"{label}: {key} mismatch")
            # Client allows portals only while Grounded. Compare distance from
            # the nearest player origin on each platform (foot offset = 10).
            point = portal["position"]
            distances = [math.hypot(point["x"] - min(max(point["x"], f["left"]), f["right"]),
                                    point["y"] - (f["y"] - 10))
                         for f in server["physics"]["platforms"]]
            if not distances or min(distances) > portal["interactionRange"]:
                warnings.append(f"{label}: no grounded position within interactionRange")
            destination = maps.get(portal["destinationMapId"])
            if destination is None:
                errors.append(f"{label}: missing destination map")
                continue
            spawn = portal["spawnPosition"]
            if not any(f["left"] <= spawn["x"] <= f["right"] and spawn["y"] + 10 <= f["y"] + .05
                       for f in destination["physics"]["platforms"]):
                warnings.append(f"{label}: spawn {spawn} has no landing platform; server uses safeFeet")
        print(f"CHECKED map {map_id}")
    return errors, warnings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-root", type=Path, required=True)
    parser.add_argument("--server-maps", type=Path,
                        default=Path(__file__).resolve().parents[2] / "SERVER/src/CHANNEL/data/Maps")
    args = parser.parse_args()
    errors, warnings = audit(args.server_maps, args.client_root)
    for item in errors:
        print("ERROR:", item)
    for item in warnings:
        print("REVIEW:", item)
    print(f"Map comparison: {len(errors)} errors, {len(warnings)} geometry warnings")
    return bool(errors)


if __name__ == "__main__":
    raise SystemExit(main())
