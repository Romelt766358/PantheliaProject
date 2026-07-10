import inspect
import json
import os
import unreal

out = []
names = [name for name in dir(unreal.AnimationLibrary) if "notify" in name.lower() or "track" in name.lower()]
for name in sorted(names):
    attr = getattr(unreal.AnimationLibrary, name)
    row = {"name": name}
    print(name)
    try:
        row["signature"] = str(inspect.signature(attr))
        print("  " + row["signature"])
    except Exception as exc:
        row["signature_error"] = str(exc)
        print("  signature unavailable: " + str(exc))
    try:
        row["doc"] = str(attr.__doc__)
        print("  doc: " + row["doc"])
    except Exception:
        pass
    out.append(row)

with open(os.path.join(unreal.Paths.project_saved_dir(), "CodexAnimationLibraryIntrospection.json"), "w", encoding="utf-8") as f:
    json.dump(out, f, indent=2)
