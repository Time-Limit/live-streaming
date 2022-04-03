#!/usr/bin/env bash

function showp() {
  echo $(cd ${1}; pwd)
}

function fp() {
  if [[ "$(basename ${0})"x == "hp"x ]]; then
    prefix="$(hostname):"
  else
    prefix=""
  fi

  while [ ${#} -gt 0 ]; do
    if [ -f ${1} ]; then
      full_path="$(showp $(dirname ${1}))/$(basename ${1})"
      echo "${prefix}${full_path}"
    elif [ -d ${1} ]; then  # in case of `fp /`.
      full_path="$(showp ${1})"
      echo "${prefix}${full_path}"
    else
      >&2 echo "${1} does not exist or seems not be a regular file or directory." 
    fi
    shift
  done
}

function findup() {
  base_path="$(fp ${1})"
  shift 1

  while [[ ${base_path} != / ]];
  do
    find "${base_path}" -maxdepth 1 -mindepth 1 "$@"
    base_path="$(fp "${base_path}/..")"
  done
}

function nearest() {
  res=$((findup . -name "${1}" &) | head -n 1)

  if [[ "x" == "${res}x" ]]; then
    >&2 echo "Cannot find \`${1}' in parent directories!"
    exit 1
  else
    echo ${res}
  fi
}

function do_format() {
  local TARGET_PATH=${1}
  echo "Target Format PATH: ${TARGET_PATH}"

  if [ -f ${TARGET_PATH} ]; then
    ${CLANG_FORMAT} ${TARGET_PATH}
  elif [ -d ${TARGET_PATH} ]; then
    find ${TARGET_PATH} -name \*.h -print -o -name \*.cc -print -o -name \*.cpp -print | xargs ${CLANG_FORMAT}
  else
    >&2 echo "${TARGET_PATH} does not exist or seems not be a regular file or directory."
    >&2 echo "If you're specifying parameters, please kindly note that it's deprecated."
  fi
}

set -eu

DEFAULT_DIR="./"
CLANG_FORMAT="clang-format --verbose -style=file -i"

if [ ${#} == 0 ]; then
  do_format ${DEFAULT_DIR}
else
  while [ ${#} -gt 0 ]; do
    do_format ${1}
    shift
  done
fi

echo "DONE!"

