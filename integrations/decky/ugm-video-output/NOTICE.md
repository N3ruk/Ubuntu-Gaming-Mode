# Attribution and provenance

**UGM Video Output** by N3ruk (2026) adapts **Video Output ALLY for DOCKs 1.0**
from https://github.com/N3ruk/SteamOS-ASUS-ROG-ALLY at the commit in ORIGIN.json.
Both derive from **Output Manager** by joeatethebeans:
https://github.com/joeatethebeans/OutputManager

The original copyright notices and BSD-3-Clause conditions are retained in
LICENSE. Names do not imply endorsement by upstream authors, ASUS, Valve or Decky.

UGM changes the visible name and internal-panel naming, discovers its own DRM
session/environment and includes a read-only gamescope_control v2 display reader
instead of depending on SteamOS gamescopectl. The switch/release transaction,
EDID identity, monitor-label cache and suspension protections are retained.
No audio controller or management/settings/about pages are included.

The frontend uses the public Decky API/UI. Exact dependency versions are recorded
in package-lock.json and their licenses are retained in licenses/npm/.
The Valve gamescope_control protocol license is in licenses/gamescope-control-MIT.txt.
