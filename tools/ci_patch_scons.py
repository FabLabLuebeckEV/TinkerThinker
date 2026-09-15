#!/usr/bin/env python3
import os
import sys

def patch_link_common_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        old_target = '    import SCons.Tool.cxx\n    import SCons.Tool.FortranCommon'
        new_target = '''    try:
        from .. import cxx
    except Exception:
        try:
            import SCons.Tool.cxx as cxx
        except Exception:
            cxx = None
    try:
        from .. import FortranCommon
    except Exception:
        try:
            import SCons.Tool.FortranCommon as FortranCommon
        except Exception:
            FortranCommon = None'''
        if old_target in content:
            content = content.replace(old_target, new_target)
            content = content.replace('has_cplusplus = SCons.Tool.cxx.iscplusplus(source)', 'has_cplusplus = cxx.iscplusplus(source) if cxx else False')
            content = content.replace('has_fortran = SCons.Tool.FortranCommon.isfortran(env, source)', 'has_fortran = FortranCommon.isfortran(env, source) if FortranCommon else False')
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"Patched SCons linkCommon: {filepath}")
            return True
    except Exception as e:
        print(f"Error patching {filepath}: {e}")
    return False

def patch_platformio():
    try:
        import platformio
        pio_dir = os.path.dirname(platformio.__file__)
        print(f"Found PlatformIO at: {pio_dir}")

        # 1. Patch remove_unnecessary_core_packages in core.py
        core_file = os.path.join(pio_dir, "package", "manager", "core.py")
        if os.path.exists(core_file):
            with open(core_file, "r", encoding="utf-8") as f:
                content = f.read()
            if "def remove_unnecessary_core_packages" in content:
                lines = content.splitlines()
                new_lines = []
                in_func = False
                for line in lines:
                    if line.startswith("def remove_unnecessary_core_packages"):
                        in_func = True
                        new_lines.append(line)
                        new_lines.append("    return []")
                    elif in_func:
                        if line.startswith("def ") or (line and not line.startswith(" ") and not line.startswith("#")):
                            in_func = False
                            new_lines.append(line)
                    else:
                        new_lines.append(line)
                with open(core_file, "w", encoding="utf-8") as f:
                    f.write("\n".join(new_lines) + "\n")
                print(f"Patched {core_file}")

        # 2. Patch _run.py
        run_file = os.path.join(pio_dir, "platform", "_run.py")
        if os.path.exists(run_file):
            with open(run_file, "r", encoding="utf-8") as f:
                content = f.read()
            hook_code = """        scons_dir = get_core_package_dir("tool-scons")
        try:
            for _r, _d, _files in os.walk(scons_dir):
                if "linkCommon" in _r and "__init__.py" in _files:
                    _p = os.path.join(_r, "__init__.py")
                    with open(_p, "r", encoding="utf-8") as _f:
                        _c = _f.read()
                    if "import SCons.Tool.FortranCommon" in _c:
                        _c = _c.replace("    import SCons.Tool.cxx\\n    import SCons.Tool.FortranCommon", "    try:\\n        from .. import cxx\\n    except Exception:\\n        try:\\n            import SCons.Tool.cxx as cxx\\n        except Exception:\\n            cxx = None\\n    try:\\n        from .. import FortranCommon\\n    except Exception:\\n        try:\\n            import SCons.Tool.FortranCommon as FortranCommon\\n        except Exception:\\n            FortranCommon = None")
                        _c = _c.replace("has_cplusplus = SCons.Tool.cxx.iscplusplus(source)", "has_cplusplus = cxx.iscplusplus(source) if cxx else False")
                        _c = _c.replace("has_fortran = SCons.Tool.FortranCommon.isfortran(env, source)", "has_fortran = FortranCommon.isfortran(env, source) if FortranCommon else False")
                        with open(_p, "w", encoding="utf-8") as _f:
                            _f.write(_c)
        except Exception:
            pass"""
            target_str = '        scons_dir = get_core_package_dir("tool-scons")'
            if target_str in content and "linkCommon" not in content:
                content = content.replace(target_str, hook_code, 1)
                with open(run_file, "w", encoding="utf-8") as f:
                    f.write(content)
                print(f"Patched {run_file}")
    except ImportError:
        print("PlatformIO not installed in current Python environment.")
    except Exception as e:
        print(f"Error patching platformio: {e}")

def patch_installed_scons():
    pio_home = os.path.expanduser("~/.platformio")
    if not os.path.isdir(pio_home):
        return
    for root, dirs, files in os.walk(pio_home):
        if "linkCommon" in root and "__init__.py" in files:
            p = os.path.join(root, "__init__.py")
            patch_link_common_file(p)

if __name__ == "__main__":
    patch_platformio()
    patch_installed_scons()
