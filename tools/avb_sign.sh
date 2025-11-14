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
  printf "  %-20s   %s\n" "${1}" "${2}"
}
__help(){
  printf "      %-16s" "${1}"
  shift
  printf "   %s\n" "$@"
}

help(){
  echo -e "Usage: $0 [global-options] image1 partition_size1 [item-options1]" \
                    "image2 partition_size2 [item-options2]..."
  _help "<image>" "Full path of imageX to be signed"
  __help "NOTE" "The \"basename\" must BE SAME AS partition name, OR, "
  __help ""     "using additional \"-P\" option."
  _help "<partition_size>" "Partition size in KB"
  echo -e "\nitem-options:"
  _help "[-a algorithm]" "Algorithm of sign, ${DEFAULT_ALG} by default"
  printf "      %-16s   %s" "Supported" && echo "${SUPPORTED_ALG[@]}"
  _help "[-k key_path]" "Path of private key, ${DEFAULT_KEY} by default"
  _help "[-o options]" "Option(s) append to avbtool add_hash_footer"
  __help "--padding_ff" "Padding 0xff for DO_NOT_CARE area"
  _help "[-P verify_path]" "Path of FILE to be verified"
  __help "FILE" "eg. Device point(/dev/ap), ELF(/ota/ota.elf), ..."
  _help "[-I format]" "Input format (ihex or binary), auto-detect by default"
  echo -e "\nglobal-options:"
  _help "[--make_vbmeta vbmeta_path]" "Enable vbmeta mode and output final vbmeta image to vbmeta_path"
  __help "all images will generate a .vbmeta file in the same directory of vbmeta_path"
  __help "and the final vbmeta image will include all these .vbmeta files"
  __help "'--do_not_append_vbmeta_image' is added to each image automatically, so image will not include vbmeta structure"
  _help "[--alg algorithm]" "Global algorithm of sign and for vbmeta, ${DEFAULT_ALG} by default"
  _help "[--key key_path]" "Path of private key to make vbmeta, ${DEFAULT_KEY} by default"
  _help "[--opt options]" "Options append to avbtool make_vbmeta_image, must be used with --make_vbmeta"
  _help "[--format format]" "Output vbmeta format (ihex or binary), must be used with --make_vbmeta"
  _help "[--addr addr]" "Output vbmeta address (hex), 0x0 by default, for example: 0x80000000, must be used with --make_vbmeta"
  _help "[--append_vbmeta image]" "Output vbmeta can be append to a image, must NOT be used with --make_vbmeta"
  _help "[--append_opt opts]" "Append option, must be used with --append_vbmeta"
  echo -e "\n[example1] sign multi image and generate vbmeta for each image:"
  _help "bash avb_sign.sh vela_bl.hex 256 -P /dev/bl -a SHA256_RSA2048 -k key1.pem vela_ap.hex 1776 -P /dev/ap -a SHA512_RSA4096 -k key2.pem"
  echo -e "\n[example2] sign multi image and generate vbmeta image:"
  _help "bash avb_sign.sh --make_vbmeta vbmeta.img --format ihex --addr 0x80000000 --alg SHA256_RSA2048 --key key.pem vela_bl.hex 256 -P /dev/bl vela_ap.hex 1776 -P /dev/ap"
  echo -e "\n[example3] sign multi image and generate vbmeta image, then append this vbmeta to one image:"
  _help "bash avb_sign.sh --append_vbmeta vela_ap.hex --part_size 1776 --block_size 16384 --alg SHA256_RSA2048 --key key.pem vela_bl.hex 256 -P /dev/bl vela_ap.hex 1776 -P /dev/ap"
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

bin2hex() {
  local addr="$1"
  local bin="$2"
  local hex="$3"
  "$BIN2HEX" --offset "$addr" "$bin" "$hex" || return 1
  return 0
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
  bin2hex $start_addr "$working_image" "$TMP_HEX" || return 1
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
  working_image_path=$(pre_process "$image_path" "$image_format") \
    || fatal "HEX to BIN conversion failed"

  printvar image_path "sign"
  printvar image_format
  printvar partition_size "bytes"
  printvar partition_name
  printvar private_key
  printvar algorithm
  [[ ${#opts[@]} -gt 0 ]] && printf "%-16s : %s\n" options "${opts[*]}"

  add_hash_footer "$working_image_path" "$partition_size" "$partition_name" \
    "$private_key" "$algorithm" "${opts[@]}" \
    || fatal "Signing failed for $image_path"

  post_process "$image_path" "$image_format" "$working_image_path" \
    || fatal "post_process (BIN->HEX) failed for $image_path"

  return 0
}

make_vbmeta_image() {
  local out_vbmeta="$1"
  local private_key="$2"
  local algorithm="$3"
  local output_format="$4"
  local output_addr="$5"
  local vbmeta_mode="$6"
  local vbmeta_partition_size="$7"
  local block_size="$8"
  local append_opt_str="$9"
  shift 9
  local opts=( "$@" )
  local temp_vbmeta

  if [ -z "$out_vbmeta" ]; then
    fatal "Output vbmeta image path not specified"
  fi

  if [[ $vbmeta_mode == "make" && -e "$out_vbmeta" ]]; then
    rm -f "$out_vbmeta"
  fi

  if [[ $vbmeta_mode == "append" && "$vbmeta_partition_size" = "0" ]]; then
    fatal "Append options not specified in vbmeta append mode"
  fi

  if ! check_alg "$algorithm" ; then
    fatal "Unsupported algorithmorithm. Supported: ${SUPPORTED_algorithm[@]}"
  fi

  printvar out_vbmeta "make_vbmeta"
  printvar private_key
  printvar algorithm
  printvar output_format
  printvar output_addr
  printvar vbmeta_mode
  printvar vbmeta_partition_size
  printvar block_size "bytes"
  printvar append_opt_str
  [[ ${#opts[@]} -gt 0 ]] && printf "%-16s : %s\n" options "${opts[*]}"

  "$AVBTOOL" make_vbmeta_image --output "$TMP_BIN" \
    --key "$private_key" --algorithm "$algorithm" "${opts[@]}" \
    || fatal "make_vbmeta_image failed for $TMP_BIN"

  if [ $vbmeta_mode == "make" ]; then
    if [ "$output_format" == "ihex" ]; then
      bin2hex "$output_addr" "$TMP_BIN" "$TMP_HEX" || return 1
      cp "$TMP_HEX" "$out_vbmeta"
    else
      cp "$TMP_BIN" "$out_vbmeta"
    fi
  else
    local image_format
    local working_image_path
    temp_vbmeta="$(dirname "$out_vbmeta")/vbmeta_append.img"
    cp "$TMP_BIN" "$temp_vbmeta"
    image_format=$(auto_detect_format "$out_vbmeta" "auto")
    working_image_path=$(pre_process "$out_vbmeta" "$image_format") \
      || fatal "HEX to BIN conversion failed"

    read -ra append_opts <<< "$append_opt_str"
    append_opts=(${append_opts[@]} --vbmeta_image "$temp_vbmeta")
    append_opts=(${append_opts[@]} --image "$working_image_path")
    append_opts=(${append_opts[@]} --partition_size "$vbmeta_partition_size")
    append_opts=(${append_opts[@]} --block_size "$block_size")
    "$AVBTOOL" append_vbmeta_image "${append_opts[@]}" \
      || fatal "append_vbmeta_image failed"

    post_process "$out_vbmeta" "$image_format" "$working_image_path" \
      || fatal "post_process (BIN->HEX) failed for $image_path"
  fi

  return 0
}

IN_PRIVKEY=$DEFAULT_KEY
ALGORITHM=$DEFAULT_ALG
BLOCK_SIZE="4096"
VBMETA_OPTS_STR=""
VBMETA_OPTS=()
VBMETA_FMT="auto"
VBMETA_MODE=""
VBMETA_OUTPUT=""
VBMETA_PATH=""
VBMETA_HEX_ADDR=""
VBMETA_ADDR="0x0"
VBMETA_APPEND_OPT=""
VBMETA_PARTITION_SIZE="0"

parse_arg() {
  if [ $# -lt 2 ]; then
    help
  fi

  #parse global options
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --make_vbmeta)
        shift; [[ $# -lt 1 ]] \
          && fatal "global option --make_vbmeta requires an argument"
        [[ ! -d $(dirname "$1") ]] \
          && fatal "global option --make_vbmeta requires a valid output path argument"
        [[ "$VBMETA_MODE" == "append" ]] \
          && fatal "global option --make_vbmeta cannot be used with --make_vbmeta"
        VBMETA_OUTPUT="$1"
        VBMETA_PATH=$(dirname "$VBMETA_OUTPUT")
        VBMETA_MODE="make"
        shift
        ;;
      --alg)
        shift; [[ $# -lt 1 ]] && fatal "global option --alg requires an argument"
        ALGORITHM="$1"; shift
        ;;
      --key)
        shift; [[ $# -lt 1 ]] && fatal "global option --key requires an argument"
        IN_PRIVKEY="$1"; shift
        ;;
      --opt)
        [[ -z "$VBMETA_MODE" ]] \
          && fatal "global option --opt must be used with --make_vbmeta or --append_vbmeta"
        shift; [[ $# -lt 1 ]] && fatal "global option --opt requires an argument"
        VBMETA_OPTS_STR="$VBMETA_OPTS_STR $1"; shift
        ;;
      --format)
        [[ "$VBMETA_MODE" != "make" ]] \
          && fatal "global option --format must be used with --make_vbmeta"
        shift; [[ $# -lt 1 ]] && fatal "global option --format requires an argument"
        [[ "$1" != "ihex" && "$1" != "binary" && "$1" != "auto" ]] && \
          fatal "Unsupported input format: $1 (--format must be ihex, binary or auto)"
        VBMETA_FMT="$1"; shift
        ;;
      --addr)
        [[ "$VBMETA_MODE" != "make" ]] \
          && fatal "global option --addr must be used with --make_vbmeta"
        shift; [[ $# -lt 1 ]] && fatal "global option --addr requires an argument"
        VBMETA_ADDR="$1"; shift
        ;;
      --append_vbmeta)
        shift; [[ $# -lt 1 ]] \
          && fatal "global option --append_vbmeta requires a valid output path argument"
        [[ "$VBMETA_MODE" == "make" ]] \
          && fatal "global option --append_vbmeta cannot be used with --make_vbmeta"
        [[ ! -e $(dirname "$VBMETA_OUTPUT") ]] \
          && fatal "global option --append_vbmeta requires a valid output path argument"
        VBMETA_OUTPUT="$1"
        VBMETA_PATH=$(dirname "$VBMETA_OUTPUT")
        VBMETA_MODE="append"
        shift
        ;;
      --append_opt)
        [[ "$VBMETA_MODE" != "append" ]] \
          && fatal "global option --append_opt must be used with --append_vbmeta"
        shift; [[ $# -lt 1 ]] \
          && fatal "global option --append_opt requires an argument"
        VBMETA_APPEND_OPT="$1"; shift
        ;;
      --part_size)
        [[ -z "$VBMETA_MODE" ]] \
          && fatal "global option --part_size must be used with --append_vbmeta or --make_vbmeta"
        shift;
        [[ $# -lt 1 ]] && fatal "global option --part_size requires an argument"
        VBMETA_PARTITION_SIZE=$(( "$1" * KSIZE )); shift
        if ! echo "$VBMETA_PARTITION_SIZE" | grep -Eq '^[0-9]+$'; then
          echo "Error: Size must be an integer KB (got invalid value: $VBMETA_PARTITION_SIZE)"
          help
        fi
        ;;
      --block_size)
        shift; [[ $# -lt 1 ]] && fatal "global option --append_opt requires an argument"
        BLOCK_SIZE="$1"; shift
        ;;
      --help|-h)
        help
        ;;
      *)
        break
        ;;
    esac
  done

  while [ $# -gt 0 ]; do
    local image="" psize="" key="" alg="" part="" fmt="" \
      item_opts_str="" item_opts=()
    image="$1"
    shift || { echo "Error: Missing image argument"; help; }

    if [[ -z "$image" ]]; then
      echo "Error: cannot get image (empty value)"
      help
    elif [[ "$image" == -* ]]; then
      echo "Error: Unexpected option '$image' - expected an image path"
      help
    elif [[ $# -lt 1 ]]; then
      echo "Error: Missing size (KB) for image: $image"
      help
    else
      psize=$(( "$1" * KSIZE ))
      shift
      if ! echo "$psize" | grep -Eq '^[0-9]+$'; then
        echo "Error: Size must be an integer KB (got invalid value: $psize)"
        help
      fi
    fi

    while [ $# -gt 0 ]; do
      case "$1" in
        -k)
          shift; [[ $# -lt 1 ]] && fatal "Option -k requires an argument"
          key="$1"; shift
          ;;
        -a)
          shift; [[ $# -lt 1 ]] && fatal "Option -a requires an argument"
          alg="$1"; shift
          ;;
        -o)
          shift; [[ $# -lt 1 ]] && fatal "Option -o requires an argument"
          item_opts_str="$item_opts_str $1"; shift
          ;;
        -P)
          shift; [[ $# -lt 1 ]] && fatal "Option -P requires an argument"
          part="$1"; shift
          ;;
        -I)
          shift; [[ $# -lt 1 ]] && fatal "Option -I requires an argument"
          [[ "$1" != "ihex" && "$1" != "binary" ]] && \
            fatal "Unsupported input format: $1 (must be ihex or binary)"
          fmt="$1"; shift
          ;;
        ?)
          help
          ;;
        -*)
          echo "Unsupported item option: $1"
          help
          ;;
        *)
          break
          ;;
      esac
    done
    [[ -z "$key" ]] && key="$IN_PRIVKEY"
    [[ -z "$alg" ]] && alg="$ALGORITHM"
    [[ -z "$fmt" ]] && fmt="auto"
    [[ -z "$part" ]] && part=$(basename "$image")

    # Convert item options string back to array
    read -ra item_opts <<< "$item_opts_str"

    # Add vbmeta related options if in vbmeta mode
    if [[ ! -z "$VBMETA_MODE" ]]; then
      item_opts=(${item_opts[@]} --do_not_append_vbmeta_image)
      item_opts=(${item_opts[@]} \
        --output_vbmeta_image $VBMETA_PATH/$(basename "$image").vbmeta)
      VBMETA_OPTS=(${VBMETA_OPTS[@]} \
        --include_descriptors_from_image $VBMETA_PATH/$(basename "$image").vbmeta)
    fi

    # Sign the image
    sign_image "$image" "$psize" "$part" "$key" "$alg" "$fmt" "${item_opts[@]}" \
      || fatal "signing failed for $image"
  done
}

parse_arg "$@"

if [[ ! -z "$VBMETA_MODE" ]]; then
  read -ra TEMP_OPTS <<< "$VBMETA_OPTS_STR"
  VBMETA_OPTS=(${VBMETA_OPTS[@]} ${TEMP_OPTS[@]})
  make_vbmeta_image "$VBMETA_OUTPUT" "$IN_PRIVKEY" "$ALGORITHM" \
    "$VBMETA_FMT" "$VBMETA_ADDR" "$VBMETA_MODE" \
    "$VBMETA_PARTITION_SIZE" "$BLOCK_SIZE" "$VBMETA_APPEND_OPT" "${VBMETA_OPTS[@]}" \
      || fatal "make_vbmeta_image failed for $VBMETA_OUTPUT"
fi
