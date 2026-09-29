# Known issues

**English** | [Español](es/problemas-conocidos.md)

## Black rectangle around Steam Big Picture notifications

### Status

**External limitation that UGM cannot currently fix safely.**

On the NVIDIA reference system, some Steam Gamepad UI notifications are shown
inside a solid black rectangle instead of preserving transparency around the
content. Notification size and placement are correct at 1080p, 1440p and 4K.

### Why this is not currently attributed to UGM

- The same black rectangle is reproducible in Steam Big Picture on the normal
  desktop, without Gamescope or the UGM session.
- The captured X11 `notificationtoasts_uid2` surface already contained opaque
  black margins before final Gamescope composition.
- `steamwebhelper` logs report
  `GLX_EXT_texture_from_pixmap extension unavailable`, fall back from the
  OpenGL composer to the System composer and then report that the requested
  transparent background is unsupported.
- Forcing `-cef-force-glx` did not change the symptom or prevent that fallback.

The evidence points to the Steam/steamwebhelper composition path and its
interaction with the NVIDIA graphics stack. It does not yet prove whether the
specific defect is in Steam, CEF or the driver, but it does show that treating it
as a UGM runtime defect would be unsupported.

### UGM scope

UGM controls the Gamescope session, its arguments and the packaged runtime. It
cannot restore transparency after Steam has already replaced it with opaque
black pixels. A Gamescope-side colour-key workaround would be destructive and
could make legitimate black UI or game content transparent.

UGM therefore does not modify Gamescope for this issue. Gaming, 4K, HDR, VRR,
scaling and Steam Overlay functionality are unaffected by this decision.

### Upstream tracking and mitigation

- ValveSoftware/steam-for-linux
  [#13196](https://github.com/ValveSoftware/steam-for-linux/issues/13196)
  reports the same Big Picture black border without requiring Gamescope.
- ValveSoftware/gamescope
  [#2171](https://github.com/ValveSoftware/gamescope/issues/2171) reports a
  similar NVIDIA 595 symptom, although that reporter only reproduces it under
  Gamescope, so it is not identical to UGM's reproduction.

There is currently no universal, confirmed community workaround. Reducing or
disabling the relevant Steam notification categories is the available
mitigation. UGM will revisit the issue if a Steam/driver update fixes it or if
new evidence identifies a reproducible UGM-specific component.
