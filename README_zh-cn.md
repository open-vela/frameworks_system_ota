# 项目概述

\[ [English](README.md) | 简体中文 \]

ota 项目主要由四部分组成，分别为 AB 升级管理器（bootctl）、升级包制作脚本（tools）、升级动画应用（ui）和安全校验工具（verify）。

## 第一部分 bootctl

`bootctl` 是系统启动时进行 AB 分区选择的管理器，使用 `bootctl` 命令可以让系统处于不同的状态，内部实现是借助 KVDB 存储相关标记。

### 配置项

```Makefile
CONFIG_UTILS_BOOTCTL=y
CONFIG_UTILS_BOOTCTL_ENTRY=y
CONFIG_UTILS_BOOTCTL_SLOT_A="/dev/ap" #a分区的名字
CONFIG_UTILS_BOOTCTL_SLOT_B="/dev/ap_b" #b分区的名字

#kvdb相关
CONFIG_MTD=y
CONFIG_MTD_BYTE_WRITE=y
CONFIG_MTD_CONFIG_NVS=y
CONFIG_KVDB=y
CONFIG_KVDB_DIRECT=y
```

`CONFIG_UTILS_BOOTCTL_ENTRY` 需要在bootloader的config中开启，ap 则不需要。

### 使用方法

在 `bootloader` 中将 `bootctl` 设置为 entry，这样系统开机 `bootloader`自动进入 `bootctl`进行分区选择。

在 ap 中启动成功后，用户需要调用 `bootctl success `，表示启动成功，`bootctl`就会把当前的分区mark为 `successful`; 在对另外一个分区进行升级时，需要使用 `bootctl update`， 将另外一个分区标记为 `update`状态， 升级成功后使用 `bootctl done`，表示将另外一个分区标记为 `bootable`，重启后会从新升级的分区启动。

### 原理介绍

#### api使用

```C
const char* bootctl_active(void);  //返回当前正在运行的分区名
int bootctl_update(void); //标记升级中
int bootctl_done(void); //标记升级完成
int bootctl_success(void); //标记启动成功
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

#### 启动流程

`bootloader` 为了判断一个系统(slot)是否为可以启动的状态， 需要为其定义对应的属性(状态)，其状态说明如下:

* `active`：活动分区标识， 排他， 代表该分区为启动分区， bootloader总会选择该分区。
* `bootable`：表示该 slot 的分区存在一套可能可以启动的系统。
* `successful`：表示该 slot 的系统能正常启动。


`slot a` 和 `slot b`， 只有一个是active， 它们可以同时有 bootable 和 successful 属性。

bootctl 检测到 `1` 个或者 `2` 个 slot 都是 bootable 的状态:

1. 选择 `active` 或者 `successful` 的slot进行尝试启动。
2. 启动成功的标记是，ap 调用 bootctl success 成功。
3. 如果启动成功，则该 slot 被标记为 successful 和 active。
4. 如果启动失败，设置另一个 slot 为 active，进行下一次尝试。

## 第二部分 tools

### 打包方式

Openvela 提供了python打包脚本 `gen_ota_zip.py`，可根据业务需要生成整包和差分包。

* **整包升级**

  假设要升级的固件是 `vela_ap.bin` 和 `vela_audio.bin`，位于 `new` 目录下，可使用如下命令生成 `OTA` 整包：

  ```Bash
  $ tree new
  new
  ├── vela_ap.bin
  └── vela_audio.bin

  # 生成ota.zip
  $ ./gen_ota_zip.py new --sign

  # ota.zip为签名ota包
  $ ls *.zip
  ota.zip
  ```
* **差分升级**

  假设要升级的固件是 `vela_ap.bin` 和 `vela_audio.bin`，旧版本固件位于 old 目录下，新版本固件位于 new 目录，可使用如下命令生成 OTA 差分包：

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
  # ota.zip为签名ota包
  $ ls *.zip
  ota.zip
  ```

### 定制处理动作

gen_ota_zip.py支持：

* `--user_begin_script` 用户自定义 OTA 预处理脚本

* `--user_end_script`    用户自定义 OTA 后处理脚本

* `--user_file` 打包进 ota.zip 的附件（供预处理、后处理阶段使用）

vendor 定制逻辑放到预处理脚本、后处理脚本里，打包时指定好预处理脚本、后处理脚本以及附件文件即可，参考打包命令：

```JavaScript
./gen_ota_zip.py new/ --debug --sign  --user_end_script testend.sh   --user_begin_script testbegin.sh  --user_file  resources/
```

### **注意事项**

固件名称及设备节点需匹配，升级固件必须以 `vela_<xxx>.bin` 的形式命名，前缀 `vela_`和后缀 `.bin`不可缺少。

同时设备上的文件系统中需要确保存在名为 `/dev/<xxx>` 的设备节点。

例如，要升级的固件名为 `vela_ap.bin` 、`vela_audio.bin`，需要设备上同时存在 /dev/ap 和 /dev/audio 两个设备节点。

## 第三部分 ui

易用，可扩展性强的 `OTA` 升级动画模块，主要包含这几个页面 `Upgrading`、`Upgrade success`、`Upgrade fail` 和 `Logo`。

### 前置条件

* 系统已经支持 `framebuffer`。
* 开启 `OTA UI` 配置。
  ```Makefile
  CONFIG_OTA_UI=y
  ```

### 获取帮助

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

其中 `ota_ui_config.json` 存放了相关资源描述信息：

* 显示开机logo：
  ```Bash
  nsh> otaUI -l
  ```

* 显示升级进度：
  ```Bash
  nsh> otaUI &
  ```

* 模拟升级进度测试：
  在运行了 `otaUI` 程序后，我们可以通过 `-t` 参数来设定升级进度百分比来模拟升级测试过程，`-t` 参数值范围为`0~6` ，可以参考帮助说明，其中 `6` 用来测试升级失败的情况。
  ```Bash
  nsh> otaUI -t 0
  ```

## 第四部分 verify

`openvela` 验签主要包括分区验签和包验签，分别对应 `avb_verify` 和 `zip_verify`，两者使用方法基本一致，这里以 `avb_verify`为例介绍。

### 设备端验签

启用 `AVB` 工具

* 配置项：
  ```Bash
  # 必选 - AVB - 启用AVB库以及Verify工具
  # Crypto和MD与demo key pairs（frameworks/ota/tools/keys/）对应
  CONFIG_LIB_AVB=y
  CONFIG_LIB_AVB_ALGORITHM_TYPE_SHA256_RSA2048=y
  CONFIG_LIB_AVB_SHA256=y
  CONFIG_UTILS_AVB_VERIFY=y

  # 可选 - AVB - 优化Verify速度
  # 设置的数值要与签名时的参数一致（签名见下文）
  CONFIG_LIB_AVB_FOOTER_SEARCH_BLKSIZE=8192

  # 必选 - AVB - 启用ROMFS以预置pub key到/etc目录
  CONFIG_FS_ROMFS=y
  CONFIG_ETC_ROMFS=y
  ```
* 预置 `Key`
  * 将 `key` 编译到 ROMFSETC（vendor/`<VENDOR>`/boards/`<BOARD>`/src/Makefile）
    ```Makefile
    ifneq ($(CONFIG_UTILS_AVB_VERIFY)$(CONFIG_UTILS_ZIP_VERIFY),)
      RCRAWS += etc/key.avb
    endif
    ```
  * 拷贝 `demo key pairs`
    ```Bash
    cd <Vela_TOP_DIR>
    cp frameworks/ota/tools/keys/key.avb \
       vendor/<VENDOR>/boards/<BOARD>/src/etc/key.avb
    ```
* 使用（vendor/`<VENDOR>`/boards/`<BOARD>`/src/etc/init.d/ **rcS.bl** ）
  ```Bash
  # avb_verify
  #  参数1：-k Key
  #  参数2：要校验的文件
  avb_verify -k /etc/key.avb /dev/ap
  if [ $? -eq 0 ]
  then
    boot /dev/ap
  fi
  echo "Boot failed!"
  ```

### 签名镜像

* 使用命令
  ```Bash
  avb_sign.sh
  #  参数1：待签名的文件
  #  参数2：分区大小
  #  Options：
  #     -P：运行时验签路径
  #     -o：附加参数（可选）
  #        --dynamic_partition_size： 仅向待签名文件追加签名信息和必要的对齐padding
  #                                   需要`参数2`为`0`
  #        --block_size： 签名文件时按此参数对齐，典型值128KB
  #                       当分区size较小时可适当减小，但需要是4KB的倍数
  #                      （例如，分区size 4MB，block_size可配置为8KB）
  #                       block_sze越大，验签时搜索的耗时越小
  #        --rollback_index： Rollback Index，用于回滚保护。如果MetaData中的rollback_index小于设备存储的rollback_index，则验签失败。
  ```
* 签名镜像 (vendor/`<VENDOR>`/boards/`<BOARD>`/configs/ap/ **Make.defs** )
  ```Bash
  # 1. （推荐）不填充分区; 搜索步长8KB;
  #    `--block_size` 要与 `CONFIG_LIB_AVB_FOOTER_SEARCH_BLKSIZE`一致
  ${TOPDIR}/../frameworks/ota/tools/avb_sign.sh vela_ap.bin 0 \
                                                -o --dynamic_partition_size \
                                                -P /dev/ap \
                                                -o "--block_size $$((8*1024))";

  # 2. 填充整个分区，分区大小为 2560 KB;
  #
  ${TOPDIR}/../frameworks/ota/tools/avb_sign.sh vela_ap.bin 2560 \
                                                -P /dev/ap;
  ```
