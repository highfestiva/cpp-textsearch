# textsearch

A no-nonsense text search with ok defaults. The way Windows Explorer search should have been.

## Usage

Make current directory root of where you want to search. Run

```bash
$ ./textsearch.exe searchstring
```

All reasonable files under the current directories will be case-sensitive searched for `searchstring`.
It will ignore .git and .svn directories and ignore large files (>1M) and things like .exe and .png.
