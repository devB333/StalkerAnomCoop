"""Compare two S02-T04b falling-body captures (campaign server and normal client).

Usage: python3 tools/compare-physics-capture.py <server.csv> <client.csv>

Row kinds are documented in xrGame/CampaignPhysicsCapture.h. Rules:

* Both sides must have run the fixture on the same object: ID, section, visual,
  element and geometry counts, root bone and its material are identical; mass
  and box size within 1e-3; the native fixed step, gravity and drop height are
  identical.
* The rest position from the save and the lifted start position agree within
  0.01 m (H06's initial-position tolerance); the floor under the body is the
  same material (its triangle index is reported).
* Steps are matched by index. Over the common free-fall steps (before either
  side's first contact) positions agree within 0.01 m and velocities within
  0.01 m/s. From the first contact to the last common step positions agree
  within the post-contact tolerance (0.05 m): the contact response starts from
  slightly different rest positions, and the differences are reported.
* The first-contact steps agree within 2 steps; the settle steps are reported.
  Both sides settled (end row), neither body was destroyed, the final positions
  agree within 0.05 m, and both end rows carry ok=1.

Prints a JSON summary and exits 1 on any asserted mismatch.
"""

import csv
import json
import math
import sys

STRUCT_TOL = 1e-3
START_TOL = 0.01
FALL_POSITION_TOL = 0.01
FALL_VELOCITY_TOL = 0.01
CONTACT_POSITION_TOL = 0.05
CONTACT_STEP_TOL = 2
FINAL_POSITION_TOL = 0.05

META_FIELDS = ["frame", "time_ms", "level", "fixed_step", "gravity", "world_steps", "id", "section", "visual",
               "mass", "elements", "geoms", "box_x", "box_y", "box_z", "bone", "bone_name", "bone_material",
               "drop_height", "nearest_id", "nearest_section", "nearest_distance"]
START_FIELDS = ["px", "py", "pz", "enabled", "floor_hit", "floor_tri", "floor_material", "floor_range",
                "lx", "ly", "lz"]
STEP_FIELDS = ["n", "world_step", "px", "py", "pz", "vx", "vy", "vz", "wx", "wy", "wz", "enabled", "contacts"]
END_FIELDS = ["steps", "frames", "sim_ms", "wall_ms", "first_contact", "settle", "px", "py", "pz", "enabled",
              "floor_hit", "floor_tri", "floor_range", "destroyed", "ok"]


def load(path):
    capture = {"steps": {}, "contacts": []}
    with open(path, newline="") as handle:
        for row in csv.reader(handle):
            if not row:
                continue
            kind, values = row[0], row[1:]
            if kind == "meta":
                capture["meta"] = dict(zip(META_FIELDS, values, strict=True))
            elif kind == "start":
                capture["start"] = dict(zip(START_FIELDS, values, strict=True))
            elif kind == "step":
                step = dict(zip(STEP_FIELDS, values, strict=True))
                capture["steps"][int(step["n"])] = step
            elif kind == "contact":
                capture["contacts"].append(values)
            elif kind == "end":
                capture["end"] = dict(zip(END_FIELDS, values, strict=True))
    for key in ("meta", "start", "end"):
        if key not in capture:
            raise SystemExit(f"{path}: no {key} row")
    return capture


def vec(row, prefix):
    return tuple(float(row[prefix + axis]) for axis in "xyz")


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b, strict=True)))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    server, client = load(sys.argv[1]), load(sys.argv[2])
    failures = []
    summary = {"server": sys.argv[1], "client": sys.argv[2]}

    sm, cm = server["meta"], client["meta"]
    for field in ("id", "section", "visual", "elements", "geoms", "bone", "bone_name", "bone_material",
                  "fixed_step", "gravity", "drop_height", "level"):
        if sm[field] != cm[field]:
            failures.append(f"meta {field}: server {sm[field]} client {cm[field]}")
    for field in ("mass", "box_x", "box_y", "box_z"):
        if abs(float(sm[field]) - float(cm[field])) > STRUCT_TOL:
            failures.append(f"meta {field}: server {sm[field]} client {cm[field]}")
    summary["object"] = {"id": sm["id"], "section": sm["section"], "visual": sm["visual"], "mass": sm["mass"],
                         "box": [sm["box_x"], sm["box_y"], sm["box_z"]], "bone_material": sm["bone_material"],
                         "fixed_step": sm["fixed_step"], "gravity": sm["gravity"], "drop_height": sm["drop_height"],
                         "nearest": {"server": [sm["nearest_id"], sm["nearest_section"], sm["nearest_distance"]],
                                     "client": [cm["nearest_id"], cm["nearest_section"], cm["nearest_distance"]]}}

    ss, cs = server["start"], client["start"]
    rest = dist(vec(ss, "p"), vec(cs, "p"))
    lifted = dist(vec(ss, "l"), vec(cs, "l"))
    summary["start"] = {"rest_distance": rest, "lifted_distance": lifted,
                        "floor_material": [ss["floor_material"], cs["floor_material"]],
                        "floor_tri": [ss["floor_tri"], cs["floor_tri"]],
                        "floor_range": [ss["floor_range"], cs["floor_range"]],
                        "enabled_before": [ss["enabled"], cs["enabled"]]}
    if rest > START_TOL:
        failures.append(f"rest position differs by {rest:.4f} m")
    if lifted > START_TOL:
        failures.append(f"lifted position differs by {lifted:.4f} m")
    if ss["floor_material"] != cs["floor_material"]:
        failures.append(f"floor material: server {ss['floor_material']} client {cs['floor_material']}")

    se, ce = server["end"], client["end"]
    s_contact, c_contact = int(se["first_contact"]), int(ce["first_contact"])
    common = sorted(set(server["steps"]) & set(client["steps"]))
    fall_end = min(s_contact if s_contact >= 0 else 10 ** 9, c_contact if c_contact >= 0 else 10 ** 9)
    fall_pos = fall_vel = contact_pos = 0.0
    fall_pos_step = contact_pos_step = None
    for n in common:
        a, b = server["steps"][n], client["steps"][n]
        p = dist(vec(a, "p"), vec(b, "p"))
        if n < fall_end:
            v = dist(vec(a, "v"), vec(b, "v"))
            if p > fall_pos:
                fall_pos, fall_pos_step = p, n
            fall_vel = max(fall_vel, v)
        elif p > contact_pos:
            contact_pos, contact_pos_step = p, n
    summary["steps"] = {"server": len(server["steps"]), "client": len(client["steps"]), "common": len(common),
                        "free_fall_common": sum(1 for n in common if n < fall_end),
                        "free_fall_max_position": fall_pos, "free_fall_max_position_step": fall_pos_step,
                        "free_fall_max_velocity": fall_vel,
                        "after_contact_max_position": contact_pos, "after_contact_max_position_step": contact_pos_step}
    if fall_pos > FALL_POSITION_TOL:
        failures.append(f"free-fall position differs by {fall_pos:.4f} m at step {fall_pos_step}")
    if fall_vel > FALL_VELOCITY_TOL:
        failures.append(f"free-fall velocity differs by {fall_vel:.4f} m/s")
    if contact_pos > CONTACT_POSITION_TOL:
        failures.append(f"post-contact position differs by {contact_pos:.4f} m at step {contact_pos_step}")

    summary["contact"] = {"first_contact_step": [s_contact, c_contact],
                          "materials": [server["contacts"][:1], client["contacts"][:1]],
                          "settle_step": [se["settle"], ce["settle"]]}
    if s_contact < 0 or c_contact < 0:
        failures.append(f"first contact missing: server {s_contact} client {c_contact}")
    elif abs(s_contact - c_contact) > CONTACT_STEP_TOL:
        failures.append(f"first contact steps differ: server {s_contact} client {c_contact}")
    if server["contacts"][:1] != client["contacts"][:1]:
        failures.append("first contact materials differ")

    final = dist(vec(se, "p"), vec(ce, "p"))
    summary["end"] = {"final_distance": final, "ok": [se["ok"], ce["ok"]],
                      "destroyed": [se["destroyed"], ce["destroyed"]],
                      "enabled": [se["enabled"], ce["enabled"]], "floor_range": [se["floor_range"], ce["floor_range"]],
                      "steps": [se["steps"], ce["steps"]], "frames": [se["frames"], ce["frames"]],
                      "sim_ms": [se["sim_ms"], ce["sim_ms"]], "wall_ms": [se["wall_ms"], ce["wall_ms"]]}
    if final > FINAL_POSITION_TOL:
        failures.append(f"final position differs by {final:.4f} m")
    for name, end in (("server", se), ("client", ce)):
        if end["ok"] != "1":
            failures.append(f"{name} fixture did not pass (ok={end['ok']})")
        if end["destroyed"] != "0":
            failures.append(f"{name} body was destroyed")
        if int(end["settle"]) < 0:
            failures.append(f"{name} body did not settle")

    summary["failures"] = failures
    summary["result"] = "match" if not failures else "mismatch"
    print(json.dumps(summary, indent=2))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
