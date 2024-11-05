# Project Overview

OTA project is mainly composed of four parts: AB upgrade manager (bootctl), ota package script (tools), upgrade animation application (ui) and security verification tool (verify).

## bootctl

Bootctl is a manager for AB partition selection during system boot. Using the bootctl command allows the system to be in different states, with the internal implementation relying on KVDB to store related flags.

1. ### Config

```Makefile
CONFIG_UTILS_BOOTCTL=y
CONFIG_UTILS_BOOTCTL_ENTRY=y
CONFIG_UTILS_BOOTCTL_SLOT_A="/dev/ap"
CONFIG_UTILS_BOOTCTL_SLOT_B="/dev/ap_b"

kvdb related
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_MTD_CONFIG=y
CONFIG_MTD_CONFIG_FAIL_SAFE=y
CONFIG_KVDB=y
CONFIG_KVDB_DIRECT=y
```

CONFIG_UTILS_BOOTCTL_ENTRY should be enabled in the config of the bootloader, but is not required for the ap.

2. ### Usage

In the bootloader, set `bootctl` as the entry point so that during system boot, the bootloader automatically enters bootctl for partition selection. Once the system has successfully started in the AP, the user needs to call `bootctl success` to indicate successful booting, which prompts bootctl to mark the current partition as `successful`. When upgrading another partition, use `bootctl update` to mark the other partition as `update` status. After a successful upgrade, use `bootctl done` to mark the other partition as `bootable`, which will then start from the newly upgraded partition upon reboot.

3. ### Principle introduction

#### api

```C
const char* bootctl_active(void);
int bootctl_update(void);
int bootctl_done(void);
int bootctl_success(void);
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

#### boot

In order to determine whether a system (slot) can boot normally, bootloader needs to define corresponding properties (status) which are described as follows:

* active: The value is exclusive. It indicates that this partition is the boot partition and always selected by bootloader.
* bootable: Indicates that a system can boot from this slot partition.
* successful: Indicates that the system in this slot is successfully started.

slot a and slot b, only one of which is active, can both have bootable and successful attributes.

Bootctl detects that one or two slots are in the bootable state:

1. Select an active slot or a successful slot.
2. The ap calls bootctl success successfully.
3. If the slot boots normally, marks it as successful and active.
4. If the slot boots failed, set the other slot to be active for the next attempt.

## tools

1. ### Packaging

Vela provides the python packaging script `gen_ota_zip.py` which can generate full packages and diff packages according to business needs.

* full ota
  Assume that the firmware vela_ap.bin and vela_audio.bin to be upgraded are stored in the directory named "new":

  ```Bash
  $ tree new
  new
  ├── vela_ap.bin
  └── vela_audio.bin

  $ ./gen_ota_zip.py new --sign
  $ ls *.zip
  ota.zip
  ```
* diff ota

  Assume that the firmware to be upgraded are vela_ap.bin and vela_audio.bin. The old firmware is stored in the "old" directory, and the new firmware is stored in the "new" directory:

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

$ ls *.zip
ota.zip
```

2. ### Customize processing actions

`gen_ota_zip.py` supported following three customization options:

* `--user_begin_script` User-defined OTA pre-processing script
* `--user_end_script`    User-defined OTA post-processing script
* `--user_file`   Packages the attachments into ota.zip (Used in pre- and post-processing)

Specify the pre- and post- processing script and attachments during packaging. For details, see the packaging command:

```JavaScript
./gen_ota_zip.py new/ --debug --sign  --user_end_script testend.sh   --user_begin_script testbegin.sh  --user_file  resources/
```

3. ### **Cautions**

The firmware name must match the name of the device node. The firmware must be named in vela_`<xxx>`.bin format. The prefix `vela_` and suffix `.bin` are indispensable. In addition, a device node named /dev/`<xxx>` should exist in the file system on the device. For example, if the firmware names are vela_ap.bin and vela_audio.bin, then  device nodes `/dev/ap` and `/dev/audio` must exist on the device.

## ui

A set of easy-to-use, scalable OTA upgrade animation module, mainly including these pages `Upgrading`, `Upgrade success`, `Upgrade fail`, `Logo`.

1. ### Precondition

* The system supports framebuffer
* Enable UI configuration

```Makefile
CONFIG_OTA_UI=y
```

2. ### Get help

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
        -l logo mode.         show logo,default is upgrade UI mode
        -c ota ui config path.         default is /resource/recovery/ota_ui_config.json
        -h print help message.
```

The file of ota_ui_config.json stores the resource description information.

* display booting logo

```Bash
nsh> otaUI -l
```

* display ota progress

```Bash
nsh> otaUI &
```

* Simulated ota progress test

After running the otaUI program, we can simulate the upgrade test process by using the -t parameter to set the upgrade progress percentage, the value of the -t parameter ranges from 0 to 6, you can refer to the help info, where 6 is used to test the case of upgrade failure.

```Bash
nsh> otaUI -t 0
```

## verify

Vela safety verification mainly includes partition check and package check, which corresponding to avb_verify and zip_verify respectively. The two methods are basically the same. This section takes avb_verify as an example.

1. ### Verify on the device

Enable AVB

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
  * Build key into ROMFSETC（vendor/`<VENDOR>`/boards/`<BOARD>`/src/Makefile）
    ```Makefile
    ifneq ($(CONFIG_UTILS_AVB_VERIFY)$(CONFIG_UTILS_ZIP_VERIFY),)
      RCRAWS += etc/key.avb
    endif
    ```
  * Copy demo key pairs
    ```Bash
    cd <Vela_TOP_DIR>
    cp frameworks/ota/tools/keys/key.avb \
       vendor/<VENDOR>/boards/<BOARD>/src/etc/key.avb
    ```
* Usage （vendor/`<VENDOR>`/boards/`<BOARD>`/src/etc/init.d/ **rcS.bl** ）

```Bash
# avb_verify
#    param1：file to be verified
#    param2：Key
avb_verify /dev/ap /etc/key.avb
if [ $? -eq 0 ]
then
  boot /dev/ap
fi
echo "Boot failed!"
```

2. ### Sign image

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