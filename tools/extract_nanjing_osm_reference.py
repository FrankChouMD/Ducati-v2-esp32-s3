"""Extract physical reference lines for boundary research, never a ban polygon.

Requires osmium (the research run used 4.3.1). Downloads are supplied separately.
Run with --input jiangsu-261014.osm.pbf --checksum jiangsu-261014.osm.pbf.md5
and --output docs/research/nanjing-osm-reference.geojson.
"""

import argparse
import hashlib
import json
from pathlib import Path

import osmium


# This box selects physical ways for research; it is not a restriction geometry.
BOUNDS = (118.60, 31.90, 118.93, 32.18)


class ReferenceHandler(osmium.SimpleHandler):
    def __init__(self):
        super().__init__()
        self.features = []

    def way(self, way):
        tags = dict(way.tags)
        refs = set(tags.get("ref", "").split(";"))
        road = tags.get("highway") in ("motorway", "trunk") and (
            bool(refs & {"G42", "G36"}) or
            any(name in tags.get("name", "") for name in ("绕城", "沪蓉", "宁洛")))
        water = tags.get("name") in ("长江", "夹江") and (
            tags.get("waterway") in ("riverbank", "river") or
            (tags.get("natural") == "water" and tags.get("water") == "river"))
        if not (road or water):
            return
        try:
            points = [[n.lon, n.lat] for n in way.nodes]
        except osmium.InvalidLocationError:
            return
        west, south, east, north = BOUNDS
        if len(points) < 2 or not any(west <= x <= east and south <= y <= north for x, y in points):
            return
        self.features.append({
            "type": "Feature",
            "properties": {
                "osm_way_id": way.id,
                "osm_version": way.version,
                "osm_timestamp": str(way.timestamp),
                "role": "road_reference" if road else "water_reference",
                "not_a_motorcycle_restriction": True,
                "source_tags": {k: tags[k] for k in
                                ("name", "ref", "highway", "natural", "water", "waterway") if k in tags},
            },
            "geometry": {"type": "LineString", "coordinates": points},
        })


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--checksum", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    expected = args.checksum.read_text(encoding="utf8").split()[0].lower()
    checksum = hashlib.md5()
    with args.input.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    if checksum.hexdigest() != expected:
        raise ValueError("OSM snapshot does not match its published MD5")
    with osmium.io.Reader(str(args.input)) as reader:
        timestamp = reader.header().get("osmosis_replication_timestamp")
    handler = ReferenceHandler()
    handler.apply_file(str(args.input), locations=True)
    data = {
        "type": "FeatureCollection",
        "name": "Nanjing physical road and water reference; not a restriction boundary",
        "attribution": "© OpenStreetMap contributors, ODbL 1.0",
        "license": "https://www.openstreetmap.org/copyright",
        "source": "https://download.geofabrik.de/asia/china/" + args.input.name,
        "source_md5": expected,
        "osm_data_timestamp": timestamp,
        "coordinate_system": "WGS84",
        "purpose": "physical_reference_only",
        "enabled_for_navigation": False,
        "selection_bbox": list(BOUNDS),
        "selection_method": "Select whole ways with at least one vertex inside the box; not clipped",
        "note": "Includes outer-ring roads for comparison. Water lines may be centerlines; not a legal riverbank.",
        "features": handler.features,
    }
    args.output.write_text(json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf8")
    print(json.dumps({"features": len(handler.features), "source_md5": expected,
                      "osm_data_timestamp": timestamp, "output": str(args.output)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
