# Morphicons circle/check transition

The discovery playlist return button uses the Lucide `log-out` outline shown
in the Morphicons playground. Its original 24-unit geometry is drawn by the
existing Qt icon provider. Source: https://github.com/lucide-icons/lucide/blob/c55aab36b553a7de50a0050e7042fd7b2d64664e/icons/log-out.svg . ISC and Feather MIT notices are preserved in `LUCIDE-LICENSE`.

The next/check Shape requests Qt's CurveRenderer, which provides analytic
antialiasing on the native GPU backend without adding per-row MSAA layers.

`NextMorphFrames.js` additionally maps the two strokes of the next-song glyph
to the same check using the identical MIT core and 33-frame sampling. Its QML
renderer animates to the check after a successful queue command and back again.

The point correspondence and intermediate shapes in
`src/qml/components/CheckMorphFrames.js` were generated with the pure core of
[morphicons 2.0.2](https://www.morphicons.com/docs), MIT licensed by Guillermo.
The full license is preserved here. Input shapes are a 24-unit circle and a
three-point check. The QML renderer interpolates the generated 64-point shapes;
no browser or JavaScript package runtime is added to the player.

Generation uses `buildPlan(resampleIcon(circle, 64), resampleIcon(check, 64))`
and `interpPolar` at 33 equally spaced progress values. The original generator
is retained in the local Phase G round-two reference directory. Interrupted
animations continue from their current progress; reduced motion jumps directly
to the endpoint.

The video parts button uses Lucide `arrow-up-down` from the same pinned Lucide commit above, as shown in Morphicons. Its ISC/Feather MIT notices remain in LUCIDE-LICENSE; the 24-unit paths are rendered by the existing Qt icon provider.

本轮历史记录入口使用同一固定来源 Lucide clock：24单位圆心(12,12)、半径10，时针折线(12,6)-(12,12)-(16,14)。复用现有抗锯齿提供器按界面尺寸缩放；独立ISC许可仍见LUCIDE-LICENSE，无新运行依赖。
