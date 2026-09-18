"""Reproduce the original map definitions without modifying the source file."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT.parent / "NewlyUpdateMap.cpp").read_bytes()
try:
    source = source.decode("utf-8-sig")
except UnicodeDecodeError:
    source = source.decode("gb18030")

names = ["Concept", "Experimental 1", "Experimental 2", "Gap", "Abyss",
         "Mountain", "Spin", "Leap", "Brain", "Shuttle", "Fulcrum",
         "Upend", "Foresight", "Valve", "Level 14", "Level 15"]
output = ['// Map coordinates imported verbatim from NewlyUpdateMap.cpp.',
          '// Regenerate with: python tools/import_levels.py',
          '#include "por2/level.hpp"', '', 'namespace por2 {', 'namespace {']
for number in range(16):
    match = re.search(r"void map" + str(number) + r"\(\)\s*\{", source)
    if not match:
        raise ValueError(f"Missing map {number}")
    start = match.end()
    end = start
    depth = 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    body = re.sub(r"//[^\n]*", "", source[start:end - 1])
    body = re.sub(r"\bmpr\((\d),", r"level.map.fill(static_cast<Tile>(\1),", body)
    body = re.sub(r"block\[([^\]]+)\]\[([^\]]+)\]\s*=\s*(\d);",
                  r"level.map.set(\1, \2, static_cast<Tile>(\3));", body)
    for old, new in {"cx": "level.spawn.position.x", "cy": "level.spawn.position.y",
                     "endx": "exit.position.x", "endy": "exit.position.y"}.items():
        body = re.sub(r"\b" + old + r"\b", new, body)
    body = re.sub(r"\bchd\s*=\s*0;", "level.spawn.direction = Direction::Up;", body)
    body = re.sub(r"\bendd\s*=\s*0;", "exit.direction = Direction::Up;", body)
    lines = [line.expandtabs(4).rstrip() for line in body.splitlines() if line.strip()]
    has_exit = "exit.position" in body
    output += [f"Level map{number}() {{", "    Level level;", f"    level.id = {number};",
               f'    level.name = "{names[number]}";']
    if number == 1:
        output += ['    // Original test map omitted its spawn; use a safe explicit default.',
                   '    level.spawn.position = {200, 200};']
    if has_exit:
        output += ['    Body exit;']
    output += lines
    if has_exit:
        output += ['    level.exit = exit;']
    output += ['    return level;', '}', '']
output += ['} // namespace', '', 'Level makeLevel(int id) {', '    switch (id) {']
output += [f'    case {i}: return map{i}();' for i in range(16)]
output += ['    default: throw std::invalid_argument("level id must be 0..15");',
           '    }', '}', '} // namespace por2', '']
(ROOT / 'src' / 'levels.cpp').write_text('\n'.join(output), encoding='utf-8')
print('Imported all 16 maps into src/levels.cpp')
