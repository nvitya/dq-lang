from pathlib import Path

def print_tree(directory: Path, indent: str = ""):
    for path in directory.iterdir():
        if path.name == ".dqbuild":
            continue
        if path.is_dir():
            print(f"{indent}{path.name}/")
            if len(indent) <= 8:
                print_tree(path, indent + "  ")
        else:
            print(f"{indent}{path.name}")

current = Path(".")

print("Files in current directory:")
for path in current.iterdir():
    if path.is_file():
        print(f"  {path.name}")

print("Directories in parent directory:")
for path in Path("..").iterdir():
    if path.is_dir():
        print(f"  {path.name}")

#print("Tree of parent directory:")
#print_tree(Path(".."), "  ")