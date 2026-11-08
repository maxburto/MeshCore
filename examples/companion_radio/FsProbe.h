#pragma once
// Bench build only (-D FS_PROBE=1): reports why a filesystem would not mount, instead of letting the framework
// format it, and logs heard packets too long for a log frame. Nothing here goes into a shipped build.
void fsProbeBeforeMount();   // before InternalFS.begin(); stays here, with the filesystem untouched, if a mount fails
void fsProbeAfterStore();    // after store.begin(): which of the stored files are there
void fsProbeTick();          // from loop(): repeats the result, since the host's port opens after boot
void fsProbeOversize(float snr, float rssi, const unsigned char* raw, int len);
