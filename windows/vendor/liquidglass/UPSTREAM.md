# Vendored liquidglass Qt preview

Source: https://github.com/shayann07/liquidglass/tree/7f2814a22c2823ab8bc29fd4aa1808d79829b588/ports/qt
Commit: 7f2814a22c2823ab8bc29fd4aa1808d79829b588 (retrieved 2026-11-11).
License: Apache-2.0; LICENSE and NOTICE preserved beside this file.
This is a recently published Qt preview, not a claim of a mature Qt library.

Runtime includes only LiquidGlass QML/JS and generated GLSL, no Apple artwork or reference captures.
Shaders compile at application build time through Qt ShaderTools (GLSL 300 ES/330, HLSL 50, MSL 12).

Local changes to GlassSurface.qml (DroidMediaLink): explicit sourceRect override, active flag suspending texture updates/frame callbacks, configurable refraction depth multiplier and chromatic dispersion. Geometry/material updates now synchronize immediately, with integral device-pixel capture dimensions shared by the FBO and layer uniforms. Ancestor geometry observers keep the sampled region aligned when layout parents move; there is no idle frame polling. A small set of upstream material uniforms is tuned at the adapter boundary (lower grey lift, less wide scatter, more preserved fine/color detail, directional counter-light). Generated material.js/GLSL stay unchanged. GlassEffect shader URLs use the application resource prefix so both packaged and file-imported diagnostic QML resolve the same compiled shaders.

GlassMaterial.qml is the application adapter. Its optional configuration map exposes radius, tint colour/alpha, refraction depth, chromatic dispersion, blur radius, bevel width, specular/edge/Fresnel gain and inner-shadow gain through the existing upstream uniforms. These controls do not replace the upstream optical shader. Optional interaction response changes light direction and lens depth while the pointer actually hovers over the surface, with bounded smooth enter/exit transitions gated by the owning surface's active flag and reduced-motion preference. No idle optical timer runs; capture resolution stays native. Use a sibling backdrop Item containing only the scene behind the material. Never pass an ancestor containing glass (recursive texture). Each Window owns its own backdrop; QQuick texture nodes cannot be shared between windows. For a transparent floating window, put a local scene in visible:false; ShaderEffectSource still captures it, without exposing square source corners. The normal window can use a visible backdrop. No desktop capture is performed; optical refraction is of the supplied real QML background scene.

The surface uses the upstream rounded signed distance field with refraction, RGB dispersion, bevel lighting, Fresnel and blur taps. It is intended for local dock/popup/floating roles, not a full-window overlay. The disabled style intentionally uses an ordinary rounded opaque surface. No frame animation runs unless followsMovement is explicitly requested; set active to the owning Window.visible. Hidden-source capture is still supported. Reduced-transparency and increased-contrast upstream uniforms remain available for accessibility hosts.


Local shader patch (2026-11-12): liquidglass.frag uses fwidth of the analytic shape distance for contour coverage rather than a fixed 1.5-device-pixel transition. This smooths small circular play buttons and fractional-DPI silhouettes without lowering capture resolution or adding blur. The modified generated shader is intentionally maintained as an application patch; preserve it when regenerating from upstream.
