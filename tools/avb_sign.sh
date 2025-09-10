#! /bin/bash
#
# Copyright (C) 2024 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e

REPO_ROOT=$(realpath $(dirname $(realpath $0))/../../../../)

readonly OTA_TOOL_PATH=$REPO_ROOT/frameworks/system/ota/tools
readonly AVB_TOOL_PATH=$REPO_ROOT/external/avb/avb
readonly KEY_PATH=$OTA_TOOL_PATH/keys
readonly AVBTOOL=$AVB_TOOL_PATH/avbtool
readonly BIN2HEX=bin2hex.py
readonly HEX2BIN=hex2bin.py
readonly HEXINFO=hexinfo.py
readonly DEFAULT_KEY=$KEY_PATH/key.pem
readonly DEFAULT_ALG=SHA256_RSA2048
readonly SUPPORTED_ALG=(SHA256_RSA2048 SHA256_RSA4096 SHA256_RSA8192 \
                        SHA512_RSA2048 SHA512_RSA4096 SHA512_RSA8192)

readonly KSIZE=1024

TMP_BIN=$(mktemp /tmp/signed_temp.XXXXXX.bin)
TMP_HEX=$(mktemp /tmp/signed_temp.XXXXXX.hex)

cleanup() {
    rm -f "$TMP_BIN" "$TMP_HEX"
}
trap cleanup EXIT

_help(){
  printf "  %-16s   %s\n" "${1}" "${2}"
}
__help(){
  printf "      %-12s" "${1}"
  shift
  printf "   %s\n" "$@"
}
help(){
  echo -e "Usage: $0 <image2sign> <partition_size>" \
          "[options]\n"
  _help "<image2sign>" "Full path of image to be signed"
  __help "NOTE" "The \"basename\" must BE SAME AS partition name, OR, "
  __help ""     "using additional \"-P\" option."
  _help "<partition_size>" "Partition size (*$KSIZE)"
  echo -e "\nOptions:"
  _help "[-a algorithm]" "Algorithm of sign, ${DEFAULT_ALG} by default"
  printf "      %-12s   %s" "Supported" && echo "${SUPPORTED_ALG[@]}"
  _help "[-k key_path]" "Path of private key, ${DEFAULT_KEY} by default"
  _help "[-o options]" "Option(s) append to avbtool"
  __help "--padding_ff" "Padding 0xff for DO_NOT_CARE area"
  _help "[-P verify_path]" "Path of FILE to be verified"
  __help "FILE" "eg. Device point(/dev/ap), ELF(/ota/ota.elf), ..."
  _help "[-I format]" "Input format (ihex or binary), auto-detect by default"
  exit 1
}
check_e(){
  if [ ! -e "$1" ]; then
    fatal "File not found: $1"
  fi
}
fatal(){
  echo -e "FATAL: $@"
  exit 2
}
printvar(){
  [[ $# -lt 2 ]] \
    && printf "%-16s : %s\n" $1 ${!1} \
    || printf "%-$((16 - ${#2}))s (%s) : %s\n" $1 $2 ${!1}
}

# Check Tool
if ! $AVBTOOL --help >/dev/null ; then
  fatal "Tool ($AVBTOOL) check failed"
fi

# Parse & Check ARGs
if ! python3 -c "import intelhex" >/dev/null 2>&1; then
  echo "python module 'intelhex' missing. Install with: pip3 install intelhex"
  pip3 install intelhex
fi

get_base_addr() {
    local HEX_FILE="$1"
    local BASE_ADDR
    BASE_ADDR=$("$HEXINFO" "$HEX_FILE" | grep -oE 'first: 0x[0-9a-fA-F]+' | \
                grep -oE '0x[0-9a-fA-F]+' | \
                sort -k1,1n | \
                head -n 1)

    if [ -z "$BASE_ADDR" ]; then
        fatal "can get base addr from $HEX_FILE "
    fi

    echo "$BASE_ADDR"
    return 0
}

[[ $# -lt 2 ]] && help
IMAGE2SIGN=$1
PARTITION_SIZE=$(($2 * $KSIZE)) # KB -> B # TODO : Get from partition table
shift; shift
while getopts "k:a:o:P:I:" opt ; do
  case $opt in
    k)
      IN_PRIVKEY=$OPTARG
      ;;
    a)
      ALGORITHM=$OPTARG
      ;;
    o)
      OPTIONS=(${OPTIONS[@]} $OPTARG)
      ;;
    P)
      DEV_PATH=$OPTARG
      ;;
    I)
      if [[ "$OPTARG" != "ihex" && "$OPTARG" != "binary" ]]; then
          fatal "Unsupported input format: $OPTARG (must be ihex or binary)"
      fi
      INPUT_FORMAT=$OPTARG
      ;;
    ?)
      help
      ;;
  esac
done

IN_PRIVKEY=${IN_PRIVKEY:-$DEFAULT_KEY}
ALGORITHM=${ALGORITHM:-$DEFAULT_ALG}

check_e $IMAGE2SIGN
check_e $IN_PRIVKEY

if [ -z "$INPUT_FORMAT" ]; then
    if "$HEXINFO" "$IMAGE2SIGN" > /dev/null 2>&1; then
        INPUT_FORMAT="ihex"
    else
        INPUT_FORMAT="binary"
    fi
    echo "Auto-detected input format: $INPUT_FORMAT"
fi

if ! echo ${SUPPORTED_ALG[@]} | grep $ALGORITHM > /dev/null ; then
  fatal "Algorithm Supported: ${SUPPORTED_ALG[@]}"
fi

# Get partition name
if [ -z $DEV_PATH ] ; then
  DEV_PATH="/dev/$(basename $IMAGE2SIGN)"
fi

# Info
if [ "$INPUT_FORMAT" = "ihex" ]; then
    "$HEX2BIN" "$IMAGE2SIGN" "$TMP_BIN" \
      || fatal "HEX to BIN conversion failed"
    WORKING_IMAGE="$TMP_BIN"
else
    WORKING_IMAGE="$IMAGE2SIGN"
fi

printvar IMAGE2SIGN
printvar INPUT_FORMAT "format"
printvar PARTITION_SIZE "bytes"
printvar DEV_PATH
printvar IN_PRIVKEY
printvar ALGORITHM
[[ ${#OPTIONS[@]} -gt 0 ]] && printf "%-16s : " OPTIONS && echo "${OPTIONS[@]}"

# Sign
$AVBTOOL add_hash_footer --image $WORKING_IMAGE \
        --partition_size $PARTITION_SIZE \
        --partition_name $DEV_PATH \
        --key $IN_PRIVKEY --algorithm $ALGORITHM ${OPTIONS[@]} \
        || fatal "Signing failed"

if [ "$INPUT_FORMAT" = "ihex" ]; then
    START_ADDR=$(get_base_addr "$IMAGE2SIGN")
    "$BIN2HEX" --offset "$START_ADDR" "$WORKING_IMAGE" "$TMP_HEX" \
      || fatal "BIN to HEX conversion failed"
    OUTPUT_IMAGE="${IMAGE2SIGN%.*}.hex"
    RECHECK_ADDR=$(get_base_addr "$TMP_HEX")
    if [ "$START_ADDR" != "$RECHECK_ADDR" ]; then
        fatal "Start address changed after sign: $START_ADDR -> $RECHECK_ADDR"
    fi
    cp "$TMP_HEX" "$OUTPUT_IMAGE"
fi
echo -e "Result: \e[1;37mSUCC\e[0m"
