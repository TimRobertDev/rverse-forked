# RVRSE Enhanced — experimental source patch

## Changes
- MP3 input via `dr_mp3.h` (decoded off the audio thread into the existing SampleData PCM format).
- WAV/AIFF loading and existing DSP/export preserved.
- Windows: drag the combined waveform more than six pixels to export the current riser + hit as a temporary 24-bit stereo WAV and initiate a native OLE `CF_HDROP` file drag.
- The existing file-drop path also accepts `.mp3` and files with real filesystem paths from Ableton.

## Building
1. Clone the iPlug2 submodule and dependencies (the GitHub source ZIP contains an empty `iPlug2` folder). Follow the upstream project's build instructions.
2. Configure with CMake on Windows, with internet access to fetch `dr_mp3.h` (or manually place the upstream `dr_mp3.h` in `RVRSE/libs/dr_libs/`).
3. Build the Windows VST3 target in Release mode.
4. Install as a **separate test copy** and verify in Ableton before replacing an existing plugin.

## Known limitations / important tests
- **Not compiled or tested in this environment** (iPlug2 and third-party dependencies are absent).
- Windows OLE file dragging depends on Ableton accepting `CF_HDROP` from a VST editor. Test in the actual host.
- Consolidated Ableton clips: direct drop works only if Ableton provides a filesystem path. Ableton's internal clip-drag protocol has not been implemented.
- The WAV is rendered synchronously when dragging starts; long samples can briefly block the GUI.
- Temporary files in `%TEMP%/RVRSE-Drag` are intentionally retained so Ableton can read them after drop. They may be deleted manually when no longer needed.
- MP3 header download currently uses the latest upstream dr_libs version; pin it to a reviewed revision for reproducible release builds.
- Windows Unicode filenames may require further handling in the MP3 decoder's file-open path.
