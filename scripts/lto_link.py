# Link-time optimisation. Compile flags (-flto) live in platformio.ini; here we
# add -flto to the link line and switch ar/ranlib to the LTO-aware wrappers so
# static libs built by PlatformIO (M5GFX, M5Unified, ...) keep a usable index.
Import("env")
env.Append(LINKFLAGS=["-flto"])
env.Replace(AR=env["AR"].replace("-ar", "-gcc-ar"),
            RANLIB=env["RANLIB"].replace("-ranlib", "-gcc-ranlib"))
