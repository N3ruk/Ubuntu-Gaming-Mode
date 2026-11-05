#!/bin/bash
set -euo pipefail
systemctl --user disable --now ugm-steam-client-update-hook.service
printf 'Hook UGM desactivado. Steam vuelve a usar su actualizador original.\n'
