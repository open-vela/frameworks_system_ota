# Project Overview

\[ English | [简体中文](README_zh-cn.md) \]

`ota` project is mainly composed of four parts: AB upgrade manager (`bootctl`), ota package script (`tools`), upgrade animation application (`ui`) and security verification tool (`verify`).

## Part 1 bootctl

`bootctl` is a manager for AB partition selection during system boot. Using the `bootctl` command allows the system to be in different states, with the internal implementation relying on `KVDB` to store related flags.

### Config

```Makefile
CONFIG_UTILS_BOOTCTL=y
CONFIG_UTILS_BOOTCTL_ENTRY=y
CONFIG_UTILS_BOOTCTL_SLOT_A="/dev/ap"
CONFIG_UTILS_BOOTCTL_SLOT_B="/dev/ap_b"

kvdb related
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_MTD_CONFIG_NVS=y
CONFIG_KVDB=y
CONFIG_KVDB_DIRECT=y
```

CONFIG_UTILS_BOOTCTL_ENTRY should be enabled in the config of the bootloader, but is not required for the ap.

### Usage

In the `bootloader`, set `bootctl` as the `entry` point so that during system boot, the `bootloader` automatically enters `bootctl` for partition selection. 

Once the system has successfully started in the `ap`, the user needs to call `bootctl success` to indicate successful booting, which prompts bootctl to mark the current partition as `successful`. When upgrading another partition, use `bootctl update` to mark the other partition as `update` status. After a successful upgrade, use `bootctl done` to mark the other partition as `bootable`, which will then start from the newly upgraded partition upon reboot.

### Introduction to the principle

#### API usage

```C
const char* bootctl_active(void); //Return the name of the partition currently running
int bootctl_update(void); //Mark the upgrade in progress
int bootctl_done(void); //Mark the upgrade completed
int bootctl_success(void); //Mark the boot successfully
```

#### kvdb

```C
#define MAGIC "vela boot manager"
#define SLOT_MAGIC "persist.boot.magic"

#define SLOT_A_PATH "/dev/ap"
#define SLOT_A_ACTIVE "persist.boot.slot_a.active"
#define SLOT_A_BOOTABLE "persist.boot.slot_a.bootable"
#define SLOT_A_SUCCESSFUL "persist.boot.slot_a.successful"

#define SLOT_B_PATH "/dev/ap_b"
#define SLOT_B_ACTIVE "persist.boot.slot_b.active"
#define SLOT_B_BOOTABLE "persist.boot.slot_b.bootable"
#define SLOT_B_SUCCESSFUL "persist.boot.slot_b.successful"

#define SLOT_TRY "persist.boot.try"

```

#### Boot process

`bootloader` In order to determine whether a system (slot) is in a bootable state, it is necessary to define the corresponding attributes (states) for it. The state description is as follows:

* `active`: active partition identifier, exclusive, indicating that the partition is the boot partition, and the bootloader will always select this partition.

* `bootable`: Indicates that there is a system that may be bootable in the partition of this `slot`.
* `successful`: Indicates that the system of this `slot` can be started normally.

Only one of `slot a` and `slot b` is active, and they can have both bootable and successful attributes.

Bootctl detects that one or two slots are bootable:

1. Select the slot of `active` or `successful` to try to start.
2. The mark of successful startup is that ap calls bootctl success successfully.
3. If the startup is successful, the slot is marked as successful and active.
4. If the startup fails, set another slot as active and make the next attempt.

## Part 2 tools

### Packaging method

Openvela provides a Python packaging script `gen_ota_zip.py`, which can generate full packages and differential packages according to business needs.

* **Full package upgrade**

  Assuming that the firmware to be upgraded is `vela_ap.bin` and `vela_audio.bin`, which are located in the `new` directory, you can use the following command to generate the `OTA` full package:

  ```Bash
  $ tree new
  new
  ├── vela_ap.bin
  └── vela_audio.bin

  # Generate ota.zip
  $ ./gen_ota_zip.py new --sign

  # ota.zip is a signed ota package
  $ ls *.zip
  ota.zip
  ```
* **Differential upgrade**

  Assuming that the firmware to be upgraded is `vela_ap.bin` and `vela_audio.bin`, the old version of the firmware is located in the old directory, and the new version of the firmware is located in the new directory, you can use the following command to generate the OTA differential package:

  ```Bash
  $ tree old new
  old
  ├── vela_ap.bin
  └── vela_audio.bin
  new
  ├── vela_ap.bin
  └── vela_audio.bin

  $ ./gen_ota_zip.py old new --sign
  ...
  # ota.zip is a signed ota package
  $ ls *.zip
  ota.zip
  ```

### Custom processing actions

gen_ota_zip.py supports:

* `--user_begin_script` User-defined OTA pre-processing script

* `--user_end_script` User-defined OTA post-processing script

* `--user_file` Attachments packaged into ota.zip (for use in pre-processing and post-processing stages)

Put the vendor custom logic in the pre-processing script and post-processing script. When packaging, specify the pre-processing script, post-processing script and attachment file. Refer to the packaging command:

```JavaScript
./gen_ota_zip.py new/ --debug --sign --user_end_script testend.sh --user_begin_script testbegin.sh --user_file resources/
```

### **Notes**

The firmware name and device node must match. The firmware to be upgraded must be named in the form of `vela_<xxx>.bin`. The prefix `vela_` and the suffix `.bin` are indispensable.

At the same time, it is necessary to ensure that there is a device node named `/dev/<xxx>` in the file system of the device.

For example, if the firmware to be upgraded is named `vela_ap.bin` and `vela_audio.bin`, the device nodes /dev/ap and /dev/audio must exist on the device at the same time.

## Part 3 ui

Easy-to-use and highly scalable `OTA` upgrade animation module, mainly including the following pages: `Upgrading`, `Upgrade success`, `Upgrade fail` and `Logo`.

### Prerequisites

* The system already supports `framebuffer`.
* Enable `OTA UI` configuration.
  ```Makefile
  CONFIG_OTA_UI=y
  ```


### Get help 
```Bash
nsh> otaUI -h
 Usage: otaUI [options]
        -t test mode.         set the upgrade progress for test.
                 0 : progress current:0,progress next 20
                 1 : progress current:20,progress next 40
                 2 : progress current:40,progress next 60
                 3 : progress current:60,progress next 90
                 4 : progress current:90,progress next 100
                 5 : progress current:100,progress next 100
                 6 : progress current:-1,progress next 100
        -l logo mode.             show logo,default is upgrade UI mode
        -c ota ui config path.    default is /resource/recovery/ota_ui_config.json
        -h print help message.
```

Among them, `ota_ui_config.json` stores the relevant resource description information:

* Display the boot logo:
  ```Bash
  nsh> otaUI -l
  ```

* Display upgrade progress:
  ```Bash
  nsh> otaUI &
  ```

* Simulate upgrade progress test:
After running the `otaUI` program, we can use the `-t` parameter to set the upgrade progress percentage to simulate the upgrade test process. The `-t` parameter value range is `0~6`. Please refer to the help instructions. Among them, `6` is used to test the upgrade failure.
  ```Bash
  nsh> otaUI -t 0
  ```

## Part 4 verify

`openvela` signature verification mainly includes partition signature verification and package signature verification, which correspond to `avb_verify` and `zip_verify` respectively. The usage of the two is basically the same. Here we take `avb_verify` as an example.
### Verify on the device

Enable `AVB`

* Config:
  ```Bash
  # mandatory
  CONFIG_LIB_AVB=y
  CONFIG_LIB_AVB_ALGORITHM_TYPE_SHA256_RSA2048=y
  CONFIG_LIB_AVB_SHA256=y
  CONFIG_UTILS_AVB_VERIFY=y

  # option
  CONFIG_LIB_AVB_FOOTER_SEARCH_BLKSIZE=8192

  # mandatory
  CONFIG_FS_ROMFS=y
  CONFIG_ETC_ROMFS=y
  ```
* Preset Key
  * Build `key` into ROMFSETC（vendor/`<VENDOR>`/boards/`<BOARD>`/src/Makefile）
    ```Makefile
    ifneq ($(CONFIG_UTILS_AVB_VERIFY)$(CONFIG_UTILS_ZIP_VERIFY),)
      RCRAWS += etc/key.avb
    endif
    ```
  * Copy `demo key pairs`
    ```Bash
    cd <Vela_TOP_DIR>
    cp frameworks/ota/tools/keys/key.avb \
       vendor/<VENDOR>/boards/<BOARD>/src/etc/key.avb
    ```
* Usage （vendor/`<VENDOR>`/boards/`<BOARD>`/src/etc/init.d/ **rcS.bl** ）

    ```Bash
    # avb_verify
    #    param1：-k Key
    #    param2：file to be verified
    avb_verify -k /etc/key.avb /dev/ap
    if [ $? -eq 0 ]
    then
      boot /dev/ap
    fi
    echo "Boot failed!"
    ```

### Sign image

* Usage
  ```Bash
  avb_sign.sh
  #  param1：file to be signed
  #  param2：partition size
  #  Options：
  #     -P：Run time check path
  #     -o：Additional parameters (optional)
  #        --dynamic_partition_size： Append only the signature information and the necessary padding to the file to be signed, requires parameter 2 to be 0
  #        --block_size： Use this parameter when signing files. The typical value is 128KB
  # When the partition size is small, it can be reduced appropriately, but it needs to be a multiple of 4KB. (For example, partition size 4MB, block_size can be configured to 8KB)
  # The larger the # block_sze, the smaller the search time during checkup
  #        --rollback_index：Rollback Index: indicates rollback protection. If the rollback_index in the MetaData is smaller than the rollback_index of the device storage, the check fails
  ```
* Sign Image (vendor/`<VENDOR>`/boards/`<BOARD>`/configs/ap/ **Make.defs** )
  ```Bash
  # 1. (Recommended) Do not fill the partition; Search step size 8KB;
  ${TOPDIR}/../frameworks/ota/tools/avb_sign.sh vela_ap.bin 0 \
                                                -o --dynamic_partition_size \
                                                -P /dev/ap \
                                                -o "--block_size $$((8*1024))";

  # 2. Fill the entire partition with a partition size of 2560 KB;
  ${TOPDIR}/../frameworks/ota/tools/avb_sign.sh vela_ap.bin 2560 \
                                                -P /dev/ap;
  ```
