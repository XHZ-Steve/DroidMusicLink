# LiquidGlass for Qt Quick

Liquid Glass for Qt Quick (Qt 6): the liquidglass production material, the same shader the Compose
library runs and measures against iOS recordings, as a `ShaderEffect`. `GlassSurface` refracts the item
behind it, live, every frame: a ListView scrolling under it, an image, video. Use it from QML in a C++
app or from Python (PySide6). iOS is not a target.

![The PySide6 example rendered offscreen in CI: a light glass card and a dark glass bar over a list](https://raw.githubusercontent.com/shayann07/liquidglass/main/review/atlas/qt-2026-10-20/qt-glass.png)

*Our render: `examples/pyside` under Xvfb on Mesa llvmpipe (OpenGL 4.5, software) in CI. Details and
limits in the [evidence](https://github.com/shayann07/liquidglass/blob/main/review/atlas/qt-2026-10-20/README.md).*

## Use

Put the `LiquidGlass` folder (this directory's `LiquidGlass/`) on the QML import path, then:

```qml
import QtQuick
import LiquidGlass

Window {
    ListView { id: list; anchors.fill: parent; model: 40; delegate: Row { /* ... */ } }

    GlassSurface {
        backdrop: list              // what it refracts; keep the glass outside it
        x: 20; y: parent.height - 100; width: parent.width - 40; height: 72
        radius: 36
        dark: true
        Text { anchors.centerIn: parent; text: "On glass"; color: "white" }
    }
}
```

```python
engine = QQmlApplicationEngine()
engine.addImportPath("path/to/ports/qt")      # the folder that holds LiquidGlass/
engine.load("main.qml")
```

In C++, `engine.addImportPath(...)` the same way.

| `GlassSurface` | Default | |
| :-- | :-- | :-- |
| `backdrop` | none | the item the glass refracts |
| `backdropColor` | white | what the glass sees where the backdrop is transparent (the window's colour, usually) |
| `radius` | 20 | corner radius, logical pixels |
| `dark` | false | the dark material |
| `tintAmount` | 50 | 0 to 100 |
| `light` | `Qt.point(-0.35, -0.94)` | toward the light; `Qt.point(0, -1)` is iOS 27's measured overhead light |
| `tint` | null | `[r, g, b, a]` in sRGB 0..1 replacing the tint |
| `materialize`, `reducedTransparency`, `increasedContrast` | 1, 0, 0 | 0 to 1 |
| `followsMovement` | false | re-map onto the backdrop every frame, for glass that moves inside an animated parent |

Children of `GlassSurface` are drawn on top of the glass and stay ordinary Qt Quick items.

## The shaders must be compiled with qsb

Qt Quick loads shaders as `.qsb` files. The repository's CI compiles them on Linux:

```bash
cd LiquidGlass/shaders
for s in liquidglass.vert liquidglass.frag; do
  pyside6-qsb --glsl "300 es,330" --hlsl 50 --msl 12 -o "$s.qsb" "$s"
done
```

(`qsb` from a C++ Qt install takes the same arguments.) The compiled `.qsb` files for the current
shaders are in CI's `qt-proof` artifact.

## How it works

- **The shader is generated.** `tool/generate_shader.mjs` translates `ports/skia/shaders/material.sksl`,
  the export of the Compose library's material shader, into the Vulkan-style GLSL 440 Qt compiles, and
  writes `GlassEffect.qml` with one QML property per uniform. The translation is mechanical. `--check`
  fails when either drifts.
- **The backdrop is live.** A `ShaderEffectSource` renders the part of `backdrop` under the glass
  (plus two pixels for the outer contour) every frame it changes, and `GlassEffect` runs the material
  over it.
- **One part is approximated: the wide tone kernel.** Thirteen Gaussian taps of the same sigma, as in
  the Flutter port. It sets the material's tone, never its detail.
- **The material's numbers are shared.** `LiquidGlass/material.js` ports the in-app material; a Node
  test checks it against the fixture the Flutter and Unity ports are checked against.

## Checks

```bash
node tool/generate_shader.mjs --check
node --test test/material.test.mjs
python examples/pyside/main.py --grab out    # after compiling the shaders: render, check pixels, save PNGs
```

CI runs all three on Linux (`qt` job): `--grab` renders the example offscreen under Xvfb on Mesa's
software OpenGL. It fails unless both shaders compile, the glass changes the pixels under it without
flattening them, the dark bar darkens the page, the light card stays light, and the glass follows the
list when it scrolls.

Apache-2.0. Not affiliated with Apple or The Qt Company.
