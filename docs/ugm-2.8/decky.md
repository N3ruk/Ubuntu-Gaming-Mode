# Optional Decky integration

**English** | [Español](../es/ugm-2.8/decky.md) · [2.8 overview](README.md)

From **2.8.5-1**, the installer offers **Decky Loader** and **UGM Options**: one
entry for Sharp Filter, Downsampling, Video Output and Non-Steam. These are
separate optional prompts, off by default on fresh installations.

[Functions, migration and limits](../ugm-2.8.5/README.md) ·
[Master source](../../integrations/decky/ugm-options/README.md).

If Decky is absent, UGM downloads SteamDeckHomebrew's **official** CLI installer
at revision `9b4e29d2a33b969da11114cce18422a87425f60e`, verifies SHA256
`2403ddbfec8ab320c33729bcb1c8176d74952de71f731ba0c71ded711058be2d`
and runs it with umask 022. Network access, curl and jq are needed; the downloaded
Decky release depends on upstream at installation time. Failure warns without
aborting UGM. Retry with `sudo dpkg-reconfigure ubuntu-gaming-mode`.

The package ships only the master with nested engines and licenses. Settings and
unrelated plugins are preserved; failed migration rolls back plugin folders.
UGM removal does not delete plugin settings. Earlier 2.8 releases used standalone
entries; see their changelog for historical behavior.
