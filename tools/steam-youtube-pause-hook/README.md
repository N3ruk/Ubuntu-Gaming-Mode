# UGM YouTube controller hook

Runtime source included in **UGM 2.8.5-1**, independent of Decky and the other hook.
Requires Node.js >=22 and native Steam build **1792618285**, with pinned hashes
and API contracts. Unknown clients are rejected. No Steam files are patched.

Synchronizes controller pause/resume with player readiness and preserves playback position.

## Packaged lifecycle

The package runs `ugm-steam-youtube-pause-hook.service` as the UGM user only during the managed
Steam session. `ugm-steam-hooks` prepares and cleans up only its own CDP marker,
migrates exact legacy checkout units and preserves custom conflicts. Use Doctor
and the user journal for status. [Usage and limits](../../docs/ugm-2.8.5/README.md).

## Optional checkout workflow

For development only, run `./tools/steam-youtube-pause-hook/instalar.sh` as the Steam user,
without sudo. It generates a user unit with absolute paths to this checkout and
Node and enables it independently of the package lifecycle. It needs Steam CDP
already available on 127.0.0.1:8080; this standalone installer does not prepare
the marker. Do not run both checkout and packaged units concurrently. Re-run the
installer after moving the checkout. `activar.sh` and `desactivar.sh` control
that checkout unit, not package configuration. Stopping a connected hook attempts
to restore its original descriptors; a closed Steam process retains no hook.

## Checks

```sh
node --test tools/steam-youtube-pause-hook/*.test.mjs
```

Tests cover compatibility, isolation and rollback/reconnection as applicable.
Earlier human testing confirmed controller pause/resume; ads, focus and slow-loading video scenarios are not separately certified.
