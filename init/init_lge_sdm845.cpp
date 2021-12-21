/*
   Copyright (c) 2015, The Linux Foundation. All rights reserved.
   Copyright (C) 2016 The CyanogenMod Project.
   Copyright (C) 2018-2019 The LineageOS Project
   Copyright (C) 2018-2019 KudProject Development
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.
   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android-base/logging.h>

#include <sys/sysinfo.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include "property_service.h"
#include "vendor_init.h"

using android::base::SetProperty;
using android::base::Trim;

char const *heapstartsize;
char const *heapgrowthlimit;
char const *heapsize;
char const *heapminfree;
char const *heapmaxfree;
char const *heaptargetutilization;

void property_override(const std::string& name, const std::string& value) {
    size_t valuelen = value.size();

    prop_info* pi = (prop_info*) __system_property_find(name.c_str());
    if (pi != nullptr) {
        __system_property_update(pi, value.c_str(), valuelen);
    }
    else {
        int rc = __system_property_add(name.c_str(), name.size(), value.c_str(), valuelen);
        if (rc < 0) {
            LOG(ERROR) << "property_set(\"" << name << "\", \"" << value << "\") failed: "
                       << "__system_property_add failed";
        }
    }
}

void property_override_multifp(char const buildfp[], char const systemfp[],
	char const bootimagefp[], char const vendorfp[], char const value[])
{
	property_override(buildfp, value);
	property_override(systemfp, value);
	property_override(bootimagefp, value);
	property_override(vendorfp, value);
}

void check_device()
{
    struct sysinfo sys;

    sysinfo(&sys);

    // from - phone-xhdpi-6144-dalvik-heap.mk
    heapstartsize = "16m";
    heaptargetutilization = "0.5";
    heapmaxfree = "32m";
    heapgrowthlimit = "256m";
    heapsize = "512m";
    heapminfree = "8m";
}

void init_target_properties() {
    std::string model;
    std::string product_name;
    std::string cmdline;
    std::string cust_prop_name = "cust.prop";
    std::string default_cust_prop_path = "/oem/OP/" + cust_prop_name;
    std::string cust_prop_path;
    std::string cust_prop_line;
    DIR *dir;
    struct dirent *ent;
    struct stat statBuf;
    bool unknownModel = true;
    bool dualSim = false;

    android::base::ReadFileToString("/proc/cmdline", &cmdline);

    for (const auto& entry : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> pieces = android::base::Split(entry, "=");
        if (pieces.size() == 2) {
            if(pieces[0].compare("androidboot.vendor.lge.product.model") == 0)
            {
                model = pieces[1];
                unknownModel = false;
            } else if(pieces[0].compare("androidboot.vendor.lge.sim_num") == 0 && pieces[1].compare("2") == 0)
            {
                dualSim = true;
            }
        }
    }

    cust_prop_path = default_cust_prop_path;

    if((dir = opendir("/oem/OP/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if(ent->d_type == DT_DIR) {
                std::string tmp = "/oem/OP/";
                tmp.append(ent->d_name);
                tmp.append("/");
                tmp.append(cust_prop_name);
                if(stat(tmp.c_str(), &statBuf) == 0)
                    cust_prop_path = tmp;
            }
        }
        closedir (dir);
    }
    std::ifstream cust_prop_stream(cust_prop_path, std::ifstream::in);

    while(std::getline(cust_prop_stream, cust_prop_line)) {
        std::vector<std::string> pieces = android::base::Split(cust_prop_line, "=");
        if (pieces.size() == 2) {
            if(pieces[0].compare("ro.vendor.lge.build.target_region") == 0 ||
               pieces[0].compare("ro.vendor.lge.build.target_operator") == 0 ||
               pieces[0].compare("ro.vendor.lge.build.target_country") == 0 ||
               pieces[0].compare("telephony.lteOnCdmaDevice") == 0 ||
               pieces[0].compare("persist.vendor.lge.audio.voice.clarity") == 0)
            {
                property_override(pieces[0], pieces[1]);
            }
        }
    }

    if(unknownModel) {
        model = "UNKNOWN";
    }

    if(dualSim) {
        property_override("persist.radio.multisim.config", "dsds");
    }

    property_override("ro.product.model", model);
    property_override("ro.product.odm.model", model);
    property_override("ro.product.product.model", model);
    property_override("ro.product.system.model", model);
    property_override("ro.product.vendor.model", model);
    property_override("ro.boot.verifiedbootstate", "green");
    property_override("ro.oem_unlock_supported", "0");
}

void vendor_load_properties()
{
    check_device();
    SetProperty("dalvik.vm.heapstartsize", heapstartsize);
    SetProperty("dalvik.vm.heapgrowthlimit", heapgrowthlimit);
    SetProperty("dalvik.vm.heapsize", heapsize);
    SetProperty("dalvik.vm.heaptargetutilization", heaptargetutilization);
    SetProperty("dalvik.vm.heapminfree", heapminfree);
    SetProperty("dalvik.vm.heapmaxfree", heapmaxfree);
    init_target_properties();
    property_override_multifp("ro.build.fingerprint", "ro.system.build.fingerprint", "ro.bootimage.build.fingerprint",
	    "ro.vendor.build.fingerprint", "google/redfin/redfin:11/RQ3A.211001.001/7641976:user/release-keys");
}