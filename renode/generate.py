#!/usr/bin/env python3

from pathlib import Path

PLACEHOLDER = "<<<<<WORKSPACE>>>>>"

def main():
    # current path
    script_dir = Path(__file__).resolve().parent

    workspace = str(script_dir.parent)

    # find *.initials
    for initial_file in script_dir.rglob("*.initial"):
        output_file = initial_file.with_suffix("")

        content = initial_file.read_text(encoding="utf-8")
        content = content.replace(PLACEHOLDER, workspace)

        output_file.write_text(content, encoding="utf-8")

        print(f"[OK] {initial_file} -> {output_file}")

if __name__ == "__main__":
    main()
