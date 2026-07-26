#!/usr/bin/env python3
"""Bounded Sokoban level generator for 9x8 PocketBook maps.

This is intentionally conservative: every loop checks a deadline, and the BFS
solver has a state cap. It is safe to run from Hermes with an outer `timeout`.
"""

from __future__ import annotations

import argparse
import collections
import json
import random
import time

W, H = 9, 8
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


class Deadline(Exception):
    pass


def check_deadline(deadline: float) -> None:
    if time.monotonic() >= deadline:
        raise Deadline


def connected(floor: set[tuple[int, int]]) -> bool:
    start = next(iter(floor))
    queue = [start]
    seen = {start}
    for point in queue:
        for dx, dy in DIRS:
            nxt = (point[0] + dx, point[1] + dy)
            if nxt in floor and nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    return len(seen) == len(floor)


def reach(player: tuple[int, int], boxes: frozenset[tuple[int, int]], floor: set[tuple[int, int]]) -> set[tuple[int, int]]:
    queue = [player]
    seen = {player}
    for point in queue:
        for dx, dy in DIRS:
            nxt = (point[0] + dx, point[1] + dy)
            if nxt in floor and nxt not in boxes and nxt not in seen:
                seen.add(nxt)
                queue.append(nxt)
    return seen


def solve(
    floor: set[tuple[int, int]],
    goals: frozenset[tuple[int, int]],
    boxes: frozenset[tuple[int, int]],
    player: tuple[int, int],
    *,
    cap: int,
    deadline: float,
) -> tuple[int, int] | None:
    def canon(pos: tuple[int, int], current_boxes: frozenset[tuple[int, int]]) -> tuple[int, int]:
        return min(reach(pos, current_boxes, floor))

    start = (boxes, canon(player, boxes))
    queue = collections.deque([(start[0], start[1], 0)])
    seen = {start}

    while queue and len(seen) < cap:
        check_deadline(deadline)
        current_boxes, current_player, pushes = queue.popleft()
        if current_boxes == goals:
            return pushes, len(seen)

        reachable = reach(current_player, current_boxes, floor)
        for box in current_boxes:
            for dx, dy in DIRS:
                behind = (box[0] - dx, box[1] - dy)
                dest = (box[0] + dx, box[1] + dy)
                if behind not in reachable or dest not in floor or dest in current_boxes:
                    continue
                next_boxes = frozenset((current_boxes - {box}) | {dest})
                state = (next_boxes, canon(box, next_boxes))
                if state not in seen:
                    seen.add(state)
                    queue.append((state[0], state[1], pushes + 1))
    return None


def render(
    floor: set[tuple[int, int]],
    goals: frozenset[tuple[int, int]],
    boxes: frozenset[tuple[int, int]],
    player: tuple[int, int],
) -> list[str]:
    rows: list[str] = []
    for y in range(H):
        row = []
        for x in range(W):
            point = (x, y)
            if point not in floor:
                row.append("#")
            elif point == player:
                row.append("@")
            elif point in boxes:
                row.append("$")
            elif point in goals:
                row.append(".")
            else:
                row.append(" ")
        rows.append("".join(row))
    return rows


def generate_one(rng: random.Random, args: argparse.Namespace, deadline: float) -> tuple[int, int, list[str]] | None:
    inner = {(x, y) for y in range(1, H - 1) for x in range(1, W - 1)}
    for _ in range(rng.randint(args.min_walls, args.max_walls)):
        check_deadline(deadline)
        point = rng.choice(tuple(inner))
        next_inner = inner - {point}
        if len(next_inner) >= args.min_floor and connected(next_inner):
            inner = next_inner

    floor = inner
    box_count = rng.choice(args.boxes)
    goal_pool = [
        point for point in floor
        if sum((point[0] + dx, point[1] + dy) in floor for dx, dy in DIRS) >= 2
    ]
    if len(goal_pool) < box_count:
        return None

    goals = frozenset(rng.sample(goal_pool, box_count))
    boxes = set(goals)
    free = list(floor - boxes)
    if not free:
        return None

    player = rng.choice(free)
    for _ in range(rng.randint(args.min_reverse_steps, args.max_reverse_steps)):
        check_deadline(deadline)
        reachable = reach(player, frozenset(boxes), floor)
        options = []
        for box in boxes:
            for dx, dy in DIRS:
                near = (box[0] - dx, box[1] - dy)
                far = (box[0] - 2 * dx, box[1] - 2 * dy)
                if near in reachable and far in floor and far not in boxes:
                    options.append((box, near, far))
        if not options:
            break
        box, near, far = rng.choice(options)
        boxes.remove(box)
        boxes.add(near)
        player = far

    frozen_boxes = frozenset(boxes)
    if frozen_boxes == goals or frozen_boxes & goals:
        return None

    solved = solve(floor, goals, frozen_boxes, player, cap=args.solver_cap, deadline=deadline)
    if not solved:
        return None
    pushes, states = solved
    if pushes < args.min_pushes:
        return None
    return pushes, states, render(floor, goals, frozen_boxes, player)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=25.0)
    parser.add_argument("--tries", type=int, default=3000)
    parser.add_argument("--want", type=int, default=5)
    parser.add_argument("--seed", type=int, default=20260726)
    parser.add_argument("--min-pushes", type=int, default=10)
    parser.add_argument("--solver-cap", type=int, default=60000)
    parser.add_argument("--min-floor", type=int, default=24)
    parser.add_argument("--min-walls", type=int, default=4)
    parser.add_argument("--max-walls", type=int, default=9)
    parser.add_argument("--min-reverse-steps", type=int, default=25)
    parser.add_argument("--max-reverse-steps", type=int, default=60)
    parser.add_argument("--boxes", type=int, nargs="+", default=[2, 3])
    parser.add_argument("--json-out", default="")
    args = parser.parse_args()

    deadline = time.monotonic() + args.seconds
    rng = random.Random(args.seed)
    found: list[tuple[int, int, list[str]]] = []
    seen: set[str] = set()

    try:
        for attempt in range(1, args.tries + 1):
            check_deadline(deadline)
            candidate = generate_one(rng, args, deadline)
            if not candidate:
                continue
            pushes, states, rows = candidate
            key = "\n".join(rows)
            if key in seen:
                continue
            seen.add(key)
            found.append(candidate)
            found.sort(key=lambda item: (item[0], item[1]), reverse=True)
            found = found[: args.want]
            print(f"found {len(found)}/{args.want} at attempt {attempt}: pushes={pushes} states={states}", flush=True)
            if len(found) >= args.want:
                break
    except Deadline:
        print("deadline reached", flush=True)

    print(f"candidates {len(found)}")
    for index, (pushes, states, rows) in enumerate(found, 1):
        print(f"\nlevel {index}: pushes={pushes} states={states}")
        for row in rows:
            print(row)
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as f:
            json.dump(
                [
                    {"pushes": pushes, "states": states, "rows": rows}
                    for pushes, states, rows in found
                ],
                f,
                indent=2,
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
