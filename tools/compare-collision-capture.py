"""Compare two S02-T03d collision captures (campaign server and normal client).

Usage: python3 tools/compare-collision-capture.py <server.csv> <client.csv> [pose_tolerance]

Row kinds are documented in xrGame/CampaignCollisionCapture.h. Rules:

* Objects are matched by ID. IDs present on one side only are reported with
  their sections, not failed: scripts spawn a varying set of objects while the
  level loads, so their IDs differ from run to run. Exactly one actor must be
  common to both sides.
* Structure must be identical for every common object: each skeleton has the
  same section, visual, kind, alive flag, bone count,
  visible-bone mask, element count and character flag; each element has the
  same bone, shape type and local shape parameters (1e-5); each shell and shell
  element has the same bone, element and geometry counts; character foot radius,
  box and mass, and shell and element masses match within 1e-3.
* Hanging lamps hide their light bone when they are off, and flicker at random
  (CHangingLamp::TurnOff, UpdateCL), so for `lights_hanging_lamp` the visible
  mask and element count are reported as lamp state, not asserted.
* Pose-dependent values are asserted only for the actor, which stands still on
  both sides: its trunk elements (pelvis, spine, neck, head) within the pose
  tolerance (default 0.05 m), its `top` and `ground` picks (same hit, range
  within the pose tolerance) and its position within 0.1 m (the client's
  character controller settles the capsule in real time). The actor's limbs
  and its `front` pick are reported only: on the client the actor is the view
  entity, so its arms follow the HUD weapon and its torso the camera. Stalkers
  and monsters run their AI in real time on the client, so their pose rows are
  reported (agreement counts and largest differences) but not asserted.

Prints a JSON summary and exits 1 on any asserted mismatch.
"""

import csv
import json
import sys
from collections import defaultdict

STRUCT_TOL = 1e-3
PARAM_TOL = 1e-5
LAMP_SECTION = "lights_hanging_lamp"
TRUNK_BONES = {"bip01_pelvis", "bip01_spine", "bip01_spine1", "bip01_spine2", "bip01_neck", "bip01_head"}
ACTOR_POSITION_TOL = 0.1
ASSERTED_ACTOR_RAYS = {"top", "ground"}


def load(path):
    rows = defaultdict(list)
    with open(path, newline="") as handle:
        for row in csv.reader(handle):
            if row:
                rows[row[0]].append(row[1:])
    return rows


def keyed(rows, key_fields):
    out = {}
    for row in rows:
        key = tuple(row[i] for i in key_fields)
        if key in out:
            raise SystemExit(f"duplicate key {key}")
        out[key] = row
    return out


def fdiff(a, b):
    return abs(float(a) - float(b))


class Report:
    def __init__(self, pose_tol):
        self.pose_tol = pose_tol
        self.failures = []
        self.summary = {"pose_tolerance": pose_tol, "structure_tolerance": STRUCT_TOL}

    def fail(self, text):
        if len(self.failures) < 50:
            self.failures.append(text)
        else:
            self.summary["more_failures"] = True

    def same_sets(self, name, a, b, section_index=None):
        """Keys present on both sides. Keys on one side only are reported, not
        failed: scripts spawn a varying set of objects while the level loads
        (smoke-52 had 1,090 objects, smoke-53 1,095), so their IDs differ
        between runs. The actor must be on both sides (checked by the caller)."""
        missing = sorted(set(a) - set(b))
        extra = sorted(set(b) - set(a))
        common = sorted(set(a) & set(b))
        self.summary[f"{name}_common"] = len(common)
        if missing or extra:
            def describe(rows, keys):
                if section_index is None:
                    return [list(k) for k in keys[:20]]
                return [[*k, rows[k][section_index]] for k in keys[:20]]
            self.summary[f"{name}_only_server"] = {"count": len(missing), "first": describe(a, missing)}
            self.summary[f"{name}_only_client"] = {"count": len(extra), "first": describe(b, extra)}
        return common


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    pose_tol = float(sys.argv[3]) if len(sys.argv) == 4 else 0.05
    server, client = load(sys.argv[1]), load(sys.argv[2])
    r = Report(pose_tol)
    r.summary["server_meta"] = server["meta"][0] if server["meta"] else None
    r.summary["client_meta"] = client["meta"][0] if client["meta"] else None

    # skeleton,<id>,section,visual,kind,alive,bones,mask,elements,character,foot,bx,by,bz,mass,px,py,pz
    s_skel, c_skel = keyed(server["skeleton"], [0]), keyed(client["skeleton"], [0])
    ids = r.same_sets("skeletons", s_skel, c_skel, section_index=1)
    actor_ids = []
    kinds = defaultdict(int)
    npc_position_max = 0.0
    lamp_state_differs = []
    for key in ids:
        a, b = s_skel[key], c_skel[key]
        kinds[a[3]] += 1
        structure = [1, 2, 3, 4, 5, 6, 7, 8]
        if a[1] == LAMP_SECTION:
            structure = [1, 2, 3, 4, 5, 8]
            if a[6:8] != b[6:8]:
                lamp_state_differs.append([key[0], a[6], a[7], b[6], b[7]])
        if [a[i] for i in structure] != [b[i] for i in structure]:
            r.fail(f"skeleton {key[0]} structure differs: {a[1:9]} vs {b[1:9]}")
        for i in (9, 10, 11, 12, 13):
            if fdiff(a[i], b[i]) > STRUCT_TOL:
                r.fail(f"skeleton {key[0]} field {i} differs: {a[i]} vs {b[i]}")
        pos = max(fdiff(a[i], b[i]) for i in (14, 15, 16))
        if a[3] == "actor":
            actor_ids.append(key[0])
            r.summary["actor_position_diff"] = pos
            r.summary["actor_position_tolerance"] = ACTOR_POSITION_TOL
            if pos > ACTOR_POSITION_TOL:
                r.fail(f"actor {key[0]} position differs by {pos}")
        else:
            npc_position_max = max(npc_position_max, pos)
    r.summary["skeleton_kinds"] = dict(kinds)
    r.summary["lamp_state_differs"] = lamp_state_differs
    r.summary["npc_position_max_diff"] = npc_position_max
    if len(actor_ids) != 1:
        r.fail(f"expected one actor skeleton, found {actor_ids}")
    actor = actor_ids[0] if actor_ids else None

    # element,<id>,<bone>,name,type,p0,p1,p2,lx,ly,lz
    s_el, c_el = keyed(server["element"], [0, 1]), keyed(client["element"], [0, 1])
    keys = r.same_sets("elements", s_el, c_el)
    trunk_max = limb_max = npc_centre_max = 0.0
    limb_worst = None
    for key in keys:
        a, b = s_el[key], c_el[key]
        if a[2:4] != b[2:4] or any(fdiff(a[i], b[i]) > PARAM_TOL for i in (4, 5, 6)):
            r.fail(f"element {key} shape differs: {a[2:7]} vs {b[2:7]}")
        centre = max(fdiff(a[i], b[i]) for i in (7, 8, 9))
        if key[0] != actor:
            npc_centre_max = max(npc_centre_max, centre)
        elif a[2] in TRUNK_BONES:
            trunk_max = max(trunk_max, centre)
        elif centre > limb_max:
            limb_max, limb_worst = centre, a[2]
    r.summary["actor_trunk_centre_max_diff"] = trunk_max
    r.summary["actor_limb_centre_max_diff"] = {"diff": limb_max, "bone": limb_worst}
    r.summary["npc_element_centre_max_diff"] = npc_centre_max
    if trunk_max > pose_tol:
        r.fail(f"actor trunk element centres differ by {trunk_max}")

    # self,<id>,hit,bone,name,range  and  ray,<id>,<name>,hit,target,hit_id,bone,bone_name,range
    for kind, key_fields, pose_index in (("self", [0], 4), ("ray", [0, 1], 7)):
        s_rows, c_rows = keyed(server[kind], key_fields), keyed(client[kind], key_fields)
        keys = r.same_sets(f"{kind}_rows", s_rows, c_rows)
        agree = 0
        npc_range_max = 0.0
        for key in keys:
            a, b = s_rows[key], c_rows[key]
            # self: hit, bone, bone name. ray: hit, target, bone, bone name
            # (the hit object's ID is compared through the target class).
            if kind == "self":
                identity, other = a[1:4], b[1:4]
            else:
                identity, other = [a[2], a[3], a[5], a[6]], [b[2], b[3], b[5], b[6]]
            rng = fdiff(a[pose_index], b[pose_index])
            same = identity == other and rng <= pose_tol
            if key[0] == actor:
                name = key[1] if kind == "ray" else "top"
                asserted = kind == "self" or name in ASSERTED_ACTOR_RAYS
                r.summary[f"actor_{kind}_{name}"] = {
                    "server": a, "client": b, "range_diff": rng, "asserted": asserted}
                if asserted and not same:
                    r.fail(f"actor {kind} {key} differs: {a} vs {b}")
            else:
                agree += same
                npc_range_max = max(npc_range_max, rng)
        npc_total = len(keys) - sum(1 for k in keys if k[0] == actor)
        r.summary[f"npc_{kind}_agree"] = f"{agree}/{npc_total}"
        r.summary[f"npc_{kind}_range_max_diff"] = npc_range_max

    # shell,<id>,section,visual,elements,geoms,mass,active,enabled,px,py,pz
    s_sh, c_sh = keyed(server["shell"], [0]), keyed(client["shell"], [0])
    keys = r.same_sets("shells", s_sh, c_sh, section_index=1)
    shell_pos_max = 0.0
    state_differs = 0
    for key in keys:
        a, b = s_sh[key], c_sh[key]
        if a[1:5] != b[1:5]:
            r.fail(f"shell {key[0]} structure differs: {a[1:5]} vs {b[1:5]}")
        if fdiff(a[5], b[5]) > STRUCT_TOL:
            r.fail(f"shell {key[0]} mass differs: {a[5]} vs {b[5]}")
        state_differs += a[6:8] != b[6:8]
        shell_pos_max = max(shell_pos_max, max(fdiff(a[i], b[i]) for i in (8, 9, 10)))
    r.summary["shell_state_differs"] = state_differs
    r.summary["shell_position_max_diff"] = shell_pos_max

    # shell_element,<id>,<index>,bone,name,geoms,mass
    s_se, c_se = keyed(server["shell_element"], [0, 1]), keyed(client["shell_element"], [0, 1])
    keys = r.same_sets("shell_elements", s_se, c_se)
    for key in keys:
        a, b = s_se[key], c_se[key]
        if a[2:5] != b[2:5] or fdiff(a[5], b[5]) > STRUCT_TOL:
            r.fail(f"shell element {key} differs: {a[2:]} vs {b[2:]}")

    r.summary["failures"] = r.failures
    r.summary["result"] = "match" if not r.failures else "mismatch"
    print(json.dumps(r.summary, indent=1))
    raise SystemExit(0 if not r.failures else 1)


if __name__ == "__main__":
    main()
