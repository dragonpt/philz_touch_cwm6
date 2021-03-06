/*
 * Copyright (c) 2013 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "voldclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"

int vold_update_volumes() {

    const char *cmd[2] = {"volume", "list"};
    return vold_command(2, cmd, 1);
}

int vold_mount_volume(const char* path, int wait) {

    const char *cmd[3] = { "volume", "mount", path };
    int state = vold_get_volume_state(path);

    if (state == State_Mounted) {
        LOGI("Volume %s already mounted\n", path);
        return 0;
    }

    if (state != State_Idle) {
        LOGI("Volume %s is not idle, current state is %d\n", path, state);
        return -1;
    }

    if (access(path, R_OK) != 0) {
        mkdir(path, 0000);
        chown(path, 1000, 1000);
    }
    return vold_command(3, cmd, wait);
}

int vold_unmount_volume(const char* path, int force, int wait) {

    const char *cmd[4] = { "volume", "unmount", path, "force" };
    int state = vold_get_volume_state(path);

    if (state <= State_Idle) {
        LOGI("Volume %s is not mounted\n", path);
        return 0;
    }

    if (state != State_Mounted) {
        LOGI("Volume %s cannot be unmounted in state %d\n", path, state);
        return -1;
    }

    return vold_command(force ? 4: 3, cmd, wait);
}

int vold_share_volume(const char* path) {

    const char *cmd[4] = { "volume", "share", path, "ums" };
    int state = vold_get_volume_state(path);

    if (state == State_Mounted)
        vold_unmount_volume(path, 0, 1);

    return vold_command(4, cmd, 1);
}

int vold_unshare_volume(const char* path, int mount) {

    const char *cmd[4] = { "volume", "unshare", path, "ums" };
    int state = vold_get_volume_state(path);
    int ret = 0;

    if (state != State_Shared) {
        LOGE("Volume %s is not shared - state=%d\n", path, state);
        return 0;
    }

    ret = vold_command(4, cmd, 1);

    if (mount)
        vold_mount_volume(path, 1);

    return ret;
}

int vold_format_volume(const char* path, int wait) {

    const char* cmd[3] = { "volume", "format", path };
    return vold_command(3, cmd, wait);
}

/*
vold_custom_format_volume():
- event_loop.c / vold_command(): will parse command line and send it to minivold binary compiled from android_system_vold repo
- in android_system_vold (minivold) / CommandListener.cpp / int CommandListener::VolumeCmd::runCommand()
  will interpret the command line "format" and return VolumeManager.cpp / formatVolume(const char *label, const char *fstype)
- VolumeManager.cpp / formatVolume() will return Volume.cpp / Volume::formatVol(const char* fstype)
- formatVol() will format to fstype == exfat, ext4 or ntfs.
  if fstype == NULL it will detect current fstype
  else if fstype == any other thing, it will format to vfat
- in our vold_custom_format_volume(), we cannot send fstype as NULL
  so, to format to current file system, call vold_format_volume() which doesn't support custom fstype
*/
int vold_custom_format_volume(const char* path, const char* fstype, int wait) {
    const char* cmd[4] = { "volume", "format", path, fstype };
    return vold_command(4, cmd, wait);
}

/* android_system_vold/Volume.h
    static const int State_Init       = -1;
    static const int State_NoMedia    = 0;
    static const int State_Idle       = 1;
    static const int State_Pending    = 2;
    static const int State_Checking   = 3;
    static const int State_Mounted    = 4;
    static const int State_Unmounting = 5;
    static const int State_Formatting = 6;
    static const int State_Shared     = 7;
    static const int State_SharedMnt  = 8;
*/
const char* volume_state_to_string(int state) {
    if (state == State_Init)
        return "Initializing";
    else if (state == State_NoMedia)
        return "No-Media";
    else if (state == State_Idle)
        return "Idle-Unmounted";
    else if (state == State_Pending)
        return "Pending";
    else if (state == State_Mounted)
        return "Mounted";
    else if (state == State_Unmounting)
        return "Unmounting";
    else if (state == State_Checking)
        return "Checking";
    else if (state == State_Formatting)
        return "Formatting";
    else if (state == State_Shared)
        return "Shared-Unmounted";
    else if (state == State_SharedMnt)
        return "Shared-Mounted";
    else
        return "Unknown-Error";
}

