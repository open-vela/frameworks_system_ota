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


check_alg(){
  local needle="$1"
  printf '%s\n' "${SUPPORTED_ALG[@]}" | grep -Fxq "$needle"
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

auto_detect_format() {
  local image="$1"
  local fmt="$2"
  if [ -z "$fmt" ] || [ "$fmt" = "auto" ]; then
    if "$HEXINFO" "$image" > /dev/null 2>&1; then
      echo "ihex"
    else
      echo "binary"
    fi
  else
    echo "$fmt"
  fi
}

pre_process() {
  local image="$1"
  local input_format="$2"
  local working_image=""
  if [ "$input_format" = "ihex" ]; then
    "$HEX2BIN" "--pad=00" "$image" "$TMP_BIN" || return 1
    working_image="$TMP_BIN"
  else
    working_image="$image"
  fi
  echo "$working_image"
}

add_hash_footer() {
  local working_image="$1"
  local part_size="$2"
  local part_name="$3"
  local key="$4"
  local alg="$5"
  shift 5
  local opts=( "$@" )

  "$AVBTOOL" add_hash_footer --image "$working_image" \
    --partition_size "$part_size" \
    --partition_name "$part_name" \
    --key "$key" --algorithm "$alg" "${opts[@]}" 2>&1 \
    || fatal "add_hash_footer failed for $working_image"
}

post_process() {
  local original_image="$1"
  local input_format="$2"
  local working_image="$3"
  if [ "$input_format" != "ihex" ]; then
    return 0
  fi
  local start_addr re_addr output_image
  start_addr=$(get_base_addr "$original_image") || return 1
  "$BIN2HEX" --offset "$start_addr" "$working_image" "$TMP_HEX" || return 1
  output_image="${original_image%.*}.hex"
  re_addr=$(get_base_addr "$TMP_HEX") || return 1
  if [ "$start_addr" != "$re_addr" ]; then
    echo "Start address changed after sign: $start_addr -> $re_addr"
    return 2
  fi
  cp "$TMP_HEX" "$output_image"
  return 0
}

sign_image() {
  local image_path="$1"
  local partition_size="$2"
  local partition_name="$3"
  local private_key="$4"
  local algorithm="$5"
  local input_fmt="$6"
  shift 6
  local opts=( "$@" )

  check_e "$image_path"
  check_e "$private_key"

  if ! check_alg "$algorithm" ; then
    fatal "Unsupported algorithmorithm. Supported: ${SUPPORTED_algorithm[@]}"
  fi

  local image_format
  image_format=$(auto_detect_format "$image_path" "$input_fmt")

  local working_image_path
  working_image_path=$(pre_process "$image_path" "$image_format") || fatal "HEX to BIN conversion failed"

  printvar image_path
  printvar image_format
  printvar partition_size "bytes"
  printvar partition_name
  printvar private_key
  printvar algorithm
  [[ ${#opts[@]} -gt 0 ]] && printf "%-16s : %s\n" OPTIONS "${opts[*]}"

  add_hash_footer "$working_image_path" "$partition_size" "$partition_name" "$private_key" "$algorithm" "${opts[@]}" \
    || fatal "Signing failed for $image_path"

  post_process "$image_path" "$image_format" "$working_image_path" \
    || fatal "post_process (BIN->HEX) failed for $image_path"

  return 0
}

[[ $# -lt 2 ]] && help
IMAGE2SIGN=$1
PARTITION_SIZE=$(($2 * $KSIZE)) # KB -> B # TODO : Get from partition table
INPUT_FORMAT="auto"
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

# Determine partition name
if [ -z $DEV_PATH ] ; then
  DEV_PATH="/dev/$(basename $IMAGE2SIGN)"
fi

# Sign
  if ! sign_image "$IMAGE2SIGN" "$PARTITION_SIZE" "$DEV_PATH" "$IN_PRIVKEY" "$ALGORITHM" "$INPUT_FORMAT" "${OPTIONS[@]}"; then
    fatal "signing failed for $image"
  fi
echo -e "Result: \e[1;37mSUCC\e[0m"
