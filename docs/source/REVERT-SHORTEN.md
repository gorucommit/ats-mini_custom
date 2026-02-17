# Revert to state before shortening (class/ID/JS/API refactor)

**Before (backup):** `control.html.before-shorten`  
**After (current):** `control.html`

## Revert (restore pre-shorten version)

```bash
cd docs/source
cp control.html.before-shorten control.html
```

Or with git, to discard the shortening and go back to last committed version:

```bash
git checkout -- docs/source/control.html
```

## Size comparison

### Raw / source

|            | Before (backup) | After (current) | Delta    |
|------------|-----------------|----------------|----------|
| **Bytes**  | 70,041          | 68,622         | -1,419   |
| **Lines**  | 1,751           | 1,795          | +44      |

### Minified (embed_html.py – HTML/CSS/JS minified)

|            | Before (backup) | After (current) | Delta    |
|------------|-----------------|----------------|----------|
| **Bytes**  | 53,071          | 50,981         | **-2,090** |

### Compressed (gzip -9, same as firmware embed)

|            | Before (backup) | After (current) | Delta    |
|------------|-----------------|----------------|----------|
| **Bytes**  | **12,752**      | **13,035**     | **+283** |

**Summary:** The shortened source is smaller raw and minified, but **gzip compressed size is slightly larger** (~283 bytes). Gzip benefits from repeated long strings (e.g. repeated class names); shorter tokens can reduce that redundancy. So for **flash size** (what gets embedded), the pre-shorten version is slightly better; for **transfer or minified size**, the shortened version wins.
