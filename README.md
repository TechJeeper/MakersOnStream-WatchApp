# MakersOnStream (Pebble)

See who’s live on [MakersOnStream](https://makersonstream.app) and what’s coming up next — on your wrist.

## On the watch

| Control | Action |
| --- | --- |
| Select | Open stream details |
| Long Select | Refresh from phone |
| Select on empty/loading | Refresh |

Sections:

- **LIVE** — makers streaming now
- **UP NEXT** — upcoming sessions (next 1–3 days)

## Settings (phone)

Open **MakersOnStream → Settings** in the Pebble app:

- **Everyone** — public live list + calendar (no API key)
- **Following** — only makers you follow (needs your API key from [Account](https://makersonstream.app/account))
- **Upcoming window** — 1 / 2 / 3 days

## Build

On Windows (Podman or Docker):

```powershell
./build.ps1
```

Output: `build/MakersOnStreamWatchApp.pbw`

```powershell
./build.ps1 -Phone 192.168.1.23
./build.ps1 -Zip
```

## Layout

```
src/c/       watch UI + AppMessage
src/pkjs/    API fetch + settings
config/      phone settings page
```
