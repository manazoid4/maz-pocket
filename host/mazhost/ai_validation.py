from __future__ import annotations

import argparse
import json
import re
import time
from dataclasses import dataclass

from .config import Settings
from .errors import RouteError
from .llm import Models, Route


@dataclass(frozen=True, slots=True)
class SemanticCase:
    prompt: str
    expected: str


CASES = (
    SemanticCase("What is 2 + 2? Reply with only the number.", "4"),
    SemanticCase("What is 10 - 3? Reply with only the number.", "7"),
    SemanticCase("What is the capital of France? Reply with one word.", "paris"),
    SemanticCase("What is the opposite of hot? Reply with one lowercase word.", "cold"),
)


def normalize_answer(value: str) -> str:
    value = value.strip().strip("`*_# .!?,;:")
    value = re.sub(r"[.!?,;:]+$", "", value)
    return " ".join(value.lower().split())


def run_semantic_suite(models: Models, route: Route) -> list[dict]:
    results: list[dict] = []
    for case in CASES:
        started = time.perf_counter()
        try:
            answer, provider = models.chat([{"role": "user", "content": case.prompt}], route)
            normalized = normalize_answer(answer)
            last_route = dict(getattr(models, "_last_route", {}))
            usage = dict(getattr(models, "_last_usage", {}))
            results.append(
                {
                    "route_requested": route,
                    "route_used": last_route.get("active", route),
                    "model": usage.get("model"),
                    "provider": provider,
                    "latency_ms": round((time.perf_counter() - started) * 1000),
                    "expected": case.expected,
                    "normalized": normalized,
                    "pass": normalized == case.expected,
                }
            )
        except RouteError as error:
            results.append(
                {
                    "route_requested": route,
                    "route_used": error.route,
                    "model": error.model,
                    "provider": None,
                    "latency_ms": round((time.perf_counter() - started) * 1000),
                    "expected": case.expected,
                    "normalized": None,
                    "pass": False,
                    "error": error.code.value,
                    "upstream_status": error.upstream_status,
                }
            )
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description="Run deterministic MAZ AI semantic checks")
    parser.add_argument(
        "--route",
        choices=("local", "local_fast", "local_smart", "auto", "cloud", "mazlatest"),
        required=True,
    )
    args = parser.parse_args()
    route: Route = args.route
    results = run_semantic_suite(Models(Settings()), route)
    for result in results:
        print(json.dumps(result, sort_keys=True))
    return 0 if all(result["pass"] for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
