"""Verify the current campaign replay index; print a fresh JSON report to stdout."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CAMPAIGN = [0, 5, 6, 7, 8, 9, 10, 3, 14, 11, 12, 15, 13, 4, 16]
NAMES = ["概念", "高山", "旋转", "远跳", "大脑", "穿梭", "支点", "裂缝", "铁砧", "倒立", "远见", "愚者", "阀门", "深渊", "天梯"]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build" / "Por2D.exe")
    args = parser.parse_args()
    executable = args.exe.resolve()
    routes = []
    for index, source_id in enumerate(CAMPAIGN):
        path = ROOT / "replays" / "difficulty_review" / f"level{index:02}.txt"
        directives = [line.split() for line in path.read_text(encoding="utf-8-sig").splitlines()
                      if line.strip() and not line.lstrip().startswith("#")]
        if directives[0] != ["level", str(source_id)]:
            raise RuntimeError(f"{path.name}: level directive does not match campaign")
        result = subprocess.run([str(executable), "--headless", "--replay", str(path)],
                                capture_output=True, text=True, encoding="utf-8", timeout=30)
        output = result.stdout.strip()
        fields = dict(token.split("=", 1) for token in output.split() if "=" in token)
        expected_next = CAMPAIGN[min(index + 1, len(CAMPAIGN) - 1)]
        if result.returncode or fields.get("replay") != "cleared" or fields.get("level") != str(expected_next):
            raise RuntimeError(f"{path.name}: {output}\n{result.stderr}")
        routes.append({"display_level": index, "name": NAMES[index], "source_id": source_id,
                       "script": path.name, "result": output, "sha256": sha256(path)})
    sources = sorted([*ROOT.glob("src/*.cpp"), *ROOT.glob("include/por2/*.hpp"),
                      ROOT / "build.ps1", ROOT / "CMakeLists.txt"])
    report = {"date": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
              "version": "por2_reconstructed, fixed downward gravity, current campaign order",
              "executable_sha256": sha256(executable),
              "source_sha256": {p.relative_to(ROOT).as_posix(): sha256(p) for p in sources},
              "routes": routes}
    print(json.dumps(report, ensure_ascii=True, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
