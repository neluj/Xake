#!/usr/bin/env python3
import argparse
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

EPD_LINE_RE = re.compile(r"^\s*(?P<fen>(?:\S+\s+){5}\S+)\s*(?P<ops>.*)$")
DEPTH_KV_RE = re.compile(r"\bD(?P<depth>\d+)\s+(?P<nodes>\d+)\b")
TOTAL_NODES_RE = re.compile(r"total nodes size:\s*(\d+)", re.IGNORECASE)
UCI_OK_RE = re.compile(r"\buciok\b", re.IGNORECASE)
READY_OK_RE = re.compile(r"\breadyok\b", re.IGNORECASE)

ANSI_GREEN = "\033[32m"
ANSI_RED = "\033[31m"
ANSI_RESET = "\033[0m"

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_EPD_PATH = SCRIPT_DIR / "data" / "perftsuite.epd"


def colorize(text: str, ok: bool, enable: bool) -> str:
    if not enable:
        return text
    return f"{ANSI_GREEN}{text}{ANSI_RESET}" if ok else f"{ANSI_RED}{text}{ANSI_RESET}"


@dataclass
class PerftCase:
    fen: str
    expected: Dict[int, int]
    id: Optional[str] = None


@dataclass
class PerftResult:
    fen: str
    depth: int
    expected: int
    got: Optional[int]
    ok: bool
    elapsed_ms: int
    error: Optional[str] = None


@dataclass
class Summary:
    total_checks: int = 0
    passed: int = 0
    failed: int = 0
    errors: int = 0
    duration_ms: int = 0


def detect_default_engine() -> Optional[Path]:
    repo_root = SCRIPT_DIR.parent
    build_dir = repo_root / "build"
    if not build_dir.exists():
        return None

    candidates = sorted(build_dir.rglob("perft_runner.exe"))
    if not candidates:
        candidates = sorted(build_dir.rglob("perft_runner"))
    if not candidates:
        return None

    return max(candidates, key=lambda path: path.stat().st_mtime)


def parse_epd(epd_path: Path, limit: Optional[int] = None) -> List[PerftCase]:
    cases: List[PerftCase] = []
    with epd_path.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            match = EPD_LINE_RE.match(line)
            if not match:
                continue

            fen = match.group("fen")
            ops = match.group("ops")
            expected: Dict[int, int] = {}
            for depth_match in DEPTH_KV_RE.finditer(ops):
                depth = int(depth_match.group("depth"))
                nodes = int(depth_match.group("nodes"))
                expected[depth] = nodes

            if not expected:
                continue

            cases.append(PerftCase(fen=fen, expected=expected))
            if limit is not None and len(cases) >= limit:
                break

    return cases


class Engine:
    def __init__(self, path: Path, timeout_sec: float = 60.0):
        self.path = path
        self.timeout_sec = timeout_sec
        self.proc: Optional[subprocess.Popen] = None

    def start(self):
        self.proc = subprocess.Popen(
            [str(self.path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True,
            bufsize=1,
        )
        self._writeln("uci")
        self._read_until_regex(UCI_OK_RE, self.timeout_sec)
        self._writeln("isready")
        self._read_until_regex(READY_OK_RE, self.timeout_sec)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self._writeln("quit")
                self.proc.wait(timeout=2)
            except Exception:
                self.proc.kill()
        self.proc = None

    def perft(self, fen: str, depth: int, timeout_sec: Optional[float] = None) -> Tuple[Optional[int], str, int]:
        if not self.proc or self.proc.poll() is not None:
            raise RuntimeError("Engine process is not running. Call start().")

        start = time.time()
        self._writeln(f"position fen {fen}")
        self._writeln(f"go perft {depth}")

        try:
            nodes, raw = self._read_total_nodes(timeout_sec or self.timeout_sec)
        except TimeoutError as exc:
            return None, str(exc), int((time.time() - start) * 1000)

        return nodes, raw, int((time.time() - start) * 1000)

    def _writeln(self, text: str):
        assert self.proc and self.proc.stdin
        self.proc.stdin.write(text + "\n")
        self.proc.stdin.flush()

    def _read_total_nodes(self, timeout_sec: float) -> Tuple[Optional[int], str]:
        assert self.proc and self.proc.stdout
        deadline = time.time() + timeout_sec
        lines: List[str] = []
        nodes: Optional[int] = None

        while time.time() < deadline:
            line = self.proc.stdout.readline()
            if not line:
                time.sleep(0.01)
                continue

            lines.append(line.rstrip("\n"))
            match = TOTAL_NODES_RE.search(line)
            if not match:
                continue

            try:
                nodes = int(match.group(1))
            except ValueError:
                nodes = None
            break

        if nodes is None:
            raise TimeoutError(f"Timed out ({timeout_sec}s) waiting for perft result")

        return nodes, "\n".join(lines)

    def _read_until_regex(self, pattern: re.Pattern, timeout_sec: float):
        assert self.proc and self.proc.stdout
        deadline = time.time() + timeout_sec
        while time.time() < deadline:
            line = self.proc.stdout.readline()
            if not line:
                time.sleep(0.01)
                continue
            if pattern.search(line):
                return
        raise TimeoutError(f"Timeout waiting for pattern: {pattern.pattern}")


def run_suite(engine_path: Path,
              epd_path: Path,
              depths_filter: Optional[List[int]],
              max_positions: Optional[int],
              stop_on_first_mismatch: bool,
              timeout_sec: float,
              on_result: Optional[Callable[[PerftResult], None]] = None) -> Summary:
    cases = parse_epd(epd_path, limit=max_positions)
    summary = Summary()
    started_at = time.time()

    engine = Engine(engine_path, timeout_sec=timeout_sec)
    try:
        engine.start()
        for case in cases:
            depths = sorted(case.expected.keys())
            if depths_filter:
                depths = [depth for depth in depths if depth in depths_filter]
                if not depths:
                    continue

            for depth in depths:
                expected = case.expected[depth]
                nodes, raw_output, elapsed_ms = engine.perft(case.fen, depth, timeout_sec=timeout_sec)
                summary.total_checks += 1

                if nodes is None:
                    result = PerftResult(
                        fen=case.fen,
                        depth=depth,
                        expected=expected,
                        got=None,
                        ok=False,
                        elapsed_ms=elapsed_ms,
                        error="timeout/parse-error",
                    )
                    summary.errors += 1
                    if on_result:
                        on_result(result)
                    if stop_on_first_mismatch:
                        raise RuntimeError("Stopping on first error")
                    continue

                ok = nodes == expected
                if ok:
                    summary.passed += 1
                else:
                    summary.failed += 1

                result = PerftResult(
                    fen=case.fen,
                    depth=depth,
                    expected=expected,
                    got=nodes,
                    ok=ok,
                    elapsed_ms=elapsed_ms,
                    error=None if ok else f"diff={nodes - expected:+d}",
                )
                if on_result:
                    on_result(result)
                if not ok and stop_on_first_mismatch:
                    raise RuntimeError("Stopping on first mismatch")
    finally:
        engine.stop()
        summary.duration_ms = int((time.time() - started_at) * 1000)

    return summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run perft tests from an EPD file against a perft-capable UCI binary."
    )
    parser.add_argument(
        "--engine",
        default=None,
        help="Path to a perft-capable UCI binary. Defaults to the newest detected perft_runner build.",
    )
    parser.add_argument(
        "--epd",
        default=str(DEFAULT_EPD_PATH),
        help="Path to EPD file (default: tests/data/perftsuite.epd)",
    )
    parser.add_argument(
        "--depths",
        default=None,
        help="Comma-separated list of depths to check (e.g., 1,2,3). Default: all depths present per line.",
    )
    parser.add_argument("--max-positions", type=int, default=None, help="Limit number of positions read from EPD.")
    parser.add_argument("--timeout-sec", type=float, default=60.0, help="Timeout per perft (seconds).")
    parser.add_argument(
        "--stop-on-first-mismatch",
        action="store_true",
        help="Stop immediately when a mismatch is found.",
    )
    parser.add_argument(
        "--log-file",
        default=None,
        help="Optional log file path. If omitted, results are printed only to stdout.",
    )
    parser.add_argument("--quiet", action="store_true", help="Only show summary and log path in console.")
    args = parser.parse_args()

    engine_path = Path(args.engine) if args.engine else detect_default_engine()
    epd_path = Path(args.epd)

    if engine_path is None:
        print("ERROR: No perft binary found. Build `perft_runner` or pass --engine explicitly.", file=sys.stderr)
        return 2
    if not engine_path.exists():
        print(f"ERROR: Engine binary not found: {engine_path}", file=sys.stderr)
        return 2
    if not epd_path.exists():
        print(f"ERROR: EPD file not found: {epd_path}", file=sys.stderr)
        return 2

    depths_filter = None
    if args.depths:
        try:
            depths_filter = [int(part.strip()) for part in args.depths.split(",") if part.strip()]
        except ValueError:
            print("ERROR: --depths must be a comma-separated list of integers", file=sys.stderr)
            return 2

    log_path = Path(args.log_file) if args.log_file else None
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)

    enable_color = sys.stdout.isatty()

    def cprint(*items):
        print(*items)
        sys.stdout.flush()

    cprint("Starting perft tests... this might take a while.")

    header_sep = "=" * 80
    log_file = log_path.open("w", encoding="utf-8") if log_path is not None else None
    try:

        def on_result(result: PerftResult):
            status_plain = "OK " if result.ok else "ERR"
            status_col = colorize(status_plain, result.ok, enable_color and not args.quiet)
            got_str = "TIMEOUT" if result.got is None else str(result.got)
            diff = "" if result.got is None else f"{result.got - result.expected:+d}"
            line_plain = (
                f"[{status_plain}] depth D{result.depth}  expected={result.expected}  "
                f"got={got_str}  delta={diff}  ({result.elapsed_ms} ms)"
            )
            line_col = (
                f"[{status_col}] depth D{result.depth}  expected={result.expected}  "
                f"got={got_str}  delta={diff}  ({result.elapsed_ms} ms)"
            )

            if not args.quiet:
                cprint(line_col)
                cprint(f"FEN: {result.fen}")
                cprint("-" * 80)

            if log_file is not None:
                log_file.write(line_plain + "\n")
                log_file.write(f"FEN: {result.fen}\n")
                log_file.write("-" * 80 + "\n")
                log_file.flush()

        summary = run_suite(
            engine_path=engine_path,
            epd_path=epd_path,
            depths_filter=depths_filter,
            max_positions=args.max_positions,
            stop_on_first_mismatch=args.stop_on_first_mismatch,
            timeout_sec=args.timeout_sec,
            on_result=on_result,
        )

        if log_file is not None:
            log_file.write(header_sep + "\n")
            log_file.write(
                f"Checked cases: {summary.total_checks} | Passed: {summary.passed} | "
                f"Failed: {summary.failed} | Errors: {summary.errors}\n"
            )
            log_file.write(f"Total duration: {summary.duration_ms} ms\n")
            log_file.write(header_sep + "\n")
            log_file.flush()
    finally:
        if log_file is not None:
            log_file.close()

    cprint(header_sep)
    cprint(
        f"Checked cases: {summary.total_checks} | Passed: {summary.passed} | "
        f"Failed: {summary.failed} | Errors: {summary.errors}"
    )
    cprint(f"Total duration: {summary.duration_ms} ms")
    cprint(header_sep)
    if log_path is not None:
        cprint(f"Log saved to: {log_path}")

    return 0 if summary.failed == 0 and summary.errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
