# Game Boy Emulator - Web Build

This folder contains the web build of the Game Boy emulator for GitHub Pages.

## Setup Instructions

1. **Enable GitHub Pages**:
   - Go to your repository on GitHub
   - Click on **Settings** → **Pages**
   - Under "Source", select **Deploy from a branch**
   - Select branch: **main**
   - Select folder: **/docs**
   - Click **Save**

2. **Access your emulator**:
   - After a few minutes, your emulator will be available at:
   - `https://[your-username].github.io/[repository-name]/`

3. **Updating the build**:
   - Rebuild the web version: `cd build_wasm && ninja`
   - Copy the new files: `cp build_wasm/index.html build_wasm/GBC.js build_wasm/GBC.wasm docs/`
   - Commit and push: `git add docs/ && git commit -m "Update web build" && git push`

## Files

- `index.html` - The main HTML file with the emulator UI
- `GBC.js` - The Emscripten JavaScript wrapper
- `GBC.wasm` - The WebAssembly binary

